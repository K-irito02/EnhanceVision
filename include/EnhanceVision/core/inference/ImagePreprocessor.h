/**
 * @file ImagePreprocessor.h
 * @brief 图像预处理器
 * @author EnhanceVision Team
 * 
 * 从 AIEngine 中提取的预处理逻辑，职责单一：
 * - 图像格式转换
 * - 归一化处理
 * - 图像分块
 * - 图像填充
 */

#ifndef ENHANCEVISION_IMAGEPREPROCESSOR_H
#define ENHANCEVISION_IMAGEPREPROCESSOR_H

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
 * @brief 图像预处理器
 * 
 * 提供图像预处理功能，将 QImage 转换为推理所需的格式。
 * 支持归一化、分块、填充等操作。
 */
class ImagePreprocessor
{
public:
    ImagePreprocessor();
    ~ImagePreprocessor();
    
    // ========== 图像转换 ==========
    
    /**
     * @brief 将 QImage 转换为 ncnn::Mat
     * @param image 输入图像
     * @param model 模型信息（包含归一化参数）
     * @return ncnn::Mat 格式的图像数据
     */
    static ncnn::Mat qimageToMat(const QImage& image, const ModelInfo& model);
    
    /**
     * @brief 将 QImage 转换为 RGB888 格式
     * @param image 输入图像
     * @return RGB888 格式的图像
     */
    static QImage convertToRGB888(const QImage& image);
    
    // ========== 归一化 ==========
    
    /**
     * @brief 对 ncnn::Mat 进行归一化
     * @param mat 输入矩阵
     * @param mean 均值
     * @param scale 缩放因子
     */
    static void normalize(ncnn::Mat& mat, const float mean[3], const float scale[3]);
    
    // ========== 分块处理 ==========
    
    /**
     * @brief 计算分块信息
     * @param imageSize 图像尺寸
     * @param tileSize 分块大小
     * @return 分块信息列表
     */
    static std::vector<TileInfo> computeTiles(const QSize& imageSize, int tileSize);
    
    /**
     * @brief 提取图像分块
     * @param image 输入图像
     * @param tileInfo 分块信息
     * @param padding 填充大小
     * @return 分块图像
     */
    static QImage extractTile(const QImage& image, const TileInfo& tileInfo, int padding);
    
    /**
     * @brief 创建带填充的图像
     * @param image 输入图像
     * @param padding 填充大小
     * @return 带填充的图像
     */
    static QImage createPaddedImage(const QImage& image, int padding);
    
    // ========== 尺寸调整 ==========
    
    /**
     * @brief 计算合适的分块大小
     * @param imageSize 图像尺寸
     * @param modelTileSize 模型建议的分块大小
     * @param maxMemoryMB 最大可用内存 (MB)
     * @return 计算出的分块大小，0 表示不需要分块
     */
    static int computeOptimalTileSize(const QSize& imageSize, int modelTileSize, 
                                       qint64 maxMemoryMB = 256);
    
    /**
     * @brief 检查是否需要分块
     * @param imageSize 图像尺寸
     * @param tileSize 分块大小
     * @return true 如果需要分块
     */
    static bool needsTiling(const QSize& imageSize, int tileSize);
    
    // ========== TTA 支持 ==========
    
    /**
     * @brief 生成 TTA 变换图像
     * @param image 输入图像
     * @return 8 个变换后的图像（原始+翻转+旋转组合）
     */
    static std::vector<QImage> generateTTATransforms(const QImage& image);
    
    /**
     * @brief 获取 TTA 变换的逆变换索引
     * @param transformIndex 变换索引 (0-7)
     * @return 逆变换所需的操作描述
     */
    static QString getTTATransformDescription(int transformIndex);

private:
    /**
     * @brief 对图像边缘进行镜像填充
     * @param paddedImage 带填充的图像（已分配内存）
     * @param originalImage 原始图像
     * @param padding 填充大小
     */
    static void applyMirrorPadding(QImage& paddedImage, const QImage& originalImage, int padding);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEPREPROCESSOR_H
