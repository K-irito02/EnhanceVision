/**
 * @file VideoProcessor.cpp
 * @brief 视频处理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/VideoProcessor.h"
#include "EnhanceVision/core/ImageProcessor.h"
#include <QtConcurrent/QtConcurrent>
#include <QFile>
#include <QDebug>
#include <QElapsedTimer>

namespace EnhanceVision {

VideoProcessor::VideoProcessor(QObject *parent)
    : QObject(parent)
    , m_isProcessing(false)
    , m_cancelled(false)
{
}

VideoProcessor::~VideoProcessor()
{
    cancel();
}

void VideoProcessor::processVideoAsync(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
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

    QtConcurrent::run([this, inputPath, outputPath, params, progressCallback, finishCallback]() {
        processVideoInternal(inputPath, outputPath, params, progressCallback, finishCallback);
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
    SwsContext* swsContext = nullptr;
    SwsContext* frameToImageSwsCtx = nullptr;  // 缓存的解码转换上下文
    ImageProcessor imageProcessor;
    
    QElapsedTimer perfTimer;
    perfTimer.start();
    
    qInfo() << "[VideoProcessor] Starting video processing:" << inputPath;

    try {
        // 打开输入文件
        if (progressCallback) {
            progressCallback(5, tr("正在打开视频文件..."));
        }
        emit progressChanged(5, tr("正在打开视频文件..."));

        if (avformat_open_input(&inputFormatContext, inputPath.toUtf8().constData(), nullptr, nullptr) != 0) {
            throw std::runtime_error(tr("无法打开输入视频文件").toStdString());
        }

        if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
            throw std::runtime_error(tr("无法获取视频流信息").toStdString());
        }

        // 查找视频流和音频流
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
            if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1) {
                videoStreamIndex = i;
            } else if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1) {
                audioStreamIndex = i;
            }
        }

        if (videoStreamIndex == -1) {
            throw std::runtime_error(tr("未找到视频流").toStdString());
        }

        // 初始化解码器
        const AVCodec* decoder = avcodec_find_decoder(inputFormatContext->streams[videoStreamIndex]->codecpar->codec_id);
        if (!decoder) {
            throw std::runtime_error(tr("未找到合适的解码器").toStdString());
        }

        videoDecoderContext = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(videoDecoderContext, inputFormatContext->streams[videoStreamIndex]->codecpar);
        if (avcodec_open2(videoDecoderContext, decoder, nullptr) < 0) {
            throw std::runtime_error(tr("无法打开解码器").toStdString());
        }

        // 初始化输出格式上下文
        avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, outputPath.toUtf8().constData());
        if (!outputFormatContext) {
            throw std::runtime_error(tr("无法创建输出格式上下文").toStdString());
        }

        // 添加视频流到输出
        AVStream* outVideoStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outVideoStream) {
            throw std::runtime_error(tr("无法创建输出视频流").toStdString());
        }

        // 查找编码器
        const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encoder) {
            encoder = avcodec_find_encoder(outputFormatContext->oformat->video_codec);
        }
        if (!encoder) {
            throw std::runtime_error(tr("未找到合适的编码器").toStdString());
        }

        videoEncoderContext = avcodec_alloc_context3(encoder);
        videoEncoderContext->height = videoDecoderContext->height;
        videoEncoderContext->width = videoDecoderContext->width;
        videoEncoderContext->sample_aspect_ratio = videoDecoderContext->sample_aspect_ratio;
        videoEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
        videoEncoderContext->time_base = inputFormatContext->streams[videoStreamIndex]->time_base;
        videoEncoderContext->framerate = videoDecoderContext->framerate;

        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
            videoEncoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        if (avcodec_open2(videoEncoderContext, encoder, nullptr) < 0) {
            throw std::runtime_error(tr("无法打开编码器").toStdString());
        }

        avcodec_parameters_from_context(outVideoStream->codecpar, videoEncoderContext);

        // 添加音频流到输出（如果存在）
        AVStream* outAudioStream = nullptr;
        int outAudioStreamIndex = -1;
        if (audioStreamIndex != -1) {
            outAudioStream = avformat_new_stream(outputFormatContext, nullptr);
            if (outAudioStream) {
                outAudioStreamIndex = outAudioStream->index;
                // 直接复制音频流参数（不重新编码）
                avcodec_parameters_copy(outAudioStream->codecpar, inputFormatContext->streams[audioStreamIndex]->codecpar);
                outAudioStream->codecpar->codec_tag = 0;
                outAudioStream->time_base = inputFormatContext->streams[audioStreamIndex]->time_base;
            }
        }

        // 打开输出文件
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error(tr("无法打开输出文件").toStdString());
            }
        }

        // 写文件头
        avformat_write_header(outputFormatContext, nullptr);

        // 分配帧
        AVFrame* frame = av_frame_alloc();
        AVFrame* outFrame = av_frame_alloc();
        outFrame->format = videoEncoderContext->pix_fmt;
        outFrame->width = videoEncoderContext->width;
        outFrame->height = videoEncoderContext->height;
        av_frame_get_buffer(outFrame, 32);

        AVPacket* packet = av_packet_alloc();
        
        // 用于 BGRA -> 编码器格式转换的上下文（延迟初始化）
        SwsContext* bgraToEncSwsCtx = nullptr;

        // 获取总帧数（精确计算）
        int64_t totalFrames = inputFormatContext->streams[videoStreamIndex]->nb_frames;
        if (totalFrames <= 0) {
            // 使用 duration（以 AV_TIME_BASE 为单位）和帧率计算
            double durationSec = static_cast<double>(inputFormatContext->duration) / AV_TIME_BASE;
            double fps = 30.0;  // 默认帧率
            if (videoDecoderContext->framerate.num > 0 && videoDecoderContext->framerate.den > 0) {
                fps = static_cast<double>(videoDecoderContext->framerate.num) / videoDecoderContext->framerate.den;
            } else if (inputFormatContext->streams[videoStreamIndex]->avg_frame_rate.num > 0 &&
                       inputFormatContext->streams[videoStreamIndex]->avg_frame_rate.den > 0) {
                fps = static_cast<double>(inputFormatContext->streams[videoStreamIndex]->avg_frame_rate.num) /
                      inputFormatContext->streams[videoStreamIndex]->avg_frame_rate.den;
            }
            totalFrames = static_cast<int64_t>(durationSec * fps);
        }
        if (totalFrames <= 0) totalFrames = 100;  // 最小估算值

        int64_t frameCount = 0;
        int lastReportedProgress = 0;

        if (progressCallback) {
            progressCallback(5, tr("开始处理视频帧..."));
        }
        emit progressChanged(5, tr("开始处理视频帧..."));
        
        qInfo() << "[VideoProcessor] Starting frame loop, totalFrames:" << totalFrames;

        // 解码、处理、编码循环
        while (av_read_frame(inputFormatContext, packet) >= 0) {
            if (m_cancelled) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            // 处理音频流：直接复制
            if (packet->stream_index == audioStreamIndex && outAudioStream) {
                AVPacket* audioPacket = av_packet_clone(packet);
                if (audioPacket) {
                    av_packet_rescale_ts(audioPacket, 
                        inputFormatContext->streams[audioStreamIndex]->time_base,
                        outAudioStream->time_base);
                    audioPacket->stream_index = outAudioStreamIndex;
                    av_interleaved_write_frame(outputFormatContext, audioPacket);
                    av_packet_free(&audioPacket);
                }
            }
            // 处理视频流
            else if (packet->stream_index == videoStreamIndex) {
                avcodec_send_packet(videoDecoderContext, packet);

                while (avcodec_receive_frame(videoDecoderContext, frame) == 0) {
                    if (m_cancelled) {
                        throw std::runtime_error(tr("处理已取消").toStdString());
                    }

                    // 延迟初始化解码转换上下文
                    if (!frameToImageSwsCtx) {
                        frameToImageSwsCtx = sws_getContext(
                            frame->width, frame->height, videoDecoderContext->pix_fmt,
                            frame->width, frame->height, AV_PIX_FMT_RGB32,
                            SWS_BILINEAR, nullptr, nullptr, nullptr
                        );
                        if (!frameToImageSwsCtx) {
                            throw std::runtime_error(tr("无法创建解码转换上下文").toStdString());
                        }
                    }

                    // 将 AVFrame 转换为 QImage（使用缓存的上下文）
                    QImage image(frame->width, frame->height, QImage::Format_RGB32);
                    uint8_t* dstData[4] = { image.bits(), nullptr, nullptr, nullptr };
                    int dstLinesize[4] = { image.bytesPerLine(), 0, 0, 0 };
                    sws_scale(frameToImageSwsCtx, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
                    
                    if (image.isNull()) {
                        qWarning() << "[VideoProcessor] frameToImage returned null, skipping frame";
                        continue;
                    }

                    // 应用滤镜
                    QImage processedImage = imageProcessor.applyShader(image, params);
                    if (processedImage.isNull()) {
                        qWarning() << "[VideoProcessor] applyShader returned null, using original";
                        processedImage = image;
                    }

                    // 将 QImage 转换回 AVFrame (BGRA格式)
                    AVFrame* processedFrame = imageToFrame(processedImage, frame);
                    if (!processedFrame) {
                        qWarning() << "[VideoProcessor] imageToFrame returned null, skipping frame";
                        continue;
                    }

                    // 延迟初始化 BGRA -> 编码器格式的转换上下文
                    if (!bgraToEncSwsCtx) {
                        bgraToEncSwsCtx = sws_getContext(
                            processedFrame->width, processedFrame->height, AV_PIX_FMT_BGRA,
                            videoEncoderContext->width, videoEncoderContext->height, videoEncoderContext->pix_fmt,
                            SWS_BILINEAR, nullptr, nullptr, nullptr
                        );
                        if (!bgraToEncSwsCtx) {
                            av_frame_free(&processedFrame);
                            throw std::runtime_error(tr("无法创建图像格式转换上下文").toStdString());
                        }
                    }

                    // 转换为编码器像素格式
                    sws_scale(bgraToEncSwsCtx,
                              processedFrame->data, processedFrame->linesize, 0, processedFrame->height,
                              outFrame->data, outFrame->linesize);

                    outFrame->pts = frame->pts;

                    // 编码
                    avcodec_send_frame(videoEncoderContext, outFrame);

                    AVPacket* outPacket = av_packet_alloc();
                    while (avcodec_receive_packet(videoEncoderContext, outPacket) == 0) {
                        av_packet_rescale_ts(outPacket, videoEncoderContext->time_base, outVideoStream->time_base);
                        outPacket->stream_index = outVideoStream->index;
                        av_interleaved_write_frame(outputFormatContext, outPacket);
                        av_packet_unref(outPacket);
                    }
                    av_packet_free(&outPacket);

                    av_frame_free(&processedFrame);

                    frameCount++;
                    // 进度计算：5-90% 范围内处理帧
                    int progress = 5 + qRound(85.0 * frameCount / totalFrames);
                    progress = qBound(5, progress, 90);

                    // 每处理100帧输出日志（减少日志频率）
                    if (frameCount == 1 || frameCount % 100 == 0) {
                        qInfo() << "[VideoProcessor] Frame" << frameCount << "/" << totalFrames 
                                << "progress:" << progress << "% elapsed:" << perfTimer.elapsed() << "ms";
                    }

                    // 只在进度变化至少1%时报告，避免过多更新
                    if (progress > lastReportedProgress) {
                        lastReportedProgress = progress;
                        if (progressCallback) {
                            progressCallback(progress, tr("正在处理第 %1/%2 帧...").arg(frameCount).arg(totalFrames));
                        }
                        emit progressChanged(progress, tr("正在处理第 %1/%2 帧...").arg(frameCount).arg(totalFrames));
                    }
                }
            }
            av_packet_unref(packet);
        }

        // 刷新编码器
        avcodec_send_frame(videoEncoderContext, nullptr);
        AVPacket* outPacket = av_packet_alloc();
        while (avcodec_receive_packet(videoEncoderContext, outPacket) == 0) {
            av_packet_rescale_ts(outPacket, videoEncoderContext->time_base, outVideoStream->time_base);
            outPacket->stream_index = outVideoStream->index;
            av_interleaved_write_frame(outputFormatContext, outPacket);
            av_packet_unref(outPacket);
        }
        av_packet_free(&outPacket);

        // 写文件尾
        av_write_trailer(outputFormatContext);

        // 清理转换上下文
        if (frameToImageSwsCtx) sws_freeContext(frameToImageSwsCtx);
        if (bgraToEncSwsCtx) sws_freeContext(bgraToEncSwsCtx);
        
        // 清理
        av_frame_free(&frame);
        av_frame_free(&outFrame);
        av_packet_free(&packet);

        if (progressCallback) {
            progressCallback(95, tr("正在完成..."));
        }
        emit progressChanged(95, tr("正在完成..."));

        resultPath = outputPath;
        success = true;

        if (progressCallback) {
            progressCallback(100, tr("处理完成"));
        }
        emit progressChanged(100, tr("处理完成"));

        qInfo() << "[VideoProcessor] Video processing completed successfully in" << perfTimer.elapsed() << "ms";

    } catch (const std::exception& e) {
        error = QString::fromStdString(e.what());
        success = false;
        qWarning() << "[VideoProcessor] Video processing failed:" << error;
        
        // 确保清理转换上下文
        if (frameToImageSwsCtx) sws_freeContext(frameToImageSwsCtx);
    }

    // 清理资源（注意：bgraToEncSwsCtx 已在 try 块内清理）
    if (videoEncoderContext) avcodec_free_context(&videoEncoderContext);
    if (videoDecoderContext) avcodec_free_context(&videoDecoderContext);
    if (outputFormatContext) {
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputFormatContext->pb);
        }
        avformat_free_context(outputFormatContext);
    }
    if (inputFormatContext) avformat_close_input(&inputFormatContext);

    m_isProcessing = false;

    if (finishCallback) {
        finishCallback(success, resultPath, error);
    }
    emit finished(success, resultPath, error);
}

QImage VideoProcessor::frameToImage(AVFrame* frame, AVPixelFormat pixelFormat)
{
    if (!frame) return QImage();

    SwsContext* swsCtx = sws_getContext(
        frame->width, frame->height, pixelFormat,
        frame->width, frame->height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsCtx) return QImage();

    QImage image(frame->width, frame->height, QImage::Format_RGB32);

    uint8_t* dstData[4] = { image.bits(), nullptr, nullptr, nullptr };
    int dstLinesize[4] = { image.bytesPerLine(), 0, 0, 0 };

    sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
    sws_freeContext(swsCtx);

    return image;
}

AVFrame* VideoProcessor::imageToFrame(const QImage& image, AVFrame* referenceFrame)
{
    if (image.isNull() || !referenceFrame) return nullptr;

    // 确保图像格式正确，并创建深拷贝避免数据失效
    QImage rgbImage = image.convertToFormat(QImage::Format_RGB32).copy();
    if (rgbImage.isNull() || rgbImage.width() <= 0 || rgbImage.height() <= 0) {
        qWarning() << "[VideoProcessor] imageToFrame: invalid RGB image after conversion";
        return nullptr;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        qWarning() << "[VideoProcessor] imageToFrame: failed to allocate frame";
        return nullptr;
    }
    
    frame->format = AV_PIX_FMT_BGRA;  // Qt的Format_RGB32实际是BGRA格式
    frame->width = rgbImage.width();
    frame->height = rgbImage.height();
    
    if (av_frame_get_buffer(frame, 32) < 0) {
        qWarning() << "[VideoProcessor] imageToFrame: failed to allocate frame buffer";
        av_frame_free(&frame);
        return nullptr;
    }

    // 直接复制像素数据，避免使用sws_scale进行同格式转换
    const int srcBytesPerLine = rgbImage.bytesPerLine();
    const int dstBytesPerLine = frame->linesize[0];
    const int copyBytes = qMin(srcBytesPerLine, dstBytesPerLine);
    
    for (int y = 0; y < rgbImage.height(); ++y) {
        const uchar* srcLine = rgbImage.constScanLine(y);
        uint8_t* dstLine = frame->data[0] + y * dstBytesPerLine;
        std::memcpy(dstLine, srcLine, copyBytes);
    }

    frame->pts = referenceFrame->pts;
    frame->pict_type = referenceFrame->pict_type;
#if LIBAVUTIL_VERSION_MAJOR >= 58
    // FFmpeg 7.x: key_frame 移至 flags
    if (referenceFrame->flags & AV_FRAME_FLAG_KEY) {
        frame->flags |= AV_FRAME_FLAG_KEY;
    }
#else
    frame->key_frame = referenceFrame->key_frame;
#endif

    return frame;
}

} // namespace EnhanceVision
