# Shader 视频处理重写

## 概述

**创建日期**: 2026-03-28  
**变更类型**: 重构与功能完善

---

## 一、变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/core/ImageProcessor.cpp` | 完全重写 `applyShader()` 及所有14个滤镜函数，与GPU Shader算法完全一致 |
| `src/core/VideoProcessor.cpp` | 重写视频处理流程，优化内存管理，添加音频流复制 |
| `include/EnhanceVision/core/VideoProcessor.h` | 移除不再使用的辅助函数声明 |

---

## 二、问题分析

### 原有问题

1. **参数不完整**：ImageProcessor 只实现了6个参数（brightness, contrast, saturation, hue, sharpness, denoise），缺少8个参数
2. **算法不一致**：CPU实现的算法与GPU Shader不同，导致预览和导出效果不一致
3. **处理顺序不一致**：参数应用顺序与GPU不同
4. **视频无音频**：导出的视频没有保留原始音频流

### GPU Shader 处理顺序（14个参数）

```
1. Exposure      (曝光)
2. Brightness    (亮度)
3. Contrast      (对比度)
4. Saturation    (饱和度)
5. Hue           (色相)
6. Gamma         (伽马)
7. Temperature   (色温)
8. Tint          (色调)
9. Highlights    (高光)
10. Shadows      (阴影)
11. Vignette     (晕影)
12. Blur         (模糊)
13. Denoise      (降噪)
14. Sharpness    (锐化)
```

---

## 三、技术实现细节

### ImageProcessor 算法一致性

每个滤镜函数都严格按照 `fullshader.frag` 的GLSL代码实现：

```cpp
// 示例：曝光 - GPU: rgb = rgb * pow(2.0, exposure)
void ImageProcessor::applyExposure(QImage& image, float exposure)
{
    float factor = std::pow(2.0f, exposure);
    // ... 像素处理
}

// 示例：对比度 - GPU: rgb = clamp((rgb - 0.5) * contrast + 0.5, 0.0, 1.0)
void ImageProcessor::applyContrast(QImage& image, float contrast)
{
    // 0.5 对应 127.5
    int r = qBound(0, qRound((qRed(line[x]) - 127.5f) * contrast + 127.5f), 255);
    // ...
}

// 示例：饱和度 - 使用亮度公式
// GPU: gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722))
float gray = 0.2126f * rf + 0.7152f * gf + 0.0722f * bf;
```

### HSV 转换算法

完全移植 GPU 的 `rgb2hsv` 和 `hsv2rgb` 算法到 C++：

```cpp
static void rgb2hsv(float r, float g, float b, float& h, float& s, float& v);
static void hsv2rgb(float h, float s, float v, float& r, float& g, float& b);
```

### VideoProcessor 优化

1. **内存管理**：使用 RAII 风格的 cleanup lambda 确保资源释放
2. **帧缓冲复用**：预分配 rgbFrame 和 outFrame，避免每帧分配
3. **音频流复制**：自动检测并复制原始音频流
4. **高质量编码**：
   - H.264 编码器
   - CRF 18 质量设置
   - 多线程编码

### 数据流

```
输入视频 → FFmpeg解码 → RGB32帧 → ImageProcessor.applyShader() → RGB32帧 → YUV420P → FFmpeg编码 → 输出视频
                                         ↓
                                   14个滤镜按GPU顺序处理
```

---

## 四、关键算法对照

| 参数 | GPU 算法 | CPU 实现 |
|------|----------|----------|
| Exposure | `rgb * pow(2.0, exposure)` | `pow(2.0f, exposure)` 乘法 |
| Brightness | `rgb + brightness` | 加法偏移 |
| Contrast | `(rgb - 0.5) * contrast + 0.5` | 以 127.5 为中心缩放 |
| Saturation | 亮度公式 + 线性插值 | 同GPU算法 |
| Hue | HSV 空间旋转 | 移植 GPU 的 rgb2hsv/hsv2rgb |
| Gamma | `pow(rgb, 1.0/gamma)` | 同GPU算法 |
| Temperature | R +0.2*temp, B -0.2*temp | 同GPU系数 |
| Tint | G +0.2*tint | 同GPU系数 |
| Highlights | 亮度>0.5时调整，系数0.3 | 同GPU算法 |
| Shadows | 亮度<0.5时调整，系数0.3 | 同GPU算法 |
| Vignette | 距离平方衰减 | 同GPU算法 |
| Blur | 5x5 高斯，混合系数0.7 | 同GPU算法 |
| Denoise | 3x3 中值滤波，混合系数0.8 | 同GPU算法 |
| Sharpness | 边缘保护锐化 + smoothstep | 同GPU算法 |

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 视频导出基本功能 | 成功生成视频文件 | ✅ 通过 |
| 多视频批量处理 | 全部成功 | ✅ 通过 |
| 不同格式视频 | 正确处理 | ✅ 通过 |
| 音频保留 | 输出包含音频 | ✅ 通过 |
| 预览与导出效果一致 | 视觉效果相同 | ✅ 通过 |

---

## 六、设计决策

1. **CPU 处理而非 GPU**：视频导出需要稳定的像素级处理，CPU 实现更可控
2. **算法严格移植**：确保与 QML 预览 100% 一致
3. **帧缓冲复用**：减少内存分配开销，提高处理速度
4. **音频直接复制**：不重新编码音频，保持原始质量

---

## 七、后续优化方向

- [ ] 考虑 SIMD 优化（AVX2）提升处理速度
- [ ] 考虑 GPU 计算着色器方案（OpenCL/Vulkan Compute）
- [ ] 添加视频编码质量选项（CRF 可配置）
