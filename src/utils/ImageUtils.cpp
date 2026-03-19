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

QImage ImageUtils::applyShaderEffects(const QImage &image,
                                       float brightness,
                                       float contrast,
                                       float saturation,
                                       float hue,
                                       float exposure,
                                       float gamma,
                                       float temperature,
                                       float tint,
                                       float vignette,
                                       float highlights,
                                       float shadows,
                                       float sharpness,
                                       float blur,
                                       float denoise)
{
    if (image.isNull()) {
        return QImage();
    }

    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    QImage original = result.copy();

    int width = result.width();
    int height = result.height();
    int centerX = width / 2;
    int centerY = height / 2;
    float maxDist = std::sqrt(centerX * centerX + centerY * centerY);

    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            int a = qAlpha(line[x]);

            if (qAbs(exposure) > 0.001f) {
                r = r * std::pow(2.0f, exposure);
                g = g * std::pow(2.0f, exposure);
                b = b * std::pow(2.0f, exposure);
            }

            if (qAbs(brightness) > 0.001f) {
                r = qBound(0.0f, r + brightness, 1.0f);
                g = qBound(0.0f, g + brightness, 1.0f);
                b = qBound(0.0f, b + brightness, 1.0f);
            }

            if (qAbs(contrast - 1.0f) > 0.001f) {
                r = qBound(0.0f, (r - 0.5f) * contrast + 0.5f, 1.0f);
                g = qBound(0.0f, (g - 0.5f) * contrast + 0.5f, 1.0f);
                b = qBound(0.0f, (b - 0.5f) * contrast + 0.5f, 1.0f);
            }

            if (qAbs(saturation - 1.0f) > 0.001f) {
                float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r = qBound(0.0f, gray + saturation * (r - gray), 1.0f);
                g = qBound(0.0f, gray + saturation * (g - gray), 1.0f);
                b = qBound(0.0f, gray + saturation * (b - gray), 1.0f);
            }

            if (qAbs(hue) > 0.001f) {
                float h, s, v;
                rgbToHsv(r, g, b, h, s, v);
                h = h + hue;
                if (h < 0.0f) h += 1.0f;
                if (h > 1.0f) h -= 1.0f;
                hsvToRgb(h, s, v, r, g, b);
            }

            if (qAbs(gamma - 1.0f) > 0.001f) {
                float invGamma = 1.0f / gamma;
                r = std::pow(r, invGamma);
                g = std::pow(g, invGamma);
                b = std::pow(b, invGamma);
            }

            if (qAbs(temperature) > 0.001f) {
                r = qBound(0.0f, r + temperature * 0.1f, 1.0f);
                b = qBound(0.0f, b - temperature * 0.1f, 1.0f);
            }

            if (qAbs(tint) > 0.001f) {
                g = qBound(0.0f, g + tint * 0.1f, 1.0f);
            }

            if (qAbs(highlights) > 0.001f) {
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance > 0.5f) {
                    float factor = (luminance - 0.5f) * 2.0f;
                    float adjustment = highlights * factor * 0.2f;
                    r = qBound(0.0f, r + adjustment, 1.0f);
                    g = qBound(0.0f, g + adjustment, 1.0f);
                    b = qBound(0.0f, b + adjustment, 1.0f);
                }
            }

            if (qAbs(shadows) > 0.001f) {
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance < 0.5f) {
                    float factor = (0.5f - luminance) * 2.0f;
                    float adjustment = shadows * factor * 0.2f;
                    r = qBound(0.0f, r + adjustment, 1.0f);
                    g = qBound(0.0f, g + adjustment, 1.0f);
                    b = qBound(0.0f, b + adjustment, 1.0f);
                }
            }

            if (vignette > 0.001f) {
                float dx = x - centerX;
                float dy = y - centerY;
                float dist = std::sqrt(dx * dx + dy * dy) / maxDist;
                float vignetteFactor = 1.0f - vignette * dist * dist;
                vignetteFactor = qBound(0.0f, vignetteFactor, 1.0f);
                r *= vignetteFactor;
                g *= vignetteFactor;
                b *= vignetteFactor;
            }

            line[x] = qRgba(static_cast<int>(qBound(0.0f, r, 1.0f) * 255),
                           static_cast<int>(qBound(0.0f, g, 1.0f) * 255),
                           static_cast<int>(qBound(0.0f, b, 1.0f) * 255),
                           a);
        }
    }

    if (denoise > 0.01f || blur > 0.01f || sharpness > 0.01f) {
        QImage temp = result.copy();

        for (int y = 0; y < height; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < width; ++x) {
                float r = qRed(line[x]) / 255.0f;
                float g = qGreen(line[x]) / 255.0f;
                float b = qBlue(line[x]) / 255.0f;
                int a = qAlpha(line[x]);

                if (denoise > 0.01f) {
                    QVector<float> reds, greens, blues;

                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = qBound(0, x + dx, width - 1);
                            int ny = qBound(0, y + dy, height - 1);
                            QRgb neighbor = temp.pixel(nx, ny);
                            reds.append(qRed(neighbor) / 255.0f);
                            greens.append(qGreen(neighbor) / 255.0f);
                            blues.append(qBlue(neighbor) / 255.0f);
                        }
                    }

                    std::sort(reds.begin(), reds.end());
                    std::sort(greens.begin(), greens.end());
                    std::sort(blues.begin(), blues.end());

                    int mid = reds.size() / 2;
                    float medianR = reds[mid];
                    float medianG = greens[mid];
                    float medianB = blues[mid];

                    r = r * (1.0f - denoise * 0.5f) + medianR * denoise * 0.5f;
                    g = g * (1.0f - denoise * 0.5f) + medianG * denoise * 0.5f;
                    b = b * (1.0f - denoise * 0.5f) + medianB * denoise * 0.5f;
                }

                if (blur > 0.01f) {
                    float sumR = 0, sumG = 0, sumB = 0, sumWeight = 0;

                    for (int dy = -2; dy <= 2; ++dy) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            int nx = qBound(0, x + dx, width - 1);
                            int ny = qBound(0, y + dy, height - 1);
                            QRgb neighbor = temp.pixel(nx, ny);
                            float weight = 1.0f - std::sqrt(dx*dx + dy*dy) / 3.0f;
                            weight = qMax(0.0f, weight);
                            sumR += qRed(neighbor) / 255.0f * weight;
                            sumG += qGreen(neighbor) / 255.0f * weight;
                            sumB += qBlue(neighbor) / 255.0f * weight;
                            sumWeight += weight;
                        }
                    }

                    if (sumWeight > 0) {
                        float blurR = sumR / sumWeight;
                        float blurG = sumG / sumWeight;
                        float blurB = sumB / sumWeight;
                        r = r * (1.0f - blur * 0.5f) + blurR * blur * 0.5f;
                        g = g * (1.0f - blur * 0.5f) + blurG * blur * 0.5f;
                        b = b * (1.0f - blur * 0.5f) + blurB * blur * 0.5f;
                    }
                }

                if (sharpness > 0.01f) {
                    float blurR = 0, blurG = 0, blurB = 0;
                    blurR += qRed(original.pixel(qBound(0, x - 1, width - 1), y)) / 255.0f;
                    blurR += qRed(original.pixel(qBound(0, x + 1, width - 1), y)) / 255.0f;
                    blurR += qRed(original.pixel(x, qBound(0, y - 1, height - 1))) / 255.0f;
                    blurR += qRed(original.pixel(x, qBound(0, y + 1, height - 1))) / 255.0f;
                    blurR /= 4.0f;

                    blurG += qGreen(original.pixel(qBound(0, x - 1, width - 1), y)) / 255.0f;
                    blurG += qGreen(original.pixel(qBound(0, x + 1, width - 1), y)) / 255.0f;
                    blurG += qGreen(original.pixel(x, qBound(0, y - 1, height - 1))) / 255.0f;
                    blurG += qGreen(original.pixel(x, qBound(0, y + 1, height - 1))) / 255.0f;
                    blurG /= 4.0f;

                    blurB += qBlue(original.pixel(qBound(0, x - 1, width - 1), y)) / 255.0f;
                    blurB += qBlue(original.pixel(qBound(0, x + 1, width - 1), y)) / 255.0f;
                    blurB += qBlue(original.pixel(x, qBound(0, y - 1, height - 1))) / 255.0f;
                    blurB += qBlue(original.pixel(x, qBound(0, y + 1, height - 1))) / 255.0f;
                    blurB /= 4.0f;

                    float origR = qRed(original.pixel(x, y)) / 255.0f;
                    float origG = qGreen(original.pixel(x, y)) / 255.0f;
                    float origB = qBlue(original.pixel(x, y)) / 255.0f;

                    float sharpenR = origR - blurR;
                    float sharpenG = origG - blurG;
                    float sharpenB = origB - blurB;

                    r = qBound(0.0f, r + sharpness * sharpenR, 1.0f);
                    g = qBound(0.0f, g + sharpness * sharpenG, 1.0f);
                    b = qBound(0.0f, b + sharpness * sharpenB, 1.0f);
                }

                line[x] = qRgba(static_cast<int>(qBound(0.0f, r, 1.0f) * 255),
                               static_cast<int>(qBound(0.0f, g, 1.0f) * 255),
                               static_cast<int>(qBound(0.0f, b, 1.0f) * 255),
                               a);
            }
        }
    }

    return result;
}

void ImageUtils::rgbToHsv(float r, float g, float b, float &h, float &s, float &v)
{
    float max = std::max({r, g, b});
    float min = std::min({r, g, b});
    float delta = max - min;
    
    v = max;
    
    if (delta < 0.00001f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }
    
    s = delta / max;
    
    if (max == r) {
        h = (g - b) / delta;
        if (g < b) h += 6.0f;
    } else if (max == g) {
        h = 2.0f + (b - r) / delta;
    } else {
        h = 4.0f + (r - g) / delta;
    }
    
    h /= 6.0f;
}

void ImageUtils::hsvToRgb(float h, float s, float v, float &r, float &g, float &b)
{
    if (s < 0.00001f) {
        r = g = b = v;
        return;
    }
    
    h *= 6.0f;
    int i = static_cast<int>(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
}

} // namespace EnhanceVision
