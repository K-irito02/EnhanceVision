/**
 * @file ImageUtils.cpp
 * @brief 图像工具类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/ImageUtils.h"
#include <QFileInfo>
#include <QImageReader>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

namespace EnhanceVision {

ImageUtils::ImageUtils(QObject *parent)
    : QObject(parent)
{
}

ImageUtils::~ImageUtils()
{
}

QImage ImageUtils::generateThumbnail(const QString &imagePath, const QSize &size)
{
    QImage image = loadImage(imagePath);
    if (image.isNull()) {
        return QImage();
    }
    return scaleImage(image, size, true);
}

QImage ImageUtils::generateVideoThumbnail(const QString &videoPath, const QSize &size)
{
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* frameRGB = nullptr;
    uint8_t* buffer = nullptr;
    struct SwsContext* swsCtx = nullptr;
    
    QByteArray pathBytes = videoPath.toUtf8();
    
    if (avformat_open_input(&formatCtx, pathBytes.constData(), nullptr, nullptr) != 0) {
        return QImage();
    }
    
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    int videoStreamIdx = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            break;
        }
    }
    
    if (videoStreamIdx == -1) {
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    AVCodecParameters* codecPar = formatCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    if (avcodec_parameters_to_context(codecCtx, codecPar) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    frame = av_frame_alloc();
    frameRGB = av_frame_alloc();
    if (!frame || !frameRGB) {
        if (frame) av_frame_free(&frame);
        if (frameRGB) av_frame_free(&frameRGB);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    int maxWidth = size.width() > 0 ? size.width() : 256;
    int maxHeight = size.height() > 0 ? size.height() : 256;
    
    int originalWidth = codecCtx->width;
    int originalHeight = codecCtx->height;
    
    int targetWidth, targetHeight;
    if (originalWidth > 0 && originalHeight > 0) {
        double aspectRatio = static_cast<double>(originalWidth) / originalHeight;
        if (aspectRatio > static_cast<double>(maxWidth) / maxHeight) {
            targetWidth = maxWidth;
            targetHeight = static_cast<int>(maxWidth / aspectRatio);
        } else {
            targetHeight = maxHeight;
            targetWidth = static_cast<int>(maxHeight * aspectRatio);
        }
    } else {
        targetWidth = maxWidth;
        targetHeight = maxHeight;
    }
    
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, targetWidth, targetHeight, 1);
    buffer = static_cast<uint8_t*>(av_malloc(numBytes * sizeof(uint8_t)));
    if (!buffer) {
        av_frame_free(&frame);
        av_frame_free(&frameRGB);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, 
                         AV_PIX_FMT_RGB24, targetWidth, targetHeight, 1);
    
    swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        targetWidth, targetHeight, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!swsCtx) {
        av_free(buffer);
        av_frame_free(&frame);
        av_frame_free(&frameRGB);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        return QImage();
    }
    
    AVPacket* packet = av_packet_alloc();
    bool frameFound = false;
    
    while (av_read_frame(formatCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIdx) {
            int ret = avcodec_send_packet(codecCtx, packet);
            if (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == 0) {
                    frameFound = true;
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    
    QImage result;
    
    if (frameFound) {
        sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height,
                  frameRGB->data, frameRGB->linesize);
        
        result = QImage(targetWidth, targetHeight, QImage::Format_RGB888);
        for (int y = 0; y < targetHeight; y++) {
            memcpy(result.scanLine(y), frameRGB->data[0] + y * frameRGB->linesize[0], targetWidth * 3);
        }
    }
    
    sws_freeContext(swsCtx);
    av_free(buffer);
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);
    
    return result;
}

QImage ImageUtils::scaleImage(const QImage &image, const QSize &size, bool keepAspectRatio)
{
    if (image.isNull()) {
        return QImage();
    }
    
    QSize targetSize = size;
    if (keepAspectRatio) {
        targetSize = image.size().scaled(size, Qt::KeepAspectRatio);
    }
    
    return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage ImageUtils::loadImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoDetectImageFormat(true);
    return reader.read();
}

bool ImageUtils::isImageFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    static const QStringList imageExtensions = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), 
        QStringLiteral("png"), QStringLiteral("bmp"), 
        QStringLiteral("webp"), QStringLiteral("tiff"),
        QStringLiteral("tif")
    };
    
    return imageExtensions.contains(suffix);
}

bool ImageUtils::isVideoFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    static const QStringList videoExtensions = {
        QStringLiteral("mp4"), QStringLiteral("avi"), 
        QStringLiteral("mkv"), QStringLiteral("mov"), 
        QStringLiteral("flv")
    };
    
    return videoExtensions.contains(suffix);
}

} // namespace EnhanceVision
