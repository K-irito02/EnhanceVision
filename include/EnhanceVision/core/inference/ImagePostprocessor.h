/**
 * @file ImagePostprocessor.h
 * @brief 图像后处理器
 * @author EnhanceVision Team
 * 
 * 从 AIEngine 中提取的后处理逻辑，职责单一：
 * - 输出转换
 * - 反归一化
 * - 分块合并
 * - 缩放输出
 */

#ifndef ENHANCEVISION_IMAGEPOSTPROCESSOR_H
#define ENHANCEVISION_IMAGEPOSTPROCESSOR_H

#include "InferenceTypes.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QImage>
#include <QSize>
#include <vector>
#include <memory>

namespace ncnn {
class Mat;
}

namespace EnhanceVision {

/**
 * @brief 图像后处理器
 * 
 * 提供图像后处理功能，将推理输出转换为 QImage。
 * 支持反归一化、分块合并、TTA 合并等操作。
 */
class ImagePostprocessor
{
public:
    ImagePostprocessor();
    ~ImagePostprocessor();
    
    // ========== 图像转换 ==========
    
    /**
     * @brief 将 ncnn::Mat 转换为 QImage
     * @param mat 输入矩阵
     * @param model 模型信息（包含反归一化参数）
     * @return QImage 格式的图像
     */
    static QImage matToQimage(const ncnn::Mat& mat, const ModelInfo& model);
    
    // ========== 反归一化 ==========
    
    /**
     * @brief 对 ncnn::Mat 进行反归一化
     * @param mat 输入矩阵
     * @param mean 均值
     * @param scale 缩放因子
     */
    static void denormalize(ncnn::Mat& mat, const float mean[3], const float scale[3]);
    
    /**
     * @brief 限制矩阵值范围到 [0, 255]
     * @param mat 输入矩阵
     */
    static void clampValues(ncnn::Mat& mat);
    
    // ========== 分块合并 ==========
    
    /**
     * @brief 合并分块处理结果
     * @param tiles 分块图像列表
     * @param tileInfos 分块信息列表
     * @param outputSize 输出尺寸
     * @param scale 缩放因子
     * @return 合并后的图像
     */
    static QImage mergeTiles(const std::vector<QImage>& tiles, 
                              const std::vector<TileInfo>& tileInfos,
                              const QSize& outputSize,
                              int scale);
    
    /**
     * @brief 计算分块在输出图像中的位置
     * @param tileInfo 分块信息
     * @param scale 缩放因子
     * @return 输出位置 (x, y, width, height)
     */
    static QRect computeTileOutputRect(const TileInfo& tileInfo, int scale);
    
    // ========== TTA 合并 ==========
    
    /**
     * @brief 合并 TTA 变换结果
     * @param results 各变换的推理结果
     * @return 合并后的图像
     */
    static QImage mergeTTAResults(const std::vector<QImage>& results);
    
    /**
     * @brief 对 TTA 结果应用逆变换
     * @param image 推理结果
     * @param transformIndex 变换索引 (0-7)
     * @return 逆变换后的图像
     */
    static QImage applyInverseTTATransform(const QImage& image, int transformIndex);
    
    // ========== 缩放输出 ==========
    
    /**
     * @brief 应用输出缩放
     * @param image 输入图像
     * @param scale 缩放因子
     * @return 缩放后的图像
     */
    static QImage applyOutscale(const QImage& image, double scale);
    
    /**
     * @brief 计算输出尺寸
     * @param inputSize 输入尺寸
     * @param modelScale 模型缩放因子
     * @param outscale 用户指定的输出缩放
     * @return 输出尺寸
     */
    static QSize computeOutputSize(const QSize& inputSize, int modelScale, double outscale);

private:
    /**
     * @brief 计算像素平均值（用于 TTA 合并）
     * @param pixels 像素值列表
     * @return 平均像素值
     */
    static QRgb averagePixels(const std::vector<QRgb>& pixels);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEPOSTPROCESSOR_H
