/**
 * @file VideoFormatConverter.h
 * @brief 视频格式转换器
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEOFORMATCONVERTER_H
#define ENHANCEVISION_VIDEOFORMATCONVERTER_H

#include <QObject>
#include <QImage>
#include <QString>
#include <memory>

extern "C" {
struct AVFrame;
enum AVPixelFormat;
}

namespace EnhanceVision {

struct ColorSpaceInfo {
    QString primaries;
    QString transfer;
    QString matrix;
    bool isHDR = false;
    bool isFullRange = false;
};

class VideoFormatConverter : public QObject {
    Q_OBJECT
    
public:
    explicit VideoFormatConverter(QObject* parent = nullptr);
    ~VideoFormatConverter() override;
    
    QImage convertFrame(const QImage& frame, 
                        const ColorSpaceInfo& srcColorSpace,
                        const ColorSpaceInfo& dstColorSpace) const;
    
    QImage applyToneMapping(const QImage& hdrFrame, 
                            double peakLuminance = 1000.0) const;
    
    QImage convertPixelFormat(const QImage& frame, 
                              int srcFmt,
                              int dstFmt) const;
    
    QImage handleAlphaChannel(const QImage& frame, 
                              bool preserveAlpha = false) const;
    
    static ColorSpaceInfo detectColorSpace(AVFrame* avFrame);
    static bool needsToneMapping(const ColorSpaceInfo& info);
    
private:
    QImage applyReinhardToneMapping(const QImage& hdrFrame, 
                                     double peakLuminance) const;
    QImage applyHlgToneMapping(const QImage& hdrFrame) const;
    QImage applyGamutMapping(const QImage& frame,
                             const ColorSpaceInfo& srcSpace,
                             const ColorSpaceInfo& dstSpace) const;
    QImage applyTransferFunction(const QImage& frame,
                                  const ColorSpaceInfo& srcSpace,
                                  const ColorSpaceInfo& dstSpace) const;
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOFORMATCONVERTER_H
