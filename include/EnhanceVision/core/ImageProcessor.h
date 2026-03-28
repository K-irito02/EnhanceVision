/**
 * @file ImageProcessor.h
 * @brief 图像处理器 - 与GPU Shader完全一致的算法
 * @author Qt客户端开发工程师
 * 
 * 优化要点：
 * 1. 算法与GPU Shader完全一致
 * 2. 使用scanLine直接访问像素数据
 * 3. 减少不必要的QImage副本
 * 4. 完善边界处理
 */

#ifndef ENHANCEVISION_IMAGEPROCESSOR_H
#define ENHANCEVISION_IMAGEPROCESSOR_H

#include <QObject>
#include <QImage>
#include <memory>
#include <atomic>
#include <vector>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

/**
 * @brief 图像处理器类
 * 负责图像处理、Shader 参数应用、进度回调
 * 
 * 算法与GPU Shader完全一致，确保前后端效果一致性
 */
class ImageProcessor : public QObject
{
    Q_OBJECT

public:
    explicit ImageProcessor(QObject *parent = nullptr);
    ~ImageProcessor() override;

    /**
     * @brief 应用Shader效果到图像
     * @param input 输入图像
     * @param params Shader参数
     * @return 处理后的图像
     */
    QImage applyShader(const QImage& input, const ShaderParams& params);

    /**
     * @brief 异步处理图像
     */
    void processImageAsync(const QString& inputPath,
                          const QString& outputPath,
                          const ShaderParams& params,
                          ProgressCallback progressCallback = nullptr,
                          FinishCallback finishCallback = nullptr);

    /**
     * @brief 带取消/暂停令牌的异步处理
     */
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
    void applyExposure(QImage& image, float exposure);
    void applyBrightness(QImage& image, float brightness);
    void applyContrast(QImage& image, float contrast);
    void applySaturation(QImage& image, float saturation);
    void applyHue(QImage& image, float hue);
    void applyGamma(QImage& image, float gamma);
    void applyTemperature(QImage& image, float temperature);
    void applyTint(QImage& image, float tint);
    void applyHighlights(QImage& image, float highlights);
    void applyShadows(QImage& image, float shadows);
    void applyVignette(QImage& image, float vignette);
    void applyBlur(QImage& image, float blurAmount);
    void applyDenoise(QImage& image, float denoise);
    void applySharpen(QImage& image, float sharpness);
    
    void applyDenoiseWithNeighbors(QImage& image, float denoise,
                                    const std::vector<std::vector<QRgb>>& neighborPixels,
                                    const std::vector<QRgb>& originalPixels,
                                    int width, int height);
    
    void applySharpenWithNeighbors(QImage& image, float sharpness,
                                    const std::vector<std::vector<QRgb>>& neighborPixels,
                                    const std::vector<QRgb>& originalPixels,
                                    int width, int height);

    bool m_isProcessing;
    bool m_cancelled;
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
    std::shared_ptr<std::atomic<bool>> m_pauseToken;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEPROCESSOR_H
