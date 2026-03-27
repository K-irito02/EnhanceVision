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
 */

#include "EnhanceVision/core/VideoProcessor.h"
#include "EnhanceVision/core/ImageProcessor.h"
#include <QtConcurrent/QtConcurrent>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

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

    try {
        // ========== 阶段1: 打开输入文件并分析流 ==========
        if (progressCallback) {
            progressCallback(2, tr("正在分析视频文件..."));
        }
        emit progressChanged(2, tr("正在分析视频文件..."));

        if (avformat_open_input(&inputFormatContext, inputPath.toUtf8().constData(), nullptr, nullptr) != 0) {
            throw std::runtime_error(tr("无法打开输入视频文件").toStdString());
        }

        if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
            throw std::runtime_error(tr("无法获取视频流信息").toStdString());
        }

        // 查找视频流和音频流
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

        // ========== 阶段2: 初始化视频解码器 ==========
        if (progressCallback) {
            progressCallback(5, tr("正在初始化解码器..."));
        }
        emit progressChanged(5, tr("正在初始化解码器..."));

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

        // 设置多线程解码
        videoDecoderContext->thread_count = qMin(QThread::idealThreadCount(), 8);

        if (avcodec_open2(videoDecoderContext, decoder, nullptr) < 0) {
            throw std::runtime_error(tr("无法打开视频解码器").toStdString());
        }

        int srcWidth = videoDecoderContext->width;
        int srcHeight = videoDecoderContext->height;
        AVPixelFormat srcPixFmt = videoDecoderContext->pix_fmt;

        qDebug() << "[VideoProcessor] Input:" << srcWidth << "x" << srcHeight 
                 << "PixFmt:" << av_get_pix_fmt_name(srcPixFmt);

        // ========== 阶段3: 初始化输出格式和编码器 ==========
        if (progressCallback) {
            progressCallback(8, tr("正在初始化编码器..."));
        }
        emit progressChanged(8, tr("正在初始化编码器..."));

        if (avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, 
                                            outputPath.toUtf8().constData()) < 0) {
            throw std::runtime_error(tr("无法创建输出格式上下文").toStdString());
        }

        // 创建输出视频流
        AVStream* outVideoStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outVideoStream) {
            throw std::runtime_error(tr("无法创建输出视频流").toStdString());
        }
        outVideoStreamIndex = outVideoStream->index;

        // 选择编码器：优先H.264
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

        // 配置编码器参数
        videoEncoderContext->width = srcWidth;
        videoEncoderContext->height = srcHeight;
        videoEncoderContext->sample_aspect_ratio = videoDecoderContext->sample_aspect_ratio;
        videoEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;
        
        // 使用输入视频的时间基和帧率
        if (inVideoStream->avg_frame_rate.num > 0 && inVideoStream->avg_frame_rate.den > 0) {
            videoEncoderContext->framerate = inVideoStream->avg_frame_rate;
        } else if (videoDecoderContext->framerate.num > 0 && videoDecoderContext->framerate.den > 0) {
            videoEncoderContext->framerate = videoDecoderContext->framerate;
        } else {
            videoEncoderContext->framerate = AVRational{25, 1};
        }
        videoEncoderContext->time_base = av_inv_q(videoEncoderContext->framerate);

        // 高质量编码设置
        videoEncoderContext->bit_rate = static_cast<int64_t>(srcWidth) * srcHeight * 4;
        videoEncoderContext->gop_size = 12;
        videoEncoderContext->max_b_frames = 2;
        videoEncoderContext->thread_count = qMin(QThread::idealThreadCount(), 8);

        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
            videoEncoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // 设置编码质量
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

        // ========== 阶段4: 复制音频流（如果存在） ==========
        AVStream* outAudioStream = nullptr;
        if (audioStreamIndex >= 0) {
            AVStream* inAudioStream = inputFormatContext->streams[audioStreamIndex];
            outAudioStream = avformat_new_stream(outputFormatContext, nullptr);
            if (outAudioStream) {
                outAudioStreamIndex = outAudioStream->index;
                if (avcodec_parameters_copy(outAudioStream->codecpar, inAudioStream->codecpar) >= 0) {
                    outAudioStream->time_base = inAudioStream->time_base;
                    qDebug() << "[VideoProcessor] Audio stream will be copied";
                } else {
                    outAudioStreamIndex = -1;
                    qWarning() << "[VideoProcessor] Failed to copy audio parameters";
                }
            }
        }

        // ========== 阶段5: 打开输出文件 ==========
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error(tr("无法打开输出文件").toStdString());
            }
        }

        if (avformat_write_header(outputFormatContext, nullptr) < 0) {
            throw std::runtime_error(tr("无法写入文件头").toStdString());
        }

        // ========== 阶段6: 初始化图像转换上下文 ==========
        // 解码后 -> RGB32 (用于 QImage 处理)
        swsContextToRGB = sws_getContext(
            srcWidth, srcHeight, srcPixFmt,
            srcWidth, srcHeight, AV_PIX_FMT_RGB32,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!swsContextToRGB) {
            throw std::runtime_error(tr("无法创建RGB转换上下文").toStdString());
        }

        // RGB32 -> YUV420P (用于编码)
        swsContextToYUV = sws_getContext(
            srcWidth, srcHeight, AV_PIX_FMT_RGB32,
            srcWidth, srcHeight, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!swsContextToYUV) {
            throw std::runtime_error(tr("无法创建YUV转换上下文").toStdString());
        }

        // ========== 阶段7: 分配帧缓冲 ==========
        frame = av_frame_alloc();
        rgbFrame = av_frame_alloc();
        outFrame = av_frame_alloc();
        packet = av_packet_alloc();

        if (!frame || !rgbFrame || !outFrame || !packet) {
            throw std::runtime_error(tr("无法分配帧缓冲").toStdString());
        }

        // 配置RGB帧
        rgbFrame->format = AV_PIX_FMT_RGB32;
        rgbFrame->width = srcWidth;
        rgbFrame->height = srcHeight;
        if (av_frame_get_buffer(rgbFrame, 32) < 0) {
            throw std::runtime_error(tr("无法分配RGB帧缓冲").toStdString());
        }

        // 配置输出帧
        outFrame->format = AV_PIX_FMT_YUV420P;
        outFrame->width = srcWidth;
        outFrame->height = srcHeight;
        if (av_frame_get_buffer(outFrame, 32) < 0) {
            throw std::runtime_error(tr("无法分配输出帧缓冲").toStdString());
        }

        // ========== 阶段8: 估算总帧数 ==========
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

        qDebug() << "[VideoProcessor] Estimated frames:" << totalFrames;

        // ========== 阶段9: 主处理循环 ==========
        if (progressCallback) {
            progressCallback(10, tr("开始处理视频帧..."));
        }
        emit progressChanged(10, tr("开始处理视频帧..."));

        int64_t frameCount = 0;
        int64_t encodedPts = 0;

        while (av_read_frame(inputFormatContext, packet) >= 0) {
            if (m_cancelled) {
                av_packet_unref(packet);
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            if (packet->stream_index == videoStreamIndex) {
                // 视频帧处理
                int ret = avcodec_send_packet(videoDecoderContext, packet);
                if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                    qWarning() << "[VideoProcessor] Decode send packet error:" << ret;
                }

                while (avcodec_receive_frame(videoDecoderContext, frame) == 0) {
                    if (m_cancelled) {
                        throw std::runtime_error(tr("处理已取消").toStdString());
                    }

                    // 转换为RGB32
                    sws_scale(swsContextToRGB,
                              frame->data, frame->linesize, 0, frame->height,
                              rgbFrame->data, rgbFrame->linesize);

                    // 创建QImage（不复制数据，直接使用rgbFrame的缓冲）
                    QImage image(rgbFrame->data[0], srcWidth, srcHeight, 
                                 rgbFrame->linesize[0], QImage::Format_RGB32);
                    
                    // 应用Shader效果（使用与GPU完全一致的算法）
                    QImage processedImage = imageProcessor.applyShader(image, params);

                    // 将处理后的图像数据复制回rgbFrame
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

                    // 转换为YUV420P用于编码
                    sws_scale(swsContextToYUV,
                              rgbFrame->data, rgbFrame->linesize, 0, srcHeight,
                              outFrame->data, outFrame->linesize);

                    // 设置PTS
                    outFrame->pts = encodedPts++;

                    // 编码
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
                    
                    // 更新进度（每10帧或达到特定百分比时）
                    if (frameCount % 10 == 0 || frameCount == 1) {
                        int progress = 10 + qRound(80.0 * frameCount / totalFrames);
                        progress = qBound(10, progress, 90);

                        if (progressCallback) {
                            progressCallback(progress, tr("正在处理第 %1 帧...").arg(frameCount));
                        }
                        emit progressChanged(progress, tr("正在处理第 %1 帧...").arg(frameCount));
                    }
                }
            } else if (packet->stream_index == audioStreamIndex && outAudioStreamIndex >= 0) {
                // 复制音频包
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

        // ========== 阶段10: 刷新编码器 ==========
        if (progressCallback) {
            progressCallback(92, tr("正在完成编码..."));
        }
        emit progressChanged(92, tr("正在完成编码..."));

        avcodec_send_frame(videoEncoderContext, nullptr);
        AVPacket* outPacket = av_packet_alloc();
        while (avcodec_receive_packet(videoEncoderContext, outPacket) == 0) {
            av_packet_rescale_ts(outPacket, videoEncoderContext->time_base, outVideoStream->time_base);
            outPacket->stream_index = outVideoStreamIndex;
            av_interleaved_write_frame(outputFormatContext, outPacket);
            av_packet_unref(outPacket);
        }
        av_packet_free(&outPacket);

        // ========== 阶段11: 写入文件尾 ==========
        if (progressCallback) {
            progressCallback(96, tr("正在写入文件..."));
        }
        emit progressChanged(96, tr("正在写入文件..."));

        if (av_write_trailer(outputFormatContext) < 0) {
            throw std::runtime_error(tr("无法写入文件尾").toStdString());
        }

        resultPath = outputPath;
        success = true;

        qDebug() << "[VideoProcessor] Completed. Processed" << frameCount << "frames";

        if (progressCallback) {
            progressCallback(100, tr("处理完成"));
        }
        emit progressChanged(100, tr("处理完成"));

    } catch (const std::exception& e) {
        error = QString::fromStdString(e.what());
        success = false;
        qWarning() << "[VideoProcessor] Error:" << error;
        
        // 删除不完整的输出文件
        if (!success && QFile::exists(outputPath)) {
            QFile::remove(outputPath);
        }
    }

    cleanup();
    m_isProcessing = false;

    if (finishCallback) {
        finishCallback(success, resultPath, error);
    }
    emit finished(success, resultPath, error);
}

} // namespace EnhanceVision
