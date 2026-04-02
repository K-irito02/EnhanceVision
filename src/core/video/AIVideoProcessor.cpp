/**
 * @file AIVideoProcessor.cpp
 * @brief AI 推理视频处理器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/AIVideoProcessor.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/core/video/VideoCompatibilityAnalyzer.h"
#include "EnhanceVision/core/video/VideoSizeAdapter.h"
#include "EnhanceVision/core/video/VideoFormatConverter.h"
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
    , m_progressReporter(std::make_unique<ProgressReporter>(this))
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
    // 如果有之前的处理任务，等待其完成
    if (m_processingFuture.isRunning()) {
        qInfo() << "[AIVideoProcessor][DEBUG] Waiting for previous task to finish";
        m_processingFuture.waitForFinished();
    }
    
    if (m_impl->isProcessing.load()) {
        emit completed(false, QString(), tr("已有视频处理任务正在进行"));
        return;
    }
    
    m_impl->cancelled.store(false);
    m_impl->progress.store(0.0);
    m_impl->lastProgressEmitMs.store(0);
    
    m_processingFuture = QtConcurrent::run([this, inputPath, outputPath]() {
        processInternal(inputPath, outputPath);
    });
}

void AIVideoProcessor::cancel()
{
    qInfo() << "[AIVideoProcessor][DEBUG] Cancel requested";
    m_impl->cancelled.store(true);
    
    // 通知引擎取消（不使用 mutex，避免死锁）
    // aiEngine 指针在处理期间不会改变，所以直接访问是安全的
    AIEngine* engine = m_impl->aiEngine;
    if (engine) {
        engine->cancelProcess();
    }
    
    // 等待取消生效
    QThread::msleep(50);
}

void AIVideoProcessor::waitForFinished(int timeoutMs)
{
    if (m_processingFuture.isRunning()) {
        qInfo() << "[AIVideoProcessor][DEBUG] Waiting for processing to finish, timeout:" << timeoutMs << "ms";
        
        // 使用循环等待，避免无限阻塞
        QElapsedTimer timer;
        timer.start();
        
        while (m_processingFuture.isRunning() && timer.elapsed() < timeoutMs) {
            QThread::msleep(10);
        }
        
        if (m_processingFuture.isRunning()) {
            qWarning() << "[AIVideoProcessor][DEBUG] Wait timeout, processing still running";
        } else {
            qInfo() << "[AIVideoProcessor][DEBUG] Processing finished after" << timer.elapsed() << "ms";
        }
    }
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
    
    m_impl->cancelled.store(false);
    m_impl->progress.store(0.0);
    m_impl->lastProgressEmitMs.store(0);
    setProcessing(true);
    
    qInfo() << "[AIVideoProcessor][DEBUG] processInternal started:"
            << "input:" << inputPath
            << "output:" << outputPath
            << "threadId:" << QThread::currentThreadId();
    
    if (m_progressReporter) {
        m_progressReporter->reset();
    }
    
    // 检查引擎状态
    {
        QMutexLocker locker(&m_impl->mutex);
        if (!m_impl->aiEngine) {
            qWarning() << "[AIVideoProcessor][DEBUG] AI engine is null at start";
            setProcessing(false);
            emitCompleted(false, QString(), tr("AI引擎未初始化"));
            return;
        }
        
        // 【关键修复】确保模型已加载
        if (m_impl->aiEngine->currentModelId().isEmpty()) {
            qWarning() << "[AIVideoProcessor][DEBUG] AI model not loaded";
            setProcessing(false);
            emitCompleted(false, QString(), tr("AI模型未加载"));
            return;
        }
        
        qInfo() << "[AIVideoProcessor][DEBUG] AI engine ready:"
                << "modelId:" << m_impl->aiEngine->currentModelId()
                << "useGpu:" << m_impl->aiEngine->useGpu()
                << "gpuAvailable:" << m_impl->aiEngine->gpuAvailable();
        
        // 【关键修复】阻塞等待模型同步完成，确保GPU资源完全就绪
        // 防止模型加载后GPU资源未完全初始化导致的堆损坏
        qInfo() << "[AIVideoProcessor][DEBUG] Waiting for model sync complete...";
        fflush(stdout);
        
        if (!m_impl->aiEngine->waitForModelSyncComplete(10000)) {
            qWarning() << "[AIVideoProcessor][DEBUG] Model sync timeout, aborting";
            setProcessing(false);
            emitCompleted(false, QString(), tr("模型同步超时，GPU资源未就绪"));
            return;
        }
        
        qInfo() << "[AIVideoProcessor][DEBUG] Model sync complete, proceeding with processing";
        fflush(stdout);
    }
    
    VideoResourceGuard guard;
    guard.setCancelled(false);
    
    auto fail = [&](const QString& err) {
        qWarning() << "[AIVideoProcessor][ERROR] Processing failed:"
                   << "error:" << err
                   << "elapsed:" << perfTimer.elapsed() << "ms"
                   << "cancelled:" << m_impl->cancelled.load();
        setProcessing(false);
        emitCompleted(false, QString(), err);
    };
    
    // 早期取消检测
    if (m_impl->cancelled.load()) {
        qInfo() << "[AIVideoProcessor][DEBUG] Cancelled before input initialization";
        setProcessing(false);
        emitCompleted(false, QString(), tr("视频处理已取消"));
        return;
    }
    
    if (!guard.initializeInput(inputPath)) {
        fail(guard.lastError());
        return;
    }
    
    // 调试信息：输出视频基本信息
    auto* inputFmtCtx = guard.inputFormatContext();
    if (inputFmtCtx) {
        qInfo() << "[AIVideoProcessor][DEBUG] Input video info:"
                << "path:" << inputPath
                << "streams:" << inputFmtCtx->nb_streams
                << "duration:" << inputFmtCtx->duration / AV_TIME_BASE << "s";
    }
    
    VideoCompatibilityAnalyzer compatibilityAnalyzer;
    compatibilityAnalyzer.setModelInfo(m_impl->modelInfo);
    VideoCompatibilityReport compatibilityReport = compatibilityAnalyzer.analyze(inputPath);
    
    if (!compatibilityReport.canProcess) {
        QString error = compatibilityReport.errors.join("\n");
        fail(error.isEmpty() ? tr("视频不兼容，无法处理") : error);
        return;
    }
    
    if (!compatibilityReport.warnings.isEmpty()) {
        qWarning() << "[AIVideoProcessor] Warnings:" << compatibilityReport.warnings;
    }
    
    VideoSizeAdapter sizeAdapter;
    sizeAdapter.setModelInfo(m_impl->modelInfo);
    SizeAdaptationResult sizeAdaptation = sizeAdapter.analyze(compatibilityReport.originalSize);
    
    VideoFormatConverter formatConverter;
    
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
    
    // 调试信息：输出解码器和尺寸信息
    qInfo() << "[AIVideoProcessor][DEBUG] Decoder info:"
            << "srcSize:" << srcW << "x" << srcH
            << "scale:" << scale
            << "outSize:" << outW << "x" << outH
            << "pixFmt:" << decCtx->pix_fmt
            << "codecId:" << decCtx->codec_id
            << "modelId:" << m_impl->modelInfo.id
            << "tileSize:" << m_impl->modelInfo.tileSize;
    
    QString effectiveOutputPath = outputPath;
    QFileInfo outInfo(outputPath);
    if (outInfo.suffix().toLower() != "mp4") {
        effectiveOutputPath = outInfo.absolutePath() + "/" + outInfo.completeBaseName() + ".mp4";
    }
    
    if (!guard.initializeOutput(effectiveOutputPath, VideoMetadata{})) {
        fail(guard.lastError());
        return;
    }
    
    qInfo() << "[AIVideoProcessor][DEBUG] Output initialized:"
            << "outputPath:" << effectiveOutputPath;
    
    AVFrame* decFrame = av_frame_alloc();
    AVFrame* encFrame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    AVPacket* outPkt = av_packet_alloc();
    
    if (!decFrame || !encFrame || !pkt || !outPkt) {
        qWarning() << "[AIVideoProcessor][DEBUG] Failed to allocate frames/packets:"
                   << "decFrame:" << (decFrame != nullptr)
                   << "encFrame:" << (encFrame != nullptr)
                   << "pkt:" << (pkt != nullptr)
                   << "outPkt:" << (outPkt != nullptr);
    }
    
    bool encoderInitialized = false;
    int64_t frameCount = 0;
    int64_t failedFrames = 0;
    const int64_t totalFrames = guard.totalFrames();
    
    qInfo() << "[AIVideoProcessor][DEBUG] Starting frame loop:"
            << "totalFrames:" << totalFrames;
    
    auto cleanupFrames = [&]() {
        if (decFrame) av_frame_free(&decFrame);
        if (encFrame) av_frame_free(&encFrame);
        if (pkt) av_packet_free(&pkt);
        if (outPkt) av_packet_free(&outPkt);
    };
    
    SwsContext* decSwsCtx = nullptr;
    int decSwsW = 0, decSwsH = 0;
    
    qInfo() << "[AIVideoProcessor][DEBUG] Entering frame read loop"
            << "srcSize:" << srcW << "x" << srcH
            << "outSize:" << outW << "x" << outH;
    fflush(stdout);  // 强制刷新日志
    
    // 帧处理统计
    int64_t skippedFrames = 0;
    int64_t processedOkFrames = 0;
    int consecutiveFrameFailures = 0;  // 局部变量
    int lastEncoderWidth = 0;
    int lastEncoderHeight = 0;
    constexpr int64_t kMaxConsecutiveFailures = 10;  // 连续失败阈值
    constexpr int64_t kMaxTotalFailures = 100;       // 总失败阈值
    
    qInfo() << "[AIVideoProcessor][DEBUG] Starting while loop";
    fflush(stdout);  // 强制刷新日志
    while (guard.readFrame(pkt)) {
        qInfo() << "[AIVideoProcessor][DEBUG] Read frame packet, stream_index:" << pkt->stream_index;
        fflush(stdout);  // 强制刷新日志
        
        // 检查取消标志
        if (m_impl->cancelled.load()) {
            qInfo() << "[AIVideoProcessor][DEBUG] Cancelled during frame loop"
                    << "frameCount:" << frameCount
                    << "processedOk:" << processedOkFrames;
            av_packet_unref(pkt);
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
        
        int sendRet = avcodec_send_packet(decCtx, pkt);
        av_packet_unref(pkt);
        
        if (sendRet < 0) {
            continue;
        }
        
        while (avcodec_receive_frame(decCtx, decFrame) == 0) {
            qInfo() << "[AIVideoProcessor][DEBUG] Received decoded frame:" << frameCount
                    << "size:" << decFrame->width << "x" << decFrame->height;
            fflush(stdout);
            
            if (m_impl->cancelled.load()) {
                cleanupFrames();
                setProcessing(false);
                emitCompleted(false, QString(), tr("视频处理已取消"));
                return;
            }
            
            const AVPixelFormat frameFmt = static_cast<AVPixelFormat>(decFrame->format);
            
            if (!decSwsCtx || decSwsW != decFrame->width ||
                decSwsH != decFrame->height) {
                if (decSwsCtx) {
                    sws_freeContext(decSwsCtx);
                }
                decSwsCtx = sws_getContext(
                    decFrame->width, decFrame->height, frameFmt,
                    decFrame->width, decFrame->height, AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!decSwsCtx) {
                    qWarning() << "[AIVideoProcessor][DEBUG] Failed to create SwsContext";
                    av_frame_unref(decFrame);
                    continue;
                }
                decSwsW = decFrame->width;
                decSwsH = decFrame->height;
                qInfo() << "[AIVideoProcessor][DEBUG] SwsContext created";
                fflush(stdout);
            }
            
            const int frameW = decFrame->width;
            const int frameH = decFrame->height;
            const int rgb24LineSize = frameW * 3;
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "dimensions:" << frameW << "x" << frameH;
            fflush(stdout);
            
            std::vector<uint8_t> rgb24Buffer(rgb24LineSize * frameH);
            uint8_t* dst[4] = { rgb24Buffer.data(), nullptr, nullptr, nullptr };
            int dstStride[4] = { rgb24LineSize, 0, 0, 0 };
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "calling sws_scale";
            fflush(stdout);
            
            sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
                      0, frameH, dst, dstStride);
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "sws_scale done";
            fflush(stdout);
            
            // 使用 QImage 构造函数直接引用数据，然后深拷贝
            QImage tempImg(rgb24Buffer.data(), frameW, frameH, rgb24LineSize, QImage::Format_RGB888);
            if (tempImg.isNull()) {
                av_frame_unref(decFrame);
                continue;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "tempImg created"
                    << "size:" << tempImg.width() << "x" << tempImg.height()
                    << "format:" << tempImg.format()
                    << "isNull:" << tempImg.isNull()
                    << "bytesPerLine:" << tempImg.bytesPerLine()
                    << "sizeInBytes:" << tempImg.sizeInBytes()
                    << "bits:" << (tempImg.bits() != nullptr ? "valid" : "NULL");
            fflush(stdout);
            
            // 【调试】验证 tempImg 数据有效性
            if (tempImg.isNull() || tempImg.bits() == nullptr || tempImg.sizeInBytes() == 0) {
                qWarning() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "tempImg is invalid, skipping";
                fflush(stdout);
                av_frame_unref(decFrame);
                continue;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "calling tempImg.copy()";
            fflush(stdout);
            
            // 深拷贝，因为 rgb24Buffer 是局部变量
            QImage frameImg = tempImg.copy();
            if (frameImg.isNull()) {
                av_frame_unref(decFrame);
                continue;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "frameImg copied";
            fflush(stdout);
            
            QImage processedFrame = frameImg;
            if (compatibilityReport.formatCompatibility == FormatCompatibility::NeedsToneMapping) {
                QImage toneMapped = formatConverter.applyToneMapping(frameImg);
                if (!toneMapped.isNull()) {
                    processedFrame = toneMapped;
                } else {
                    qWarning() << "[AIVideoProcessor] frame" << frameCount << "tone mapping failed, using original";
                }
            } else if (compatibilityReport.formatCompatibility == FormatCompatibility::NeedsColorConvert) {
                QImage converted = formatConverter.convertPixelFormat(frameImg, frameFmt, AV_PIX_FMT_RGB24);
                if (!converted.isNull()) {
                    processedFrame = converted;
                } else {
                    qWarning() << "[AIVideoProcessor] frame" << frameCount << "color conversion failed, using original";
                }
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "calling sizeAdapter.adaptFrame";
            fflush(stdout);
            
            QImage adaptedFrame = sizeAdapter.adaptFrame(processedFrame, sizeAdaptation);
            if (adaptedFrame.isNull()) {
                qWarning() << "[AIVideoProcessor] frame" << frameCount << "size adaptation failed, using processed";
                adaptedFrame = processedFrame;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "adaptedFrame ready:"
                    << adaptedFrame.width() << "x" << adaptedFrame.height()
                    << "format:" << adaptedFrame.format();
            fflush(stdout);
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "calling processFrame (AI inference)";
            fflush(stdout);
            
            QImage resultImg = processFrame(adaptedFrame);
            if (resultImg.isNull()) {
                consecutiveFrameFailures++;
                
                qWarning() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "AI inference failed"
                           << "inputSize:" << adaptedFrame.width() << "x" << adaptedFrame.height()
                           << "consecutiveFailures:" << consecutiveFrameFailures
                           << "totalFailed:" << failedFrames;
                failedFrames++;
                
                // 检查是否应该中止处理
                if (consecutiveFrameFailures >= kMaxConsecutiveFailures) {
                    qWarning() << "[AIVideoProcessor][ERROR] Too many consecutive failures ("
                               << consecutiveFrameFailures << "), aborting";
                    av_frame_unref(decFrame);
                    cleanupFrames();
                    if (decSwsCtx) sws_freeContext(decSwsCtx);
                    fail(tr("AI推理连续失败，可能是模型不兼容或显存不足"));
                    return;
                }
                
                if (failedFrames >= kMaxTotalFailures) {
                    qWarning() << "[AIVideoProcessor][ERROR] Total failures exceeded ("
                               << failedFrames << "), aborting";
                    av_frame_unref(decFrame);
                    break;
                }
                
                // 每 5 次失败尝试清理 GPU 内存
                if (failedFrames % 5 == 0) {
                    QMutexLocker locker(&m_impl->mutex);
                    if (m_impl->aiEngine) {
                        qInfo() << "[AIVideoProcessor][DEBUG] Cleaning GPU memory after"
                                << failedFrames << "failures";
                        m_impl->aiEngine->cleanupGpuMemory();
                    }
                }
                
                // 使用原始帧作为备选（保持视频连续性）
                resultImg = adaptedFrame;
            } else {
                // 成功处理，重置连续失败计数
                consecutiveFrameFailures = 0;
                processedOkFrames++;
            }
            
            QImage restoredImg = sizeAdapter.restoreFrame(resultImg, sizeAdaptation);
            if (restoredImg.isNull()) {
                qWarning() << "[AIVideoProcessor] frame" << frameCount << "size restore failed, using result";
                restoredImg = resultImg;
            }
            
            if (restoredImg.isNull()) {
                qWarning() << "[AIVideoProcessor] frame" << frameCount << "final image is null, skipping";
                av_frame_unref(decFrame);
                continue;
            }
            
            resultImg = restoredImg;
            
            const int finalOutW = resultImg.width() & ~1;
            const int finalOutH = resultImg.height() & ~1;
            
            // 尺寸验证：确保输出尺寸合理
            if (finalOutW <= 0 || finalOutH <= 0 || finalOutW > 16384 || finalOutH > 16384) {
                qWarning() << "[AIVideoProcessor][DEBUG] Invalid output dimensions:"
                           << finalOutW << "x" << finalOutH
                           << "at frame" << frameCount;
                av_frame_unref(decFrame);
                continue;
            }
            
            if (!encoderInitialized) {
                encoderInitialized = true;
                lastEncoderWidth = finalOutW;
                lastEncoderHeight = finalOutH;
                
                qInfo() << "[AIVideoProcessor][DEBUG] Initializing encoder:"
                        << "size:" << finalOutW << "x" << finalOutH;
                
                if (!guard.initializeEncoder(VideoMetadata{}, finalOutW, finalOutH)) {
                    cleanupFrames();
                    fail(guard.lastError());
                    return;
                }
                
                encFrame->format = AV_PIX_FMT_YUV420P;
                encFrame->width = finalOutW;
                encFrame->height = finalOutH;
                if (av_frame_get_buffer(encFrame, 32) < 0) {
                    qWarning() << "[AIVideoProcessor][ERROR] Failed to allocate encoder frame buffer";
                    cleanupFrames();
                    fail(tr("无法分配编码器帧缓冲区"));
                    return;
                }
            }
            
            // 检测尺寸变化：如果输出尺寸与编码器尺寸不匹配，需要缩放
            if (finalOutW != lastEncoderWidth || finalOutH != lastEncoderHeight) {
                qInfo() << "[AIVideoProcessor][DEBUG] Frame size changed:"
                        << "from" << lastEncoderWidth << "x" << lastEncoderHeight
                        << "to" << finalOutW << "x" << finalOutH
                        << "at frame" << frameCount;
                
                // 缩放到编码器期望的尺寸
                resultImg = resultImg.scaled(lastEncoderWidth, lastEncoderHeight,
                                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            
            if (resultImg.format() != QImage::Format_RGB888) {
                resultImg = resultImg.convertToFormat(QImage::Format_RGB888);
            }
            
            // 验证 SwsContext 和编码器状态
            SwsContext* encSwsCtx = guard.encoderSwsContext();
            if (!encSwsCtx) {
                qWarning() << "[AIVideoProcessor][ERROR] Encoder SwsContext is null at frame" << frameCount;
                av_frame_unref(decFrame);
                continue;
            }
            
            // 确保 resultImg 尺寸与编码器一致
            if (resultImg.width() != encFrame->width || resultImg.height() != encFrame->height) {
                qWarning() << "[AIVideoProcessor][DEBUG] Scaling result to encoder size:"
                           << "from" << resultImg.width() << "x" << resultImg.height()
                           << "to" << encFrame->width << "x" << encFrame->height;
                resultImg = resultImg.scaled(encFrame->width, encFrame->height,
                                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            
            uint8_t* src[4] = { resultImg.bits(), nullptr, nullptr, nullptr };
            int srcStride[4] = { static_cast<int>(resultImg.bytesPerLine()), 0, 0, 0 };
            sws_scale(encSwsCtx, src, srcStride, 0, resultImg.height(),
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
                const double prog = 0.05 + 0.90 * static_cast<double>(frameCount) / totalFrames;
                updateProgress(prog);
            }
        }
    }
    
    updateProgress(0.96);
    
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
    
    setProcessing(false);
    
    qInfo() << "[AIVideoProcessor][DEBUG] Processing completed:"
            << "totalFrames:" << frameCount
            << "processedOk:" << processedOkFrames
            << "failed:" << failedFrames
            << "skipped:" << skippedFrames
            << "elapsed:" << perfTimer.elapsed() << "ms";
    
    if (frameCount > 0) {
        QFileInfo outFileInfo(effectiveOutputPath);
        if (outFileInfo.exists() && outFileInfo.size() > 0) {
            qInfo() << "[AIVideoProcessor][DEBUG] Output file valid:"
                    << "path:" << effectiveOutputPath
                    << "size:" << outFileInfo.size();
            emitCompleted(true, effectiveOutputPath, QString());
        } else {
            qWarning() << "[AIVideoProcessor][ERROR] Output file invalid:"
                       << "path:" << effectiveOutputPath
                       << "exists:" << outFileInfo.exists()
                       << "size:" << outFileInfo.size();
            emitCompleted(false, QString(), tr("视频处理完成但输出文件无效"));
        }
    } else {
        qWarning() << "[AIVideoProcessor][ERROR] No frames processed successfully";
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
    
    // 创建输入的深拷贝，确保线程安全
    QImage safeInput = input.copy();
    if (safeInput.isNull()) {
        qWarning() << "[AIVideoProcessor] failed to create safe input copy";
        return QImage();
    }
    
    // 确保输入格式为 RGB888，避免格式不兼容导致推理失败
    if (safeInput.format() != QImage::Format_RGB888) {
        safeInput = safeInput.convertToFormat(QImage::Format_RGB888);
        if (safeInput.isNull()) {
            qWarning() << "[AIVideoProcessor] failed to convert input to RGB888";
            return QImage();
        }
    }
    
    QImage result;
    
    // 使用 try-catch 包装推理调用，防止异常导致崩溃
    // 注意：NCNN 通常不抛出 C++ 异常，但某些 GPU 驱动/系统库可能会
    try {
        result = m_impl->aiEngine->process(safeInput);
    } catch (const std::exception& e) {
        qWarning() << "[AIVideoProcessor] AI process exception:" << e.what();
        return QImage();
    } catch (...) {
        qWarning() << "[AIVideoProcessor] AI process unknown exception";
        return QImage();
    }
    
    if (result.isNull()) {
        qWarning() << "[AIVideoProcessor] AI process returned null image";
    }
    
    return result;
}

void AIVideoProcessor::updateProgress(double progress, const QString& stage)
{
    if (m_progressReporter) {
        m_progressReporter->setProgress(progress, stage);
        m_impl->progress.store(progress);
        emit progressChanged(progress);
        return;
    }
    
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

double AIVideoProcessor::estimateProgressByTime(int frameCount, qint64 elapsedMs)
{
    if (frameCount <= 0 || elapsedMs <= 0) {
        return 0.0;
    }
    
    double avgFrameTimeMs = static_cast<double>(elapsedMs) / frameCount;
    const int estimatedTotalFrames = 60 * 30;
    double progress = static_cast<double>(frameCount) / estimatedTotalFrames;
    return std::min(0.95, progress);
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
