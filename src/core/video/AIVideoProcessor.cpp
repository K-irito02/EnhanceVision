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
#include <vector>

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
    std::atomic<bool> processingFrame{false};  // 当前是否正在处理单帧
    std::atomic<double> progress{0.0};
    std::atomic<qint64> lastProgressEmitMs{0};
    
    // 帧处理完成等待
    std::mutex frameCompleteMutex;
    std::condition_variable frameCompleteCv;
    
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
        emit completed(false, QString(), tr("Video processing task already in progress"));
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
    if (!m_impl->cancelled.load()) {
        qInfo() << "[AIVideoProcessor] Cancel requested, waiting for current frame to complete...";
        m_impl->cancelled.store(true);
        
        // 等待当前帧处理完成（带超时），确保不会在帧处理中途中断导致资源泄漏
        if (m_impl->processingFrame.load()) {
            constexpr int kFrameWaitTimeoutMs = 5000;  // 最多等待5秒
            std::unique_lock<std::mutex> lock(m_impl->frameCompleteMutex);
            bool completed = m_impl->frameCompleteCv.wait_for(
                lock,
                std::chrono::milliseconds(kFrameWaitTimeoutMs),
                [this]() { return !m_impl->processingFrame.load(); }
            );
            
            if (completed) {
                qInfo() << "[AIVideoProcessor] Current frame completed before cancel";
            } else {
                qWarning() << "[AIVideoProcessor] Timeout waiting for current frame during cancel";
            }
        }
        
        emit cancelled();
    }
}

void AIVideoProcessor::waitForFinished(int timeoutMs)
{
    if (m_processingFuture.isRunning()) {
        qInfo() << "[AIVideoProcessor] Waiting for processing to finish, timeout:" << timeoutMs << "ms";
        
        // 使用事件循环等待，避免阻塞
        QElapsedTimer timer;
        timer.start();
        
        while (m_processingFuture.isRunning() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            QThread::msleep(10);
        }
        
        if (m_processingFuture.isRunning()) {
            qWarning() << "[AIVideoProcessor] Wait timeout, processing still running";
        } else {
            qInfo() << "[AIVideoProcessor] Processing finished after" << timer.elapsed() << "ms";
        }
    }
}

bool AIVideoProcessor::waitForCurrentFrame(int timeoutMs)
{
    if (!m_impl->processingFrame.load()) {
        return true;  // 没有正在处理的帧
    }
    
    qInfo() << "[AIVideoProcessor] Waiting for current frame to complete, timeout:" << timeoutMs << "ms";
    
    std::unique_lock<std::mutex> lock(m_impl->frameCompleteMutex);
    bool completed = m_impl->frameCompleteCv.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [this]() { return !m_impl->processingFrame.load(); }
    );
    
    if (completed) {
        qInfo() << "[AIVideoProcessor] Current frame completed";
    } else {
        qWarning() << "[AIVideoProcessor] Timeout waiting for current frame";
    }
    
    return completed;
}

bool AIVideoProcessor::isProcessingFrame() const
{
    return m_impl->processingFrame.load();
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
            emitCompleted(false, QString(), tr("AI engine not initialized"));
            return;
        }
        
        // 【关键修复】确保模型已加载
        if (m_impl->aiEngine->currentModelId().isEmpty()) {
            qWarning() << "[AIVideoProcessor][DEBUG] AI model not loaded";
            setProcessing(false);
            emitCompleted(false, QString(), tr("AI model not loaded"));
            return;
        }
        
        qInfo() << "[AIVideoProcessor][DEBUG] AI engine ready:"
                << "modelId:" << m_impl->aiEngine->currentModelId();
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
        emitCompleted(false, QString(), tr("Video processing cancelled"));
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
        fail(error.isEmpty() ? tr("Video incompatible, cannot process") : error);
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
        fail(tr("Decoder initialization failed"));
        return;
    }
    
    const int srcW = decCtx->width;
    const int srcH = decCtx->height;
    const int scale = m_impl->modelInfo.scaleFactor;
    const int outW = (srcW * scale) & ~1;
    const int outH = (srcH * scale) & ~1;
    
    // 【安全验证】验证输入尺寸有效性
    if (srcW <= 0 || srcH <= 0 || srcW > 16384 || srcH > 16384) {
        fail(tr("Invalid video size: %1x%2").arg(srcW).arg(srcH));
        return;
    }
    
    // 【安全验证】验证输出尺寸有效性
    if (outW <= 0 || outH <= 0 || outW > 16384 || outH > 16384) {
        fail(tr("Invalid output size: %1x%2").arg(outW).arg(outH));
        return;
    }
    
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
            emitCompleted(false, QString(), tr("Video processing cancelled"));
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
                emitCompleted(false, QString(), tr("Video processing cancelled"));
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
            
            // 【安全验证】验证帧尺寸有效性
            if (frameW <= 0 || frameH <= 0 || frameW > 16384 || frameH > 16384) {
                qWarning() << "[AIVideoProcessor][DEBUG] Invalid frame dimensions at frame" << frameCount
                           << "width:" << frameW << "height:" << frameH;
                av_frame_unref(decFrame);
                continue;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "dimensions:" << frameW << "x" << frameH;
            fflush(stdout);
            
            // 【安全验证】验证 SwsContext 状态
            if (!decSwsCtx) {
                qWarning() << "[AIVideoProcessor][DEBUG] SwsContext is null at frame" << frameCount;
                av_frame_unref(decFrame);
                continue;
            }
            
            // 【安全验证】验证源数据有效性
            if (!decFrame->data[0]) {
                qWarning() << "[AIVideoProcessor][DEBUG] decFrame data is null at frame" << frameCount;
                av_frame_unref(decFrame);
                continue;
            }
            
            // 【关键修复】使用 AVFrame 作为中间缓冲区，让 FFmpeg 自己管理内存
            // 这是最安全的方式，避免手动内存管理导致的问题
            AVFrame* rgbFrame = av_frame_alloc();
            if (!rgbFrame) {
                qWarning() << "[AIVideoProcessor][DEBUG] Failed to allocate RGB frame at frame" << frameCount;
                av_frame_unref(decFrame);
                continue;
            }
            
            rgbFrame->format = AV_PIX_FMT_RGB24;
            rgbFrame->width = frameW;
            rgbFrame->height = frameH;
            
            // 让 FFmpeg 分配缓冲区
            int ret = av_frame_get_buffer(rgbFrame, 32);  // 32 字节对齐
            if (ret < 0) {
                qWarning() << "[AIVideoProcessor][DEBUG] Failed to allocate RGB frame buffer at frame" << frameCount;
                av_frame_free(&rgbFrame);
                av_frame_unref(decFrame);
                continue;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "calling sws_scale"
                    << "srcSize:" << decFrame->width << "x" << decFrame->height
                    << "dstSize:" << frameW << "x" << frameH
                    << "srcStride:" << decFrame->linesize[0]
                    << "dstStride:" << rgbFrame->linesize[0];
            fflush(stdout);
            
            int swsRet = sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
                                   0, frameH, rgbFrame->data, rgbFrame->linesize);
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "sws_scale done, ret:" << swsRet;
            fflush(stdout);
            
            if (swsRet != frameH) {
                qWarning() << "[AIVideoProcessor][DEBUG] sws_scale returned unexpected height:"
                           << swsRet << "expected:" << frameH;
                av_frame_free(&rgbFrame);
                av_frame_unref(decFrame);
                continue;
            }
            
            // 直接创建目标 QImage 并手动拷贝数据
            QImage processedFrame(frameW, frameH, QImage::Format_RGB888);
            if (processedFrame.isNull()) {
                qWarning() << "[AIVideoProcessor][DEBUG] Failed to create processedFrame at frame" << frameCount;
                av_frame_free(&rgbFrame);
                av_frame_unref(decFrame);
                continue;
            }
            
            // 逐行拷贝数据到 QImage
            const int copyBytes = frameW * 3;  // RGB888 每像素 3 字节
            for (int y = 0; y < frameH; ++y) {
                std::memcpy(processedFrame.scanLine(y), 
                           rgbFrame->data[0] + y * rgbFrame->linesize[0], 
                           copyBytes);
            }
            
            // 释放 RGB 帧
            av_frame_free(&rgbFrame);
            
            if (processedFrame.isNull() || !processedFrame.bits()) {
                qWarning() << "[AIVideoProcessor][DEBUG] processedFrame is invalid at frame" << frameCount;
                av_frame_unref(decFrame);
                continue;
            }
            
            qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "processedFrame created:"
                    << "size:" << processedFrame.width() << "x" << processedFrame.height()
                    << "format:" << processedFrame.format()
                    << "bytesPerLine:" << processedFrame.bytesPerLine()
                    << "isNull:" << processedFrame.isNull();
            fflush(stdout);
            if (compatibilityReport.formatCompatibility == FormatCompatibility::NeedsToneMapping) {
                QImage toneMapped = formatConverter.applyToneMapping(processedFrame);
                if (!toneMapped.isNull()) {
                    processedFrame = toneMapped;
                } else {
                    qWarning() << "[AIVideoProcessor] frame" << frameCount << "tone mapping failed, using original";
                }
            } else if (compatibilityReport.formatCompatibility == FormatCompatibility::NeedsColorConvert) {
                QImage converted = formatConverter.convertPixelFormat(processedFrame, frameFmt, AV_PIX_FMT_RGB24);
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
            
            // 标记开始处理帧
            m_impl->processingFrame.store(true);
            
            // 取消检测点：在AI推理前检查
            if (m_impl->cancelled.load()) {
                m_impl->processingFrame.store(false);
                m_impl->frameCompleteCv.notify_all();
                qInfo() << "[AIVideoProcessor][DEBUG] Cancelled before AI inference at frame" << frameCount;
                av_frame_unref(decFrame);
                cleanupFrames();
                if (decSwsCtx) sws_freeContext(decSwsCtx);
                setProcessing(false);
                emitCompleted(false, QString(), tr("Video processing cancelled"));
                return;
            }
            
            QImage resultImg = processFrame(adaptedFrame);
            
            // 标记帧处理完成
            {
                m_impl->processingFrame.store(false);
                m_impl->frameCompleteCv.notify_all();
            }
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
                    fail(tr("AI inference failed repeatedly. The model may be incompatible or GPU memory insufficient"));
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
                    qInfo() << "[AIVideoProcessor][DEBUG] Frame failures reached"
                            << failedFrames;
                }
                
                // 使用原始帧作为备选（保持视频连续性）
                resultImg = adaptedFrame;
            } else {
                // 成功处理，重置连续失败计数
                consecutiveFrameFailures = 0;
                processedOkFrames++;
                
                // 【关键修复】每处理 2 帧清理一次推理缓存，避免内存累积导致堆损坏
                if (processedOkFrames % 2 == 0) {
                    m_impl->aiEngine->clearInferenceCache();
                }
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
                    fail(tr("Failed to allocate encoder frame buffer"));
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
            
            // 【内存管理增强】显式释放中间QImage对象，减少内存峰值
            // QImage 使用隐式共享（COW），显式清空可强制释放底层数据
            processedFrame = QImage();
            adaptedFrame = QImage();
            resultImg = QImage();
            restoredImg = QImage();
            
            // 【关键修复】释放解码帧引用，避免内存泄漏和堆损坏
            av_frame_unref(decFrame);
            
            // 【内存管理】每10帧强制执行一次GC提示（Qt不保证立即回收，但有助于释放大块内存）
            if (frameCount % 10 == 0) {
                m_impl->aiEngine->clearInferenceCache();
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
            emitCompleted(false, QString(), tr("Video processing completed but output file is invalid"));
        }
    } else {
        qWarning() << "[AIVideoProcessor][ERROR] No frames processed successfully";
        emitCompleted(false, QString(), tr("Video processing failed: no frames successfully processed"));
    }
}

QImage AIVideoProcessor::processFrame(const QImage& input)
{
    // 【详细日志】记录推理开始
    qInfo() << "[AIVideoProcessor][DEBUG] processFrame() called:"
            << "inputSize:" << input.width() << "x" << input.height()
            << "format:" << input.format()
            << "isNull:" << input.isNull()
            << "bits:" << (input.bits() != nullptr ? "valid" : "null")
            << "bytesPerLine:" << input.bytesPerLine();
    fflush(stdout);
    
    if (!m_impl->aiEngine) {
        qWarning() << "[AIVideoProcessor] no AI engine set";
        return QImage();
    }
    
    if (input.isNull() || input.width() <= 0 || input.height() <= 0) {
        qWarning() << "[AIVideoProcessor] invalid input image";
        return QImage();
    }
    
    // 【安全检查】验证 input 的 bits() 是否有效
    const uchar* inputBits = input.bits();
    if (inputBits == nullptr) {
        qWarning() << "[AIVideoProcessor] input.bits() is null";
        return QImage();
    }
    
    // 【安全检查】验证 input 的尺寸和 bytesPerLine 是否合理
    const int expectedBytesPerLine = input.width() * 3;  // RGB888 = 3 bytes per pixel
    if (input.bytesPerLine() < expectedBytesPerLine) {
        qWarning() << "[AIVideoProcessor] input.bytesPerLine() is too small:"
                   << input.bytesPerLine() << "expected:" << expectedBytesPerLine;
        return QImage();
    }
    
    if (m_impl->cancelled.load()) {
        qInfo() << "[AIVideoProcessor][DEBUG] processFrame() cancelled early";
        return QImage();
    }
    
    // 【关键修复】手动创建新 QImage 并逐行拷贝数据
    // 避免使用 QImage::copy() 和 detach()，因为它们可能在堆损坏时触发崩溃
    qInfo() << "[AIVideoProcessor][DEBUG] processFrame() creating manual deep copy of input";
    fflush(stdout);
    
    const int w = input.width();
    const int h = input.height();
    const int srcBytesPerLine = input.bytesPerLine();
    const int copyBytes = w * 3;  // RGB888 = 3 bytes per pixel
    
    QImage safeInput(w, h, QImage::Format_RGB888);
    if (safeInput.isNull()) {
        qWarning() << "[AIVideoProcessor] failed to create safeInput QImage";
        return QImage();
    }
    
    // 逐行拷贝数据
    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = input.constScanLine(y);
        uchar* dstLine = safeInput.scanLine(y);
        std::memcpy(dstLine, srcLine, copyBytes);
    }
    
    qInfo() << "[AIVideoProcessor][DEBUG] processFrame() deep copy created:"
            << "size:" << safeInput.width() << "x" << safeInput.height()
            << "format:" << safeInput.format()
            << "bits:" << (safeInput.bits() != nullptr ? "valid" : "null")
            << "bytesPerLine:" << safeInput.bytesPerLine();
    fflush(stdout);
    
    if (safeInput.isNull() || safeInput.bits() == nullptr) {
        qWarning() << "[AIVideoProcessor] failed to create deep copy of input";
        return QImage();
    }

    // 确保输入格式为 RGB888，避免格式不兼容导致推理失败
    if (safeInput.format() != QImage::Format_RGB888) {
        qInfo() << "[AIVideoProcessor][DEBUG] processFrame() converting format from" << safeInput.format() << "to RGB888";
        fflush(stdout);
        
        safeInput = safeInput.convertToFormat(QImage::Format_RGB888);
        if (safeInput.isNull()) {
            qWarning() << "[AIVideoProcessor] failed to convert input to RGB888";
            return QImage();
        }
    }

    QImage result;

    qInfo() << "[AIVideoProcessor][DEBUG] processFrame() calling aiEngine->process()";
    fflush(stdout);
    
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

    qInfo() << "[AIVideoProcessor][DEBUG] processFrame() aiEngine->process() returned:"
            << "resultSize:" << result.width() << "x" << result.height()
            << "isNull:" << result.isNull();
    fflush(stdout);
    
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
