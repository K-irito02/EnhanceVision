/**
 * @file ImageProcessor.h
 * @brief 图像处理器
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_IMAGEPROCESSOR_H
#define ENHANCEVISION_IMAGEPROCESSOR_H

#include <QObject>
#include <QImage>
#include <memory>
#include <atomic>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

/**
 * @brief 图像处理器类
 * 负责图像处理、Shader 参数应用、进度回调
 */
class ImageProcessor : public QObject
{
    Q_OBJECT

public:
    explicit ImageProcessor(QObject *parent = nullptr);
    ~ImageProcessor() override;

    QImage applyShader(const QImage& input, const ShaderParams& params);

    void processImageAsync(const QString& inputPath,
                          const QString& outputPath,
                          const ShaderParams& params,
                          ProgressCallback progressCallback = nullptr,
                          FinishCallback finishCallback = nullptr);

    void processImageAsyncWithTokens(const QString& inputPath,
                                     const QString& outputPath,
                                     const ShaderParams& params,
                                     std::shared_ptr<std::atomic<bool>> cancelToken,
                                     std::shared_ptr<std::atomic<bool>> pauseToken = nullptr,
                                     ProgressCallback progressCallback = nullptr,
                                     FinishCallback finishCallback = nullptr);

    void cancel();
    
    void interrupt();

    bool isProcessing() const;
    
    bool shouldContinue() const;
    
    void waitIfPaused();

signals:
    void progressChanged(int progress, const QString& status);
    void finished(bool success, const QString& resultPath, const QString& error);
    void cancelled();

private:
    void applyBrightness(QImage& image, float brightness);
    void applyContrast(QImage& image, float contrast);
    void applySaturation(QImage& image, float saturation);
    void applyHue(QImage& image, float hue);
    void applySharpen(QImage& image, float sharpness);
    void applyDenoise(QImage& image, float denoise);
    void applyExposure(QImage& image, float exposure);
    void applyGamma(QImage& image, float gamma);
    void applyTemperature(QImage& image, float temperature);
    void applyTint(QImage& image, float tint);
    void applyVignette(QImage& image, float vignette);
    void applyHighlights(QImage& image, float highlights);
    void applyShadows(QImage& image, float shadows);
    void applyBlur(QImage& image, float blur);

    bool m_isProcessing;
    bool m_cancelled;
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
    std::shared_ptr<std::atomic<bool>> m_pauseToken;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEPROCESSOR_H
