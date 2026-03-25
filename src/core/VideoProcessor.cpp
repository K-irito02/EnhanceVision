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
    ImageProcessor imageProcessor;

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

        // 查找视频流
        int videoStreamIndex = -1;
        for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
            if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
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

        // 打开输出文件
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE) < 0) {
                throw std::runtime_error(tr("无法打开输出文件").toStdString());
            }
        }

        // 写文件头
        avformat_write_header(outputFormatContext, nullptr);

        // 初始化图像转换上下文
        swsContext = sws_getContext(
            videoDecoderContext->width, videoDecoderContext->height, videoDecoderContext->pix_fmt,
            videoEncoderContext->width, videoEncoderContext->height, videoEncoderContext->pix_fmt,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        // 分配帧
        AVFrame* frame = av_frame_alloc();
        AVFrame* outFrame = av_frame_alloc();
        outFrame->format = videoEncoderContext->pix_fmt;
        outFrame->width = videoEncoderContext->width;
        outFrame->height = videoEncoderContext->height;
        av_frame_get_buffer(outFrame, 32);

        AVPacket* packet = av_packet_alloc();

        // 获取总帧数（估算）
        int64_t totalFrames = inputFormatContext->streams[videoStreamIndex]->nb_frames;
        if (totalFrames <= 0) {
            totalFrames = inputFormatContext->duration * inputFormatContext->streams[videoStreamIndex]->time_base.num /
                           inputFormatContext->streams[videoStreamIndex]->time_base.den *
                           videoDecoderContext->framerate.num / videoDecoderContext->framerate.den;
        }
        if (totalFrames <= 0) totalFrames = 1000;

        int64_t frameCount = 0;

        if (progressCallback) {
            progressCallback(10, tr("开始处理视频帧..."));
        }
        emit progressChanged(10, tr("开始处理视频帧..."));

        // 解码、处理、编码循环
        while (av_read_frame(inputFormatContext, packet) >= 0) {
            if (m_cancelled) {
                throw std::runtime_error(tr("处理已取消").toStdString());
            }

            if (packet->stream_index == videoStreamIndex) {
                avcodec_send_packet(videoDecoderContext, packet);

                while (avcodec_receive_frame(videoDecoderContext, frame) == 0) {
                    if (m_cancelled) {
                        throw std::runtime_error(tr("处理已取消").toStdString());
                    }

                    // 将 AVFrame 转换为 QImage
                    QImage image = frameToImage(frame, videoDecoderContext->pix_fmt);

                    // 应用滤镜
                    QImage processedImage = imageProcessor.applyShader(image, params);

                    // 将 QImage 转换回 AVFrame
                    AVFrame* processedFrame = imageToFrame(processedImage, frame);

                    // 转换为编码器像素格式
                    sws_scale(swsContext,
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
                    int progress = 10 + qRound(80.0 * frameCount / totalFrames);
                    progress = qMin(progress, 90);

                    if (progressCallback) {
                        progressCallback(progress, tr("正在处理第 %1 帧...").arg(frameCount));
                    }
                    emit progressChanged(progress, tr("正在处理第 %1 帧...").arg(frameCount));
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

    } catch (const std::exception& e) {
        error = QString::fromStdString(e.what());
        success = false;
    }

    // 清理资源
    if (swsContext) sws_freeContext(swsContext);
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

    QImage rgbImage = image.convertToFormat(QImage::Format_RGB32);

    AVFrame* frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_RGB32;
    frame->width = rgbImage.width();
    frame->height = rgbImage.height();
    av_frame_get_buffer(frame, 32);

    SwsContext* swsCtx = sws_getContext(
        rgbImage.width(), rgbImage.height(), AV_PIX_FMT_RGB32,
        rgbImage.width(), rgbImage.height(), AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    uint8_t* srcData[4] = { const_cast<uint8_t*>(rgbImage.bits()), nullptr, nullptr, nullptr };
    int srcLinesize[4] = { rgbImage.bytesPerLine(), 0, 0, 0 };

    sws_scale(swsCtx, srcData, srcLinesize, 0, rgbImage.height(), frame->data, frame->linesize);
    sws_freeContext(swsCtx);

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
