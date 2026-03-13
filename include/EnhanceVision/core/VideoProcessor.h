/**
 * @file VideoProcessor.h
 * @brief 视频处理器
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_VIDEOPROCESSOR_H
#define ENHANCEVISION_VIDEOPROCESSOR_H

#include <QObject>
#include <QString>
#include "EnhanceVision/models/DataTypes.h"

// FFmpeg 头文件
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace EnhanceVision {

/**
 * @brief 视频处理器类
 * 使用 FFmpeg 进行视频解码、处理、编码
 */
class VideoProcessor : public QObject
{
    Q_OBJECT

public:
    explicit VideoProcessor(QObject *parent = nullptr);
    ~VideoProcessor() override;

    /**
     * @brief 异步处理视频
     * @param inputPath 输入视频路径
     * @param outputPath 输出视频路径
     * @param params Shader 参数
     * @param progressCallback 进度回调函数
     * @param finishCallback 完成回调函数
     */
    void processVideoAsync(const QString& inputPath,
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
     * @brief 内部视频处理函数
     */
    void processVideoInternal(const QString& inputPath,
                             const QString& outputPath,
                             const ShaderParams& params,
                             ProgressCallback progressCallback,
                             FinishCallback finishCallback);

    /**
     * @brief 将 AVFrame 转换为 QImage
     */
    QImage frameToImage(AVFrame* frame, AVPixelFormat pixelFormat);

    /**
     * @brief 将 QImage 转换为 AVFrame
     */
    AVFrame* imageToFrame(const QImage& image, AVFrame* referenceFrame);

    bool m_isProcessing;
    bool m_cancelled;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOPROCESSOR_H
