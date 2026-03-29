/**
 * @file VideoFormatConverter.cpp
 * @brief 视频格式转换器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/VideoFormatConverter.h"
#include <QPainter>
#include <cmath>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
}

namespace EnhanceVision {

struct VideoFormatConverter::Impl {
};

VideoFormatConverter::VideoFormatConverter(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
}

VideoFormatConverter::~VideoFormatConverter() = default;

ColorSpaceInfo VideoFormatConverter::detectColorSpace(AVFrame* avFrame)
{
    ColorSpaceInfo info;
    
    if (!avFrame) {
        info.primaries = "bt709";
        info.transfer = "bt709";
        info.matrix = "bt709";
        info.isHDR = false;
        info.isFullRange = false;
        return info;
    }
    
    switch (avFrame->color_primaries) {
    case AVCOL_PRI_BT709:
        info.primaries = "bt709";
        break;
    case AVCOL_PRI_BT2020:
        info.primaries = "bt2020";
        break;
    default:
        info.primaries = "bt709";
        break;
    }
    
    switch (avFrame->color_trc) {
    case AVCOL_TRC_BT709:
        info.transfer = "bt709";
        break;
    case AVCOL_TRC_SMPTE2084:
        info.transfer = "smpte2084";
        break;
    case AVCOL_TRC_ARIB_STD_B67:
        info.transfer = "hlg";
        break;
    default:
        info.transfer = "bt709";
        break;
    }
    
    switch (avFrame->colorspace) {
    case AVCOL_SPC_BT709:
        info.matrix = "bt709";
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        info.matrix = "bt2020nc";
        break;
    default:
        info.matrix = "bt709";
        break;
    }
    
    if (avFrame->color_trc == AVCOL_TRC_SMPTE2084 ||
        avFrame->color_trc == AVCOL_TRC_ARIB_STD_B67) {
        info.isHDR = true;
    }
    
    info.isFullRange = avFrame->color_range == AVCOL_RANGE_JPEG;
    
    return info;
}

bool VideoFormatConverter::needsToneMapping(const ColorSpaceInfo& info)
{
    return info.isHDR || 
           info.transfer == "smpte2084" || 
           info.transfer == "hlg";
}

QImage VideoFormatConverter::convertFrame(const QImage& frame, 
                                           const ColorSpaceInfo& srcColorSpace,
                                           const ColorSpaceInfo& dstColorSpace) const
{
    if (frame.isNull()) {
        return QImage();
    }
    
    if (srcColorSpace.primaries == dstColorSpace.primaries &&
        srcColorSpace.transfer == dstColorSpace.transfer &&
        srcColorSpace.matrix == dstColorSpace.matrix) {
        return frame.copy();
    }
    
    QImage result = frame.copy();
    
    if (srcColorSpace.primaries != dstColorSpace.primaries) {
        result = applyGamutMapping(result, srcColorSpace, dstColorSpace);
    }
    
    if (srcColorSpace.transfer != dstColorSpace.transfer) {
        result = applyTransferFunction(result, srcColorSpace, dstColorSpace);
    }
    
    return result;
}

QImage VideoFormatConverter::applyToneMapping(const QImage& hdrFrame, 
                                               double peakLuminance) const
{
    if (hdrFrame.isNull()) {
        return QImage();
    }
    
    return applyReinhardToneMapping(hdrFrame, peakLuminance);
}

QImage VideoFormatConverter::convertPixelFormat(const QImage& frame, 
                                                 int srcFmt,
                                                 int dstFmt) const
{
    Q_UNUSED(srcFmt)
    Q_UNUSED(dstFmt)
    
    if (frame.isNull()) {
        return QImage();
    }
    
    return frame.convertToFormat(QImage::Format_RGB888);
}

QImage VideoFormatConverter::handleAlphaChannel(const QImage& frame, 
                                                 bool preserveAlpha) const
{
    if (frame.isNull()) {
        return QImage();
    }
    
    if (!frame.hasAlphaChannel()) {
        return frame.copy();
    }
    
    if (preserveAlpha) {
        return frame.convertToFormat(QImage::Format_ARGB32);
    }
    
    QImage result(frame.size(), QImage::Format_RGB888);
    result.fill(Qt::black);
    
    QPainter painter(&result);
    painter.drawImage(0, 0, frame);
    painter.end();
    
    return result;
}

QImage VideoFormatConverter::applyReinhardToneMapping(const QImage& hdrFrame, 
                                                       double peakLuminance) const
{
    if (hdrFrame.isNull()) {
        return QImage();
    }
    
    const int w = hdrFrame.width();
    const int h = hdrFrame.height();
    
    QImage sdrFrame(w, h, QImage::Format_RGB888);
    
    const double L_white = peakLuminance / 100.0;
    const double exposure = 1.0;
    
    QImage srcFrame = hdrFrame.convertToFormat(QImage::Format_ARGB32);
    
    for (int y = 0; y < h; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(srcFrame.constScanLine(y));
        uchar* dstLine = sdrFrame.scanLine(y);
        
        for (int x = 0; x < w; ++x) {
            QRgb hdrPixel = srcLine[x];
            
            double r = qRed(hdrPixel) / 255.0;
            double g = qGreen(hdrPixel) / 255.0;
            double b = qBlue(hdrPixel) / 255.0;
            
            double L = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            
            double L_d = (L * (1.0 + L / (L_white * L_white))) / (1.0 + L);
            double scale = L_d / std::max(L, 1e-6);
            
            r = std::min(1.0, r * scale * exposure);
            g = std::min(1.0, g * scale * exposure);
            b = std::min(1.0, b * scale * exposure);
            
            r = std::pow(r, 1.0 / 2.2);
            g = std::pow(g, 1.0 / 2.2);
            b = std::pow(b, 1.0 / 2.2);
            
            dstLine[x * 3 + 0] = static_cast<uchar>(std::clamp(r * 255.0, 0.0, 255.0));
            dstLine[x * 3 + 1] = static_cast<uchar>(std::clamp(g * 255.0, 0.0, 255.0));
            dstLine[x * 3 + 2] = static_cast<uchar>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
    
    return sdrFrame;
}

QImage VideoFormatConverter::applyHlgToneMapping(const QImage& hdrFrame) const
{
    if (hdrFrame.isNull()) {
        return QImage();
    }
    
    const int w = hdrFrame.width();
    const int h = hdrFrame.height();
    
    QImage sdrFrame(w, h, QImage::Format_RGB888);
    QImage srcFrame = hdrFrame.convertToFormat(QImage::Format_ARGB32);
    
    const double a = 0.17883277;
    const double b = 0.28466892;
    const double c = 0.55991073;
    
    for (int y = 0; y < h; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(srcFrame.constScanLine(y));
        uchar* dstLine = sdrFrame.scanLine(y);
        
        for (int x = 0; x < w; ++x) {
            QRgb hdrPixel = srcLine[x];
            
            double r = qRed(hdrPixel) / 255.0;
            double g = qGreen(hdrPixel) / 255.0;
            double b = qBlue(hdrPixel) / 255.0;
            
            auto hlgToLinear = [a, b, c](double E) -> double {
                if (E <= 0.5) {
                    return E * E / 3.0;
                } else {
                    return std::exp((E - c) / a) + b;
                }
            };
            
            r = hlgToLinear(r);
            g = hlgToLinear(g);
            b = hlgToLinear(b);
            
            r = std::pow(r, 1.0 / 2.2);
            g = std::pow(g, 1.0 / 2.2);
            b = std::pow(b, 1.0 / 2.2);
            
            dstLine[x * 3 + 0] = static_cast<uchar>(std::clamp(r * 255.0, 0.0, 255.0));
            dstLine[x * 3 + 1] = static_cast<uchar>(std::clamp(g * 255.0, 0.0, 255.0));
            dstLine[x * 3 + 2] = static_cast<uchar>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
    
    return sdrFrame;
}

QImage VideoFormatConverter::applyGamutMapping(const QImage& frame,
                                                const ColorSpaceInfo& srcSpace,
                                                const ColorSpaceInfo& dstSpace) const
{
    Q_UNUSED(srcSpace)
    Q_UNUSED(dstSpace)
    return frame.copy();
}

QImage VideoFormatConverter::applyTransferFunction(const QImage& frame,
                                                    const ColorSpaceInfo& srcSpace,
                                                    const ColorSpaceInfo& dstSpace) const
{
    Q_UNUSED(srcSpace)
    Q_UNUSED(dstSpace)
    return frame.copy();
}

} // namespace EnhanceVision
