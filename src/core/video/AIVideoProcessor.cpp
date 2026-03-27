/**
 * @file AIVideoProcessor.cpp
 * @brief AI 推理视频处理器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/AIVideoProcessor.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QElapsedTimer>
#include <QtConcurrent>
#include <QThread>
#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace EnhanceVision {

struct AIVideoProcessor::Impl {
    AIEngine* aiEngine = nullptr;
    ModelInfo modelInfo;
    VideoProcessingConfig config;
    
    std::atomic<bool> isProcessing{false};
    std::atomic<bool> cancelled{false};
    std::atomic<double> progress{0.0};
    std::atomic<qint64> lastProgressEmitMs{0};
    
    QMutex mutex;
};

AIVideoProcessor::AIVideoProcessor(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
}

AIVideoProcessor::~AIVideoProcessor()
{
    cancel();
}

void AIVideoProcessor::setAIEngine(AIEngine* engine)
{
    m_impl->aiEngine = engine;
}

void AIVideoProcessor::setModelInfo(const ModelInfo& model)
{
    m_impl->modelInfo = model;
}

void AIVideoProcessor::setConfig(const VideoProcessingConfig& config)
{
    m_impl->config = config;
}

void AIVideoProcessor::processAsync(const QString& inputPath, const QString& outputPath)
{
    if (m_impl->isProcessing.load()) {
        emit completed(false, QString(), tr("已有视频处理任务正在进行"));
        return;
    }
    
    m_impl->cancelled.store(false);
    m_impl->progress.store(0.0);
    m_impl->lastProgressEmitMs.store(0);
    
    QtConcurrent::run([this, inputPath, outputPath]() {
        processInternal(inputPath, outputPath);
    });
}

void AIVideoProcessor::cancel()
{
    m_impl->cancelled.store(true);
}

bool AIVideoProcessor::isProcessing() const
{
    return m_impl->isProcessing.load();
}

double AIVideoProcessor::progress() const
{
    return m_impl->progress.load();
}

void AIVideoProcessor::processInternal(const QString& inputPath, const QString& outputPath)
{
    QElapsedTimer perfTimer;
    perfTimer.start();
    
    qInfo() << "[AIVideoProcessor] processInternal starting"
            << "input:" << inputPath
            << "output:" << outputPath;
    
    setProcessing(true);
    
    VideoResourceGuard guard;
    guard.setCancelled(false);
    
    auto fail = [&](const QString& err) {
        qWarning() << "[AIVideoProcessor] error:" << err;
        setProcessing(false);
        emitCompleted(false, QString(), err);
    };
    
    if (!guard.initializeInput(inputPath)) {
        fail(guard.lastError());
        return;
    }
    
    auto* decCtx = guard.decoderContext();
    if (!decCtx) {
        fail(tr("解码器初始化失败"));
        return;
    }
    
    const int srcW = decCtx->width;
    const int srcH = decCtx->height;
    const int scale = m_impl->modelInfo.scaleFactor;
    const int outW = (srcW * scale) & ~1;
    const int outH = (srcH * scale) & ~1;
    
    qInfo() << "[AIVideoProcessor] video info:"
            << "srcW:" << srcW << "srcH:" << srcH
            << "scale:" << scale << "outW:" << outW << "outH:" << outH;
    
    QString effectiveOutputPath = outputPath;
    QFileInfo outInfo(outputPath);
    if (outInfo.suffix().toLower() != "mp4") {
        effectiveOutputPath = outInfo.absolutePath() + "/" + outInfo.completeBaseName() + ".mp4";
    }
    
    if (!guard.initializeOutput(effectiveOutputPath, VideoMetadata{})) {
        fail(guard.lastError());
        return;
    }
    
    AVFrame* decFrame = av_frame_alloc();
    AVFrame* encFrame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    AVPacket* outPkt = av_packet_alloc();
    
    bool encoderInitialized = false;
    int64_t frameCount = 0;
    int64_t failedFrames = 0;
    const int64_t totalFrames = guard.totalFrames();
    
    auto cleanupFrames = [&]() {
        if (decFrame) av_frame_free(&decFrame);
        if (encFrame) av_frame_free(&encFrame);
        if (pkt) av_packet_free(&pkt);
        if (outPkt) av_packet_free(&outPkt);
    };
    
    SwsContext* decSwsCtx = nullptr;
    int decSwsW = 0, decSwsH = 0;
    
    while (guard.readFrame(pkt)) {
        if (m_impl->cancelled.load()) {
            av_packet_unref(pkt);
            qInfo() << "[AIVideoProcessor] Cancelled at frame" << frameCount;
            cleanupFrames();
            setProcessing(false);
            emitCompleted(false, QString(), tr("视频处理已取消"));
            return;
        }
        
        if (pkt->stream_index == guard.audioStreamIndex() && encoderInitialized) {
            AVPacket* audioPkt = av_packet_clone(pkt);
            audioPkt->stream_index = guard.outputAudioStreamIndex();
            av_packet_rescale_ts(audioPkt,
                guard.inputFormatContext()->streams[guard.audioStreamIndex()]->time_base,
                guard.outputFormatContext()->streams[guard.outputAudioStreamIndex()]->time_base);
            av_interleaved_write_frame(guard.outputFormatContext(), audioPkt);
            av_packet_free(&audioPkt);
            av_packet_unref(pkt);
            continue;
        }
        
        if (pkt->stream_index != guard.videoStreamIndex()) {
            av_packet_unref(pkt);
            continue;
        }
        
        avcodec_send_packet(decCtx, pkt);
        av_packet_unref(pkt);
        
        while (avcodec_receive_frame(decCtx, decFrame) == 0) {
            if (m_impl->cancelled.load()) {
                qInfo() << "[AIVideoProcessor] Cancelled at frame receive" << frameCount;
                cleanupFrames();
                setProcessing(false);
                emitCompleted(false, QString(), tr("视频处理已取消"));
                return;
            }
            
            const AVPixelFormat frameFmt = static_cast<AVPixelFormat>(decFrame->format);
            if (!decSwsCtx || decSwsW != decFrame->width ||
                decSwsH != decFrame->height) {
                if (decSwsCtx) sws_freeContext(decSwsCtx);
                decSwsCtx = sws_getContext(
                    decFrame->width, decFrame->height, frameFmt,
                    decFrame->width, decFrame->height, AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                decSwsW = decFrame->width;
                decSwsH = decFrame->height;
            }
            
            const int frameW = decFrame->width;
            const int frameH = decFrame->height;
            const int rgb24LineSize = frameW * 3;
            
            std::vector<uint8_t> rgb24Buffer(rgb24LineSize * frameH);
            uint8_t* dst[4] = { rgb24Buffer.data(), nullptr, nullptr, nullptr };
            int dstStride[4] = { rgb24LineSize, 0, 0, 0 };
            
            sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
                      0, frameH, dst, dstStride);
            
            QImage frameImg(frameW, frameH, QImage::Format_RGB888);
            for (int y = 0; y < frameH; ++y) {
                const uint8_t* srcLine = rgb24Buffer.data() + y * rgb24LineSize;
                uchar* dstLine = frameImg.scanLine(y);
                std::memcpy(dstLine, srcLine, rgb24LineSize);
            }
            
            QImage resultImg = processFrame(frameImg);
            if (resultImg.isNull()) {
                qWarning() << "[AIVideoProcessor] frame" << frameCount << "AI inference failed";
                resultImg = frameImg;
                failedFrames++;
            }
            
            if (!encoderInitialized) {
                encoderInitialized = true;
                
                const int finalOutW = resultImg.width() & ~1;
                const int finalOutH = resultImg.height() & ~1;
                
                if (!guard.initializeEncoder(VideoMetadata{}, finalOutW, finalOutH)) {
                    cleanupFrames();
                    fail(guard.lastError());
                    return;
                }
                
                encFrame->format = AV_PIX_FMT_YUV420P;
                encFrame->width = finalOutW;
                encFrame->height = finalOutH;
                av_frame_get_buffer(encFrame, 32);
            }
            
            if (resultImg.format() != QImage::Format_RGB888) {
                resultImg = resultImg.convertToFormat(QImage::Format_RGB888);
            }
            
            uint8_t* src[4] = { resultImg.bits(), nullptr, nullptr, nullptr };
            int srcStride[4] = { static_cast<int>(resultImg.bytesPerLine()), 0, 0, 0 };
            sws_scale(guard.encoderSwsContext(), src, srcStride, 0, resultImg.height(),
                      encFrame->data, encFrame->linesize);
            
            encFrame->pts = decFrame->pts;
            
            int sendRet = avcodec_send_frame(guard.encoderContext(), encFrame);
            if (sendRet >= 0) {
                while (avcodec_receive_packet(guard.encoderContext(), outPkt) == 0) {
                    av_packet_rescale_ts(outPkt, guard.encoderContext()->time_base,
                        guard.outputFormatContext()->streams[guard.outputVideoStreamIndex()]->time_base);
                    outPkt->stream_index = guard.outputVideoStreamIndex();
                    av_interleaved_write_frame(guard.outputFormatContext(), outPkt);
                    av_packet_unref(outPkt);
                }
            }
            
            frameCount++;
            
            if (totalFrames > 0) {
                const double prog = static_cast<double>(frameCount) / totalFrames;
                updateProgress(prog);
            }
        }
    }
    
    if (encoderInitialized && guard.encoderContext()) {
        avcodec_send_frame(guard.encoderContext(), nullptr);
        while (avcodec_receive_packet(guard.encoderContext(), outPkt) == 0) {
            av_packet_rescale_ts(outPkt, guard.encoderContext()->time_base,
                guard.outputFormatContext()->streams[guard.outputVideoStreamIndex()]->time_base);
            outPkt->stream_index = guard.outputVideoStreamIndex();
            av_interleaved_write_frame(guard.outputFormatContext(), outPkt);
            av_packet_unref(outPkt);
        }
    }
    
    // 写入文件尾部（moov atom），这对于MP4文件至关重要
    if (!guard.writeTrailer()) {
        qWarning() << "[AIVideoProcessor] Failed to write trailer";
    }
    
    if (decSwsCtx) {
        sws_freeContext(decSwsCtx);
    }
    
    cleanupFrames();
    
    const qint64 totalCostMs = perfTimer.elapsed();
    qInfo() << "[AIVideoProcessor] finished"
            << "frames:" << frameCount
            << "failedFrames:" << failedFrames
            << "totalCost:" << totalCostMs << "ms"
            << "avgPerFrame:" << (frameCount > 0 ? totalCostMs / frameCount : 0) << "ms"
            << "output:" << effectiveOutputPath;
    
    setProcessing(false);
    
    if (frameCount > 0) {
        QFileInfo outFileInfo(effectiveOutputPath);
        if (outFileInfo.exists() && outFileInfo.size() > 0) {
            emitCompleted(true, effectiveOutputPath, QString());
        } else {
            emitCompleted(false, QString(), tr("视频处理完成但输出文件无效"));
        }
    } else {
        emitCompleted(false, QString(), tr("视频处理失败：未能成功处理任何帧"));
    }
}

QImage AIVideoProcessor::processFrame(const QImage& input)
{
    if (!m_impl->aiEngine) {
        qWarning() << "[AIVideoProcessor] no AI engine set";
        return QImage();
    }
    
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIVideoProcessor] invalid input image";
        return QImage();
    }
    
    if (m_impl->cancelled.load()) {
        return QImage();
    }
    
    QImage safeInput = input.copy();
    if (safeInput.isNull()) {
        qWarning() << "[AIVideoProcessor] failed to create safe input copy";
        return QImage();
    }
    
    QImage result = m_impl->aiEngine->process(safeInput);
    
    if (result.isNull()) {
        qWarning() << "[AIVideoProcessor] AI process returned null image";
    }
    
    return result;
}

void AIVideoProcessor::updateProgress(double progress)
{
    constexpr double kProgressEmitDelta = 0.01;
    constexpr qint64 kProgressEmitIntervalMs = 100;
    
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double current = m_impl->progress.load();
    
    if (std::abs(progress - current) >= kProgressEmitDelta ||
        (now - m_impl->lastProgressEmitMs.load()) >= kProgressEmitIntervalMs ||
        progress >= 1.0) {
        m_impl->progress.store(progress);
        m_impl->lastProgressEmitMs.store(now);
        emit progressChanged(progress);
    }
}

void AIVideoProcessor::setProcessing(bool processing)
{
    bool wasProcessing = m_impl->isProcessing.exchange(processing);
    if (wasProcessing != processing) {
        emit processingChanged(processing);
    }
}

void AIVideoProcessor::emitCompleted(bool success, const QString& resultPath, const QString& error)
{
    emit completed(success, resultPath, error);
}

} // namespace EnhanceVision
