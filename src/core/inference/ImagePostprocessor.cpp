/**
 * @file ImagePostprocessor.cpp
 * @brief 图像后处理器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/inference/ImagePostprocessor.h"
#include <QDebug>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <net.h>

namespace EnhanceVision {

ImagePostprocessor::ImagePostprocessor() {}
ImagePostprocessor::~ImagePostprocessor() {}

QImage ImagePostprocessor::matToQimage(const ncnn::Mat& mat, const ModelInfo& model)
{
    if (mat.empty() || mat.w <= 0 || mat.h <= 0 || mat.c <= 0 || mat.data == nullptr) {
        qWarning() << "[ImagePostprocessor] received empty/invalid mat";
        return QImage();
    }

    const int w = mat.w, h = mat.h;
    ncnn::Mat out = mat.clone();
    if (out.empty() || out.data == nullptr) {
        qWarning() << "[ImagePostprocessor] mat.clone() produced empty mat";
        return QImage();
    }

    out.substract_mean_normalize(model.denormMean, model.denormScale);
    float* data = static_cast<float*>(out.data);
    const int total = w * h * out.c;
    for (int i = 0; i < total; ++i) {
        data[i] = std::clamp(data[i], 0.f, 255.f);
    }

    // 32 字节对齐 stride（参考 cpu-video-inference-crash-fix-notes）
    const int compactStride = w * 3;
    const int alignedStride = (compactStride + 31) & ~31;
    const size_t bufferSize = static_cast<size_t>(alignedStride) * h;
    std::vector<unsigned char> buffer(bufferSize);

    out.to_pixels(buffer.data(), ncnn::Mat::PIXEL_RGB, alignedStride);

    QImage result(w, h, QImage::Format_RGB888);
    if (result.isNull()) {
        qWarning() << "[ImagePostprocessor] failed to create QImage";
        return QImage();
    }

    for (int y = 0; y < h; ++y) {
        std::memcpy(result.scanLine(y), buffer.data() + y * alignedStride, compactStride);
    }

    return result;
}

void ImagePostprocessor::denormalize(ncnn::Mat& mat, const float mean[3], const float scale[3])
{
    mat.substract_mean_normalize(mean, scale);
}

void ImagePostprocessor::clampValues(ncnn::Mat& mat)
{
    float* data = static_cast<float*>(mat.data);
    int total = mat.w * mat.h * mat.c;
    for (int i = 0; i < total; ++i) {
        data[i] = std::clamp(data[i], 0.f, 255.f);
    }
}

QImage ImagePostprocessor::mergeTiles(const std::vector<QImage>& tiles,
                                       const std::vector<TileInfo>& tileInfos,
                                       const QSize& outputSize,
                                       int scale)
{
    Q_UNUSED(tiles); Q_UNUSED(tileInfos); Q_UNUSED(outputSize); Q_UNUSED(scale);
    qWarning() << "[ImagePostprocessor] mergeTiles not yet implemented";
    return QImage();
}

QRect ImagePostprocessor::computeTileOutputRect(const TileInfo& tileInfo, int scale)
{
    Q_UNUSED(tileInfo); Q_UNUSED(scale);
    return QRect();
}

QImage ImagePostprocessor::mergeTTAResults(const std::vector<QImage>& results)
{
    if (results.empty()) return QImage();

    int w = results[0].width();
    int h = results[0].height();
    QImage merged(w, h, QImage::Format_RGB888);
    merged.fill(Qt::black);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sumR = 0, sumG = 0, sumB = 0, count = 0;
            for (const auto& img : results) {
                if (x < img.width() && y < img.height()) {
                    QRgb pixel = img.pixel(x, y);
                    sumR += qRed(pixel);
                    sumG += qGreen(pixel);
                    sumB += qBlue(pixel);
                    count++;
                }
            }
            if (count > 0) {
                merged.setPixel(x, y, qRgb(sumR / count, sumG / count, sumB / count));
            }
        }
    }

    return merged;
}

QImage ImagePostprocessor::applyInverseTTATransform(const QImage& image, int transformIndex)
{
    Q_UNUSED(transformIndex);
    return image.mirrored(false, true).copy();
}

QImage ImagePostprocessor::applyOutscale(const QImage& image, double scale)
{
    if (image.isNull() || scale <= 0) return image;
    if (std::abs(scale - 1.0) < 0.001) return image;

    int newWidth = static_cast<int>(image.width() * scale);
    int newHeight = static_cast<int>(image.height() * scale);
    return image.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QSize ImagePostprocessor::computeOutputSize(const QSize& inputSize, int modelScale, double outscale)
{
    int w = inputSize.width() * modelScale;
    int h = inputSize.height() * modelScale;
    if (std::abs(outscale - 1.0) > 0.001) {
        w = static_cast<int>(w * outscale);
        h = static_cast<int>(h * outscale);
    }
    return QSize(w, h);
}

QRgb ImagePostprocessor::averagePixels(const std::vector<QRgb>& pixels)
{
    if (pixels.empty()) return qRgb(0, 0, 0);

    int sumR = 0, sumG = 0, sumB = 0;
    for (QRgb p : pixels) {
        sumR += qRed(p);
        sumG += qGreen(p);
        sumB += qBlue(p);
    }
    int n = static_cast<int>(pixels.size());
    return qRgb(sumR / n, sumG / n, sumB / n);
}

} // namespace EnhanceVision
