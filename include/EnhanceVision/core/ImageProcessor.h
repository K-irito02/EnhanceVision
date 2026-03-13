/**
 * @file ImageProcessor.h
 * @brief 图像处理器
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_IMAGEPROCESSOR_H
#define ENHANCEVISION_IMAGEPROCESSOR_H

#include <QObject>
#include <QImage>
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

    /**
     * @brief 应用 Shader 滤镜到图像
     * @param input 输入图像
     * @param params Shader 参数
     * @return 处理后的图像
     */
    QImage applyShader(const QImage& input, const ShaderParams& params);

    /**
     * @brief 异步处理图像
     * @param inputPath 输入图像路径
     * @param outputPath 输出图像路径
     * @param params Shader 参数
     * @param progressCallback 进度回调函数
     * @param finishCallback 完成回调函数
     */
    void processImageAsync(const QString& inputPath,
                          const QString& outputPath,
                          const ShaderParams& params,
                          ProgressCallback progressCallback = nullptr,
                          FinishCallback finishCallback = nullptr);

    /**
     * @brief 取消当前处理
     */
    void cancel();

    /**
     * @brief 检查是否正在处理
     * @return 是否正在处理
     */
    bool isProcessing() const;

signals:
    /**
     * @brief 处理进度更新信号
     * @param progress 进度 (0-100)
     * @param status 状态信息
     */
    void progressChanged(int progress, const QString& status);

    /**
     * @brief 处理完成信号
     * @param success 是否成功
     * @param resultPath 结果文件路径
     * @param error 错误信息（失败时）
     */
    void finished(bool success, const QString& resultPath, const QString& error);

private:
    /**
     * @brief 应用亮度调整
     * @param image 图像
     * @param brightness 亮度值 (-1.0 ~ 1.0)
     */
    void applyBrightness(QImage& image, float brightness);

    /**
     * @brief 应用对比度调整
     * @param image 图像
     * @param contrast 对比度值 (0.0 ~ 2.0)
     */
    void applyContrast(QImage& image, float contrast);

    /**
     * @brief 应用饱和度调整
     * @param image 图像
     * @param saturation 饱和度值 (0.0 ~ 2.0)
     */
    void applySaturation(QImage& image, float saturation);

    /**
     * @brief 应用色相调整
     * @param image 图像
     * @param hue 色相值 (-1.0 ~ 1.0)
     */
    void applyHue(QImage& image, float hue);

    /**
     * @brief 应用锐化滤镜
     * @param image 图像
     * @param sharpness 锐度值 (0.0 ~ 2.0)
     */
    void applySharpen(QImage& image, float sharpness);

    /**
     * @brief 应用降噪滤镜
     * @param image 图像
     * @param denoise 降噪值 (0.0 ~ 1.0)
     */
    void applyDenoise(QImage& image, float denoise);

    bool m_isProcessing;
    bool m_cancelled;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEPROCESSOR_H
