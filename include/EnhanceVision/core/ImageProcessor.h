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
 * 5. 细粒度进度报告
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

class ProgressReporter;

enum class ProcessingStage {
    Idle = 0,
    Loading,
    Preprocessing,
    ColorAdjust,
    Effects,
    Postprocessing,
    Saving,
    Completed
};

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

    void processImageAsyncWithReporter(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
                                       ProgressReporter* reporter,
                                       ProgressCallback progressCallback = nullptr,
                                       FinishCallback finishCallback = nullptr);

    void cancel();
    void interrupt();
    bool isProcessing() const;
    bool shouldContinue() const;
    void waitIfPaused();
    
    ProcessingStage currentStage() const;

signals:
    void progressChanged(int progress, const QString& status);
    void finished(bool success, const QString& resultPath, const QString& error);
    void cancelled();
    void stageChanged(ProcessingStage stage);

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
    
    void reportProgress(int progress, const QString& status);
    void reportStageProgress(ProcessingStage stage, double stageProgress);
    QString stageToString(ProcessingStage stage) const;

    bool m_isProcessing;
    bool m_cancelled;
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
    std::shared_ptr<std::atomic<bool>> m_pauseToken;
    ProgressReporter* m_reporter;
    std::atomic<ProcessingStage> m_currentStage{ProcessingStage::Idle};
    
    static constexpr int kProgressLoadingStart = 0;
    static constexpr int kProgressLoadingEnd = 15;
    static constexpr int kProgressPreprocessStart = 15;
    static constexpr int kProgressPreprocessEnd = 20;
    static constexpr int kProgressColorAdjustStart = 20;
    static constexpr int kProgressColorAdjustEnd = 60;
    static constexpr int kProgressEffectsStart = 60;
    static constexpr int kProgressEffectsEnd = 85;
    static constexpr int kProgressSavingStart = 85;
    static constexpr int kProgressSavingEnd = 100;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEPROCESSOR_H
