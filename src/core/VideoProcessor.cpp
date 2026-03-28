/**
 * @file VideoProcessor.cpp
 * @brief 视频处理器实现 - Shader模式视频处理
 * @author Qt客户端开发工程师
 * 
 * 设计目标：
 * 1. 确保与QML预览区域GPU Shader效果完全一致
 * 2. 支持不同大小比例和格式的视频文件
 * 3. 保留原始音频流
 * 4. 高质量编码输出
 * 5. 精确帧级进度映射
 */

#include "EnhanceVision/core/VideoProcessor.h"
#include "EnhanceVision/core/ImageProcessor.h"
#include "EnhanceVision/core/ProgressReporter.h"
#include <QtConcurrent/QtConcurrent>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace EnhanceVision {

VideoProcessor::VideoProcessor(QObject *parent)
    : QObject(parent)
    , m_isProcessing(false)
    , m_cancelled(false)
    , m_reporter(nullptr)
{
}

VideoProcessor::~VideoProcessor()
{
    cancel();
}

VideoProcessingStage VideoProcessor::currentStage() const
{
    return m_currentStage.load();
}

QString VideoProcessor::stageToString(VideoProcessingStage stage) const
{
    switch (stage) {
        case VideoProcessingStage::Idle: return tr("空闲");
        case VideoProcessingStage::Opening: return tr("打开文件");
        case VideoProcessingStage::DecodingInit: return tr("初始化解码器");
        case VideoProcessingStage::EncodingInit: return tr("初始化编码器");
        case VideoProcessingStage::FrameProcessing: return tr("处理帧");
        case VideoProcessingStage::EncodingFinalize: return tr("完成编码");
        case VideoProcessingStage::Writing: return tr("写入文件");
        case VideoProcessingStage::Completed: return tr("完成");
        default: return QString();
    }
}

void VideoProcessor::reportStageProgress(VideoProcessingStage stage, double stageProgress)
{
    m_currentStage.store(stage);
    emit stageChanged(stage);
    
    if (m_reporter) {
        double overallProgress = 0.0;
        switch (stage) {
            case VideoProcessingStage::Opening:
                overallProgress = kProgressOpeningStart + 
                    (kProgressOpeningEnd - kProgressOpeningStart) * stageProgress;
                break;
            case VideoProcessingStage::DecodingInit:
                overallProgress = kProgressDecodingInitStart + 
                    (kProgressDecodingInitEnd - kProgressDecodingInitStart) * stageProgress;
                break;
            case VideoProcessingStage::EncodingInit:
                overallProgress = kProgressEncodingInitStart + 
                    (kProgressEncodingInitEnd - kProgressEncodingInitStart) * stageProgress;
                break;
            case VideoProcessingStage::FrameProcessing:
                overallProgress = kProgressFrameProcessingStart + 
                    (kProgressFrameProcessingEnd - kProgressFrameProcessingStart) * stageProgress;
                break;
            case VideoProcessingStage::EncodingFinalize:
                overallProgress = kProgressEncodingFinalizeStart + 
                    (kProgressEncodingFinalizeEnd - kProgressEncodingFinalizeStart) * stageProgress;
                break;
            case VideoProcessingStage::Writing:
                overallProgress = kProgressWritingStart + 
                    (kProgressWritingEnd - kProgressWritingStart) * stageProgress;
                break;
            case VideoProcessingStage::Completed:
                overallProgress = 100.0;
                break;
            default:
                break;
        }
        m_reporter->setProgress(overallProgress / 100.0, stageToString(stage));
    }
}

void VideoProcessor::processVideoAsync(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
                                       ProgressCallback progressCallback,
                                       FinishCallback finishCallback)
{
    processVideoAsyncWithReporter(inputPath, outputPath, params, nullptr, progressCallback, finishCallback);
}

void VideoProcessor::processVideoAsyncWithReporter(const QString& inputPath,
                                                    const QString& outputPath,
                                                    const ShaderParams& params,
                                                    ProgressReporter* reporter,
                                                    ProgressCallback progressCallback,
                                                    FinishCallback finishCallback)
{
    if (m_isProcessing) {
        if (finishCallback) {
            finishCallback(false, QString(), tr("已有处理任务正在进行"));
        }
        emit finished(false, QString(), tr("已有处理任务正在进行"));
        return;
    }

    m_isProcessing = true;
    m_cancelled = false;
    m_reporter = reporter;

    if (m_reporter) {
        m_reporter->reset();
        m_reporter->beginBatch(6, tr("处理视频"));
    }

    QtConcurrent::run([this, inputPath, outputPath, params, progressCallback, finishCallback]() {
        processVideoInternal(inputPath, outputPath, params, m_reporter, progressCallback, finishCallback);
    });
}

void VideoProcessor::cancel()
{
    m_cancelled = true;
}

bool VideoProcessor::isProcessing() const
{
    return m_isProcessing;
}

void VideoProcessor::processVideoInternal(const QString& inputPath,
                                          const QString& outputPath,
                                          const ShaderParams& params,
                                          ProgressReporter* reporter,
                                          ProgressCallback progressCallback,
                                          FinishCallback finishCallback)
{
    bool success = false;
    QString error;
    QString resultPath;

    AVFormatContext* inputFormatContext = nullptr;
    AVFormatContext* outputFormatContext = nullptr;
    AVCodecContext* videoDecoderContext = nullptr;
    AVCodecContext* videoEncoderContext = nullptr;
    SwsContext* swsContextToRGB = nullptr;
    SwsContext* swsContextToYUV = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgbFrame = nullptr;
    AVFrame* outFrame = nullptr;
    AVPacket* packet = nullptr;
    
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int outVideoStreamIndex = -1;
    int outAudioStreamIndex = -1;
    
    ImageProcessor imageProcessor;

    auto cleanup = [&]() {
        if (swsContextToRGB) sws_freeContext(swsContextToRGB);
        if (swsContextToYUV) sws_freeContext(swsContextToYUV);
        if (frame) av_frame_free(&frame);
        if (rgbFrame) av_frame_free(&rgbFrame);
        if (outFrame) av_frame_free(&outFrame);
        if (packet) av_packet_free(&packet);
        if (videoEncoderContext) avcodec_free_context(&videoEncoderContext);
        if (videoDecoderContext) avcodec_free_context(&videoDecoderContext);
        if (outputFormatContext) {
            if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&outputFormatContext->pb);
            }
            avformat_free_context(outputFormatContext);
        }
        if (inputFormatContext) avformat_close_input(&inputFormatContext);
    };

    auto reportProgress = [&](int progress, const QString& status) {
        if (progressCallback) progressCallback(progress, status);
        emit progressChanged(progress, status);
    };

    try {
        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }

        reportStageProgress(VideoProcessingStage::Opening, 0.0);
        reportProgress(kProgressOpeningStart, tr("正在分析视频文件..."));

        if (avformat_open_input(&inputFormatContext, inputPath.toUtf8().constData(), nullptr, nullptr) != 0) {
            throw std::runtime_error(tr("无法打开输入视频文件").toStdString());
        }

        if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
            throw std::runtime_error(tr("无法获取视频流信息").toStdString());
        }

        for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
            AVMediaType codecType = inputFormatContext->streams[i]->codecpar->codec_type;
            if (codecType == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1) {
                videoStreamIndex = i;
            } else if (codecType == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1) {
                audioStreamIndex = i;
            }
        }

        if (videoStreamIndex == -1) {
            throw std::runtime_error(tr("未找到视频流").toStdString());
        }

        AVStream* inVideoStream = inputFormatContext->streams[videoStreamIndex];

        reportStageProgress(VideoProcessingStage::Opening, 1.0);
        reportProgress(kProgressOpeningEnd, tr("文件分析完成"));

        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }

        reportStageProgress(VideoProcessingStage::DecodingInit, 0.0);
        reportProgress(kProgressDecodingInitStart, tr("正在初始化解码器..."));

        const AVCodec* decoder = avcodec_find_decoder(inVideoStream->codecpar->codec_id);
        if (!decoder) {
            throw std::runtime_error(tr("未找到合适的视频解码器").toStdString());
        }

        videoDecoderContext = avcodec_alloc_context3(decoder);
        if (!videoDecoderContext) {
            throw std::runtime_error(tr("无法分配解码器上下文").toStdString());
        }

        if (avcodec_parameters_to_context(videoDecoderContext, inVideoStream->codecpar) < 0) {
            throw std::runtime_error(tr("无法复制解码器参数").toStdString());
        }

        videoDecoderContext->thread_count = qMin(QThread::idealThreadCount(), 8);

        if (avcodec_open2(videoDecoderContext, decoder, nullptr) < 0) {
            throw std::runtime_error(tr("无法打开视频解码器").toStdString());
        }

        int srcWidth = videoDecoderContext->width;
        int srcHeight = videoDecoderContext->height;
        AVPixelFormat srcPixFmt = videoDecoderContext->pix_fmt;

        reportStageProgress(VideoProcessingStage::DecodingInit, 1.0);
        reportProgress(kProgressDecodingInitEnd, tr("解码器初始化完成"));

        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }

        reportStageProgress(VideoProcessingStage::EncodingInit, 0.0);
        reportProgress(kProgressEncodingInitStart, tr("正在初始化编码器..."));

        if (avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, 
                                            outputPath.toUtf8().constData()) < 0) {
            throw std::runtime_error(tr("无法创建输出格式上下文").toStdString());
        }

        AVStream* outVideoStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outVideoStream) {
            throw std::runtime_error(tr("无法创建输出视频流").toStdString());
        }
        outVideoStreamIndex = outVideoStream->index;

        const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encoder) {
            encoder = avcodec_find_encoder(outputFormatContext->oformat->video_codec);
        }
        if (!encoder) {
            throw std::runtime_error(tr("未找到合适的视频编码器").toStdString());
        }

        videoEncoderContext = avcodec_alloc_context3(encoder);
        if (!videoEncoderContext) {
            throw std::runtime_error(tr("无法分配编码器上下文").toStdString());
        }

        videoEncoderContext->width = srcWidth;
        videoEncoderContext->height = srcHeight;
        videoEncoderContext->sample_aspect_ratio = videoDecoderContext->sample_aspect_ratio;
        videoEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
        
        if (inVideoStream->avg_frame_rate.num > 0 && inVideoStream->avg_frame_rate.den > 0) {
            videoEncoderContext->framerate = inVideoStream->avg_frame_rate;
        } else if (videoDecoderContext->framerate.num > 0 && videoDecoderContext->framerate.den > 0) {
            videoEncoderContext->framerate = videoDecoderContext->framerate;
        } else {
            videoEncoderContext->framerate = AVRational{25, 1};
        }
        videoEncoderContext->time_base = av_inv_q(videoEncoderContext->framerate);

        videoEncoderContext->bit_rate = static_cast<int64_t>(srcWidth) * srcHeight * 4;
        videoEncoderContext->gop_size = 12;
        videoEncoderContext->max_b_frames = 2;
        videoEncoderContext->thread_count = qMin(QThread::idealThreadCount(), 8);

        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
            videoEncoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        AVDictionary* encoderOpts = nullptr;
        av_dict_set(&encoderOpts, "preset", "medium", 0);
        av_dict_set(&encoderOpts, "crf", "18", 0);

        if (avcodec_open2(videoEncoderContext, encoder, &encoderOpts) < 0) {
            av_dict_free(&encoderOpts);
            throw std::runtime_error(tr("无法打开视频编码器").toStdString());
        }
        av_dict_free(&encoderOpts);

        if (avcodec_parameters_from_context(outVideoStream->codecpar, videoEncoderContext) < 0) {
            throw std::runtime_error(tr("无法复制编码器参数到输出流").toStdString());
        }
        outVideoStream->time_base = videoEncoderContext->time_base;

        AVStream* outAudioStream = nullptr;
        if (audioStreamIndex >= 0) {
            AVStream* inAudioStream = inputFormatContext->streams[audioStreamIndex];
            outAudioStream = avformat_new_stream(outputFormatContext, nullptr);
            if (outAudioStream) {
                outAudioStreamIndex = outAudioStream->index;
                if (avcodec_parameters_copy(outAudioStream->codecpar, inAudioStream->codecpar) >= 0) {
                    outAudioStream->time_base = inAudioStream->time_base;
                } else {
                    outAudioStreamIndex = -1;
                    qWarning() << "[VideoProcessor] Failed to copy audio parameters";
                }
            }
        }

        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error(tr("无法打开输出文件").toStdString());
            }
        }

        if (avformat_write_header(outputFormatContext, nullptr) < 0) {
            throw std::runtime_error(tr("无法写入文件头").toStdString());
        }

        reportStageProgress(VideoProcessingStage::EncodingInit, 1.0);
        reportProgress(kProgressEncodingInitEnd, tr("编码器初始化完成"));

        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }

        swsContextToRGB = sws_getContext(
            srcWidth, srcHeight, srcPixFmt,
            srcWidth, srcHeight, AV_PIX_FMT_RGB32,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!swsContextToRGB) {
            throw std::runtime_error(tr("无法创建RGB转换上下文").toStdString());
        }

        swsContextToYUV = sws_getContext(
            srcWidth, srcHeight, AV_PIX_FMT_RGB32,
            srcWidth, srcHeight, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!swsContextToYUV) {
            throw std::runtime_error(tr("无法创建YUV转换上下文").toStdString());
        }

        frame = av_frame_alloc();
        rgbFrame = av_frame_alloc();
        outFrame = av_frame_alloc();
        packet = av_packet_alloc();

        if (!frame || !rgbFrame || !outFrame || !packet) {
            throw std::runtime_error(tr("无法分配帧缓冲").toStdString());
        }

        rgbFrame->format = AV_PIX_FMT_RGB32;
        rgbFrame->width = srcWidth;
        rgbFrame->height = srcHeight;
        if (av_frame_get_buffer(rgbFrame, 32) < 0) {
            throw std::runtime_error(tr("无法分配RGB帧缓冲").toStdString());
        }

        outFrame->format = AV_PIX_FMT_YUV420P;
        outFrame->width = srcWidth;
        outFrame->height = srcHeight;
        if (av_frame_get_buffer(outFrame, 32) < 0) {
            throw std::runtime_error(tr("无法分配输出帧缓冲").toStdString());
        }

        int64_t totalFrames = inVideoStream->nb_frames;
        if (totalFrames <= 0 && inputFormatContext->duration > 0) {
            double fps = av_q2d(videoEncoderContext->framerate);
            if (fps > 0) {
                totalFrames = static_cast<int64_t>(
                    (inputFormatContext->duration / static_cast<double>(AV_TIME_BASE)) * fps
                );
            }
        }
        if (totalFrames <= 0) totalFrames = 1000;

        reportStageProgress(VideoProcessingStage::FrameProcessing, 0.0);
        reportProgress(kProgressFrameProcessingStart, tr("开始处理视频帧..."));

        int64_t frameCount = 0;
        int64_t encodedPts = 0;
        int lastReportedPercent = kProgressFrameProcessingStart;
        const int frameProgressRange = kProgressFrameProcessingEnd - kProgressFrameProcessingStart;

        while (av_read_frame(inputFormatContext, packet) >= 0) {
            if (m_cancelled) {
                av_packet_unref(packet);
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            if (packet->stream_index == videoStreamIndex) {
                int ret = avcodec_send_packet(videoDecoderContext, packet);
                if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                    qWarning() << "[VideoProcessor] Decode send packet error:" << ret;
                }

                while (avcodec_receive_frame(videoDecoderContext, frame) == 0) {
                    if (m_cancelled) {
                        throw std::runtime_error(tr("处理已取消").toStdString());
                    }

                    sws_scale(swsContextToRGB,
                              frame->data, frame->linesize, 0, frame->height,
                              rgbFrame->data, rgbFrame->linesize);

                    QImage image(rgbFrame->data[0], srcWidth, srcHeight, 
                                 rgbFrame->linesize[0], QImage::Format_RGB32);
                    
                    QImage processedImage = imageProcessor.applyShader(image, params);

                    QImage converted = processedImage.convertToFormat(QImage::Format_RGB32);
                    if (converted.bytesPerLine() == rgbFrame->linesize[0]) {
                        memcpy(rgbFrame->data[0], converted.constBits(), 
                               static_cast<size_t>(srcHeight) * rgbFrame->linesize[0]);
                    } else {
                        for (int y = 0; y < srcHeight; ++y) {
                            memcpy(rgbFrame->data[0] + y * rgbFrame->linesize[0],
                                   converted.constScanLine(y),
                                   qMin(converted.bytesPerLine(), rgbFrame->linesize[0]));
                        }
                    }

                    sws_scale(swsContextToYUV,
                              rgbFrame->data, rgbFrame->linesize, 0, srcHeight,
                              outFrame->data, outFrame->linesize);

                    outFrame->pts = encodedPts++;

                    ret = avcodec_send_frame(videoEncoderContext, outFrame);
                    if (ret < 0 && ret != AVERROR(EAGAIN)) {
                        qWarning() << "[VideoProcessor] Encode send frame error:" << ret;
                    }

                    AVPacket* outPacket = av_packet_alloc();
                    while (avcodec_receive_packet(videoEncoderContext, outPacket) == 0) {
                        av_packet_rescale_ts(outPacket, videoEncoderContext->time_base, 
                                             outVideoStream->time_base);
                        outPacket->stream_index = outVideoStreamIndex;
                        
                        ret = av_interleaved_write_frame(outputFormatContext, outPacket);
                        if (ret < 0) {
                            qWarning() << "[VideoProcessor] Write video frame error:" << ret;
                        }
                        av_packet_unref(outPacket);
                    }
                    av_packet_free(&outPacket);

                    frameCount++;
                    
                    double frameProgress = static_cast<double>(frameCount) / totalFrames;
                    int currentPercent = static_cast<int>(
                        kProgressFrameProcessingStart + frameProgressRange * frameProgress);
                    currentPercent = qBound(kProgressFrameProcessingStart, currentPercent, kProgressFrameProcessingEnd - 1);
                    
                    if (currentPercent - lastReportedPercent >= 1 || frameCount == 1) {
                        QString statusMsg = tr("正在处理第 %1/%2 帧...").arg(frameCount).arg(totalFrames);
                        
                        if (reporter) {
                            reporter->setSubProgress(frameProgress, statusMsg);
                        }
                        reportProgress(currentPercent, statusMsg);
                        lastReportedPercent = currentPercent;
                    }
                }
            } else if (packet->stream_index == audioStreamIndex && outAudioStreamIndex >= 0) {
                AVPacket* audioPacket = av_packet_clone(packet);
                if (audioPacket) {
                    av_packet_rescale_ts(audioPacket,
                                         inputFormatContext->streams[audioStreamIndex]->time_base,
                                         outputFormatContext->streams[outAudioStreamIndex]->time_base);
                    audioPacket->stream_index = outAudioStreamIndex;
                    
                    int ret = av_interleaved_write_frame(outputFormatContext, audioPacket);
                    if (ret < 0) {
                        qWarning() << "[VideoProcessor] Write audio frame error:" << ret;
                    }
                    av_packet_free(&audioPacket);
                }
            }

            av_packet_unref(packet);
        }

        reportStageProgress(VideoProcessingStage::FrameProcessing, 1.0);

        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }

        reportStageProgress(VideoProcessingStage::EncodingFinalize, 0.0);
        reportProgress(kProgressEncodingFinalizeStart, tr("正在完成编码..."));

        avcodec_send_frame(videoEncoderContext, nullptr);
        AVPacket* outPacket = av_packet_alloc();
        while (avcodec_receive_packet(videoEncoderContext, outPacket) == 0) {
            av_packet_rescale_ts(outPacket, videoEncoderContext->time_base, outVideoStream->time_base);
            outPacket->stream_index = outVideoStreamIndex;
            av_interleaved_write_frame(outputFormatContext, outPacket);
            av_packet_unref(outPacket);
        }
        av_packet_free(&outPacket);

        reportStageProgress(VideoProcessingStage::EncodingFinalize, 1.0);

        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }

        reportStageProgress(VideoProcessingStage::Writing, 0.0);
        reportProgress(kProgressWritingStart, tr("正在写入文件..."));

        if (av_write_trailer(outputFormatContext) < 0) {
            throw std::runtime_error(tr("无法写入文件尾").toStdString());
        }

        resultPath = outputPath;
        success = true;

        reportStageProgress(VideoProcessingStage::Writing, 1.0);
        reportStageProgress(VideoProcessingStage::Completed, 1.0);
        
        if (reporter) {
            reporter->endBatch();
        }
        reportProgress(100, tr("处理完成"));

    } catch (const std::exception& e) {
        error = QString::fromStdString(e.what());
        success = false;
        qWarning() << "[VideoProcessor] Error:" << error;
        
        if (reporter) {
            reporter->reset();
        }
        
        if (!success && QFile::exists(outputPath)) {
            QFile::remove(outputPath);
        }
    }

    cleanup();
    m_isProcessing = false;
    m_reporter = nullptr;

    if (m_cancelled) {
        if (finishCallback) {
            finishCallback(false, QString(), tr("处理已取消"));
        }
        emit finished(false, QString(), tr("处理已取消"));
    } else {
        if (finishCallback) {
            finishCallback(success, resultPath, error);
        }
        emit finished(success, resultPath, error);
    }
}

} // namespace EnhanceVision
