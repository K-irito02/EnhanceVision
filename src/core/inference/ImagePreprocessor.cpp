/**
 * @file ImagePreprocessor.cpp
 * @brief 图像预处理器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/inference/ImagePreprocessor.h"
#include <QDebug>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <net.h>

namespace EnhanceVision {

ImagePreprocessor::ImagePreprocessor()
{
}

ImagePreprocessor::~ImagePreprocessor()
{
}

ncnn::Mat ImagePreprocessor::qimageToMat(const QImage& image, const ModelInfo& model)
{
    QImage img = convertToRGB888(image);
    
    if (img.isNull() || img.width() <= 0 || img.height() <= 0) {
        qWarning() << "[ImagePreprocessor] Invalid image";
        return ncnn::Mat();
    }
    
    int w = img.width();
    int h = img.height();
    const int stride = img.bytesPerLine();
    
    ncnn::Mat in = ncnn::Mat::from_pixels(img.constBits(), ncnn::Mat::PIXEL_RGB, w, h, stride);
    
    if (in.empty()) {
        qWarning() << "[ImagePreprocessor] Failed to create ncnn::Mat from image";
        return ncnn::Mat();
    }
    
    normalize(in, model.normMean, model.normScale);
    
    return in;
}

QImage ImagePreprocessor::convertToRGB888(const QImage& image)
{
    if (image.format() == QImage::Format_RGB888) {
        return image;
    }
    
    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    if (converted.isNull()) {
        qWarning() << "[ImagePreprocessor] Failed to convert image to RGB888";
    }
    return converted;
}

void ImagePreprocessor::normalize(ncnn::Mat& mat, const float mean[3], const float scale[3])
{
    mat.substract_mean_normalize(mean, scale);
}

std::vector<TileInfo> ImagePreprocessor::computeTiles(const QSize& imageSize, int tileSize)
{
    std::vector<TileInfo> tiles;
    
    if (tileSize <= 0) {
        return tiles;
    }
    
    int w = imageSize.width();
    int h = imageSize.height();
    
    int tilesX = (w + tileSize - 1) / tileSize;
    int tilesY = (h + tileSize - 1) / tileSize;
    int totalTiles = tilesX * tilesY;
    
    int index = 0;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            TileInfo info;
            info.x = tx * tileSize;
            info.y = ty * tileSize;
            info.width = std::min(tileSize, w - info.x);
            info.height = std::min(tileSize, h - info.y);
            info.index = index;
            info.total = totalTiles;
            tiles.push_back(info);
            ++index;
        }
    }
    
    return tiles;
}

QImage ImagePreprocessor::extractTile(const QImage& image, const TileInfo& tileInfo, int padding)
{
    int extractX = tileInfo.x;
    int extractY = tileInfo.y;
    int extractW = tileInfo.width + 2 * padding;
    int extractH = tileInfo.height + 2 * padding;
    
    return image.copy(extractX, extractY, extractW, extractH);
}

QImage ImagePreprocessor::createPaddedImage(const QImage& image, int padding)
{
    if (padding <= 0) {
        return image;
    }
    
    int w = image.width();
    int h = image.height();
    
    QImage normalizedInput = convertToRGB888(image);
    if (normalizedInput.isNull()) {
        qWarning() << "[ImagePreprocessor] Failed to normalize input format";
        return QImage();
    }
    
    QImage paddedImage(w + 2 * padding, h + 2 * padding, QImage::Format_RGB888);
    if (paddedImage.isNull()) {
        qWarning() << "[ImagePreprocessor] Failed to create padded image";
        return QImage();
    }
    paddedImage.fill(Qt::black);
    
    const int srcBytesPerLine = normalizedInput.bytesPerLine();
    const int expectedBytesPerLine = w * 3;
    
    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = normalizedInput.constScanLine(y);
        uchar* dstLine = paddedImage.scanLine(y + padding);
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    
    applyMirrorPadding(paddedImage, normalizedInput, padding);
    
    return paddedImage;
}

int ImagePreprocessor::computeOptimalTileSize(const QSize& imageSize, int modelTileSize, qint64 maxMemoryMB)
{
    if (modelTileSize <= 0) {
        return 0;
    }
    
    int w = imageSize.width();
    int h = imageSize.height();
    
    if (w <= modelTileSize && h <= modelTileSize) {
        return 0;
    }
    
    constexpr qint64 kBytesPerMB = 1024LL * 1024;
    const int channels = 3;
    const int scale = 4;
    
    auto memForTile = [&](int tile) -> double {
        const qint64 px = static_cast<qint64>(tile) * tile;
        const qint64 bytes = px * channels * static_cast<qint64>(sizeof(float)) * static_cast<qint64>(scale * scale);
        double kFactor = 8.0;
        return static_cast<double>(bytes) * kFactor / kBytesPerMB;
    };
    
    const int maxPossibleTile = std::max(w, h);
    const int minTile = 64;
    const int maxTile = std::min(512, maxPossibleTile);
    const int step = 32;
    
    int bestTile = minTile;
    for (int t = maxTile; t >= minTile; t -= step) {
        if (memForTile(t) <= static_cast<double>(maxMemoryMB)) {
            bestTile = t;
            break;
        }
    }
    
    if (w <= bestTile && h <= bestTile) {
        return 0;
    }
    
    return bestTile;
}

bool ImagePreprocessor::needsTiling(const QSize& imageSize, int tileSize)
{
    if (tileSize <= 0) {
        return false;
    }
    
    return imageSize.width() > tileSize || imageSize.height() > tileSize;
}

std::vector<QImage> ImagePreprocessor::generateTTATransforms(const QImage& image)
{
    std::vector<QImage> transforms;
    transforms.reserve(8);
    
    transforms.push_back(image);
    transforms.push_back(image.mirrored(true, false));
    transforms.push_back(image.mirrored(false, true));
    transforms.push_back(image.mirrored(true, true));
    
    QTransform rot90;
    rot90.rotate(90);
    QImage rotated90 = image.transformed(rot90);
    transforms.push_back(rotated90);
    transforms.push_back(rotated90.mirrored(true, false));
    transforms.push_back(rotated90.mirrored(false, true));
    transforms.push_back(rotated90.mirrored(true, true));
    
    return transforms;
}

QString ImagePreprocessor::getTTATransformDescription(int transformIndex)
{
    switch (transformIndex) {
        case 0: return QStringLiteral("Original");
        case 1: return QStringLiteral("Horizontal Flip");
        case 2: return QStringLiteral("Vertical Flip");
        case 3: return QStringLiteral("Both Flip");
        case 4: return QStringLiteral("Rotate 90");
        case 5: return QStringLiteral("Rotate 90 + H Flip");
        case 6: return QStringLiteral("Rotate 90 + V Flip");
        case 7: return QStringLiteral("Rotate 90 + Both Flip");
        default: return QStringLiteral("Unknown");
    }
}

void ImagePreprocessor::applyMirrorPadding(QImage& paddedImage, const QImage& originalImage, int padding)
{
    int w = originalImage.width();
    int h = originalImage.height();
    
    const int srcBytesPerLine = originalImage.bytesPerLine();
    const int expectedBytesPerLine = w * 3;
    
    for (int y = 0; y < std::min(padding, h); ++y) {
        const uchar* srcLine = originalImage.constScanLine(y);
        uchar* dstLine = paddedImage.scanLine(padding - 1 - y);
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    
    for (int y = 0; y < std::min(padding, h); ++y) {
        const uchar* srcLine = originalImage.constScanLine(h - 1 - y);
        uchar* dstLine = paddedImage.scanLine(padding + h + y);
        if (srcBytesPerLine >= expectedBytesPerLine) {
            std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
        } else {
            std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
            std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
        }
    }
    
    for (int y = 0; y < h; ++y) {
        uchar* dstLine = paddedImage.scanLine(y + padding);
        const uchar* srcLine = originalImage.constScanLine(y);
        for (int x = 0; x < std::min(padding, w); ++x) {
            int srcX = x;
            int dstX = padding - 1 - x;
            dstLine[dstX * 3 + 0] = srcLine[srcX * 3 + 0];
            dstLine[dstX * 3 + 1] = srcLine[srcX * 3 + 1];
            dstLine[dstX * 3 + 2] = srcLine[srcX * 3 + 2];
        }
        for (int x = 0; x < std::min(padding, w); ++x) {
            int srcX = w - 1 - x;
            int dstX = padding + w + x;
            dstLine[dstX * 3 + 0] = srcLine[srcX * 3 + 0];
            dstLine[dstX * 3 + 1] = srcLine[srcX * 3 + 1];
            dstLine[dstX * 3 + 2] = srcLine[srcX * 3 + 2];
        }
    }
    
    for (int y = 0; y < std::min(padding, h); ++y) {
        uchar* dstLineTop = paddedImage.scanLine(padding - 1 - y);
        uchar* dstLineBot = paddedImage.scanLine(padding + h + y);
        const uchar* srcLineTop = originalImage.constScanLine(y);
        const uchar* srcLineBot = originalImage.constScanLine(h - 1 - y);
        for (int x = 0; x < std::min(padding, w); ++x) {
            int dstXL = padding - 1 - x;
            int dstXR = padding + w + x;
            int srcXL = x;
            int srcXR = w - 1 - x;
            dstLineTop[dstXL * 3 + 0] = srcLineTop[srcXL * 3 + 0];
            dstLineTop[dstXL * 3 + 1] = srcLineTop[srcXL * 3 + 1];
            dstLineTop[dstXL * 3 + 2] = srcLineTop[srcXL * 3 + 2];
            dstLineTop[dstXR * 3 + 0] = srcLineTop[srcXR * 3 + 0];
            dstLineTop[dstXR * 3 + 1] = srcLineTop[srcXR * 3 + 1];
            dstLineTop[dstXR * 3 + 2] = srcLineTop[srcXR * 3 + 2];
            dstLineBot[dstXL * 3 + 0] = srcLineBot[srcXL * 3 + 0];
            dstLineBot[dstXL * 3 + 1] = srcLineBot[srcXL * 3 + 1];
            dstLineBot[dstXL * 3 + 2] = srcLineBot[srcXL * 3 + 2];
            dstLineBot[dstXR * 3 + 0] = srcLineBot[srcXR * 3 + 0];
            dstLineBot[dstXR * 3 + 1] = srcLineBot[srcXR * 3 + 1];
            dstLineBot[dstXR * 3 + 2] = srcLineBot[srcXR * 3 + 2];
        }
    }
}

} // namespace EnhanceVision
