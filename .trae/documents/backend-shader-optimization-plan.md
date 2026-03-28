# 后端C++ Shader模式图片视频处理模块优化计划

## 问题诊断总结

### 1. 处理顺序一致性分析

**GPU Shader处理顺序** (fullshader.frag):
1. Exposure → 2. Brightness → 3. Contrast → 4. Saturation → 5. Hue → 6. Gamma → 7. Temperature → 8. Tint → 9. Highlights → 10. Shadows → 11. Vignette → 12. Blur → 13. Denoise → 14. Sharpness

**C++ ImageProcessor处理顺序** (ImageProcessor.cpp):
1. Exposure → 2. Brightness → 3. Contrast → 4. Saturation → 5. Hue → 6. Gamma → 7. Temperature → 8. Tint → 9. Highlights → 10. Shadows → 11. Vignette → 12. Blur → 13. Denoise → 14. Sharpness

**结论**: 处理顺序一致 ✓

### 2. 算法实现差异分析

#### 2.1 Blur算法差异（严重）

| 项目 | GPU Shader | C++实现 |
|------|------------|---------|
| 算法类型 | 可分离高斯模糊（水平+垂直） | 5x5区域加权模糊 |
| 采样次数 | 9次 | 25次 |
| 权重计算 | 预计算高斯权重 | 距离加权 |
| 混合因子 | blurAmount * 0.6 | blur * 0.7 |

**问题**: 算法完全不同，视觉效果会有明显差异

#### 2.2 Denoise算法差异（中等）

| 项目 | GPU Shader | C++实现 |
|------|------------|---------|
| 中值算法 | quickMedian9（高效） | 部分冒泡排序 |
| 混合因子 | denoise * 0.8 | denoise * 0.8 |

**问题**: 排序算法不同，结果可能略有差异

#### 2.3 Sharpness算法差异（轻微）

| 项目 | GPU Shader | C++实现 |
|------|------------|---------|
| 边缘因子 | smoothstep(0.0, 0.02, localVariance) | 手动计算 |

**问题**: smoothstep实现方式略有不同

### 3. 性能问题分析

#### 3.1 内存问题
- 每次处理创建多个QImage副本（applyBlur、applyDenoise、applySharpen各创建副本）
- 大图像处理时内存占用高

#### 3.2 访问效率问题
- 使用`image.pixel(x, y)`和`image.setPixel()`而非直接内存访问
- 每次调用都有函数调用开销

#### 3.3 无并行化
- 单线程处理所有像素
- 未利用多核CPU

#### 3.4 无SIMD优化
- 未使用AVX/SSE指令集加速浮点运算

### 4. 边界处理问题

- Blur: 未处理边缘2像素
- Denoise: 未处理边缘1像素
- Sharpness: 未处理边缘1像素

### 5. 代码质量问题

1. **异常安全**: 部分资源可能泄漏
2. **代码重复**: 多个函数有相似的像素遍历逻辑
3. **缺少验证**: 无单元测试验证与GPU一致性

---

## 优化方案

### 阶段一：算法一致性修复（高优先级）

#### 1.1 重写Blur算法
**文件**: `src/core/ImageProcessor.cpp`
**改动**:
- 实现与GPU一致的可分离高斯模糊
- 使用预计算权重
- 修改混合因子为 blurAmount * 0.6

#### 1.2 重写Denoise算法
**文件**: `src/core/ImageProcessor.cpp`
**改动**:
- 实现quickMedian9快速中值算法
- 与GPU算法完全一致

#### 1.3 修复Sharpness算法
**文件**: `src/core/ImageProcessor.cpp`
**改动**:
- 实现与GPU一致的smoothstep函数
- 确保边缘因子计算一致

### 阶段二：性能优化（高优先级）

#### 2.1 内存优化
**改动**:
- 减少不必要的QImage副本创建
- 使用原地处理（in-place）优化
- 预分配缓冲区复用

#### 2.2 访问效率优化
**改动**:
- 使用`scanLine()`直接访问像素数据
- 避免使用`pixel()`/`setPixel()`

#### 2.3 多线程并行处理
**改动**:
- 使用QtConcurrent实现像素级并行
- 分块处理大图像
- 支持取消和暂停

#### 2.4 SIMD优化（可选）
**改动**:
- 使用AVX2指令集加速浮点运算
- 使用SSE4.1加速整数运算

### 阶段三：代码质量提升（中优先级）

#### 3.1 边界处理完善
**改动**:
- 正确处理图像边缘像素
- 使用镜像填充或边缘扩展

#### 3.2 代码重构
**改动**:
- 提取公共像素遍历逻辑
- 创建统一的处理框架
- 添加参数验证

#### 3.3 异常安全
**改动**:
- 使用RAII管理资源
- 确保异常时资源正确释放

### 阶段四：测试验证（中优先级）

#### 4.1 创建单元测试
**新文件**: `tests/ImageProcessorTest.cpp`
**内容**:
- 对比GPU和CPU处理结果
- 验证各参数效果
- 边界条件测试

#### 4.2 性能基准测试
**内容**:
- 测量不同分辨率处理时间
- 对比优化前后性能
- 内存使用分析

---

## 实施步骤

### Step 1: 重写applyBlur函数
1. 实现可分离高斯模糊
2. 使用预计算权重
3. 修改混合因子
4. 添加边界处理

### Step 2: 重写applyDenoise函数
1. 实现quickMedian9算法
2. 优化排序逻辑
3. 添加边界处理

### Step 3: 修复applySharpen函数
1. 实现smoothstep函数
2. 确保边缘因子计算一致

### Step 4: 性能优化
1. 修改所有函数使用scanLine访问
2. 减少QImage副本创建
3. 添加并行处理支持

### Step 5: 代码重构
1. 提取公共处理逻辑
2. 创建统一的处理框架
3. 添加参数验证

### Step 6: 测试验证
1. 创建单元测试
2. 对比GPU和CPU结果
3. 性能基准测试

---

## 详细代码修改

### 1. applyBlur重写

```cpp
void ImageProcessor::applyBlur(QImage& image, float blurAmount)
{
    if (blurAmount <= 0.001f) return;
    
    int width = image.width();
    int height = image.height();
    
    // 预计算高斯权重（与GPU一致）
    float weights[5] = {0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f};
    float blurRadius = blurAmount * 1.5f;
    
    QImage temp(width, height, QImage::Format_RGB32);
    
    // 水平Pass
    for (int y = 0; y < height; ++y) {
        QRgb* srcLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(temp.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            float r = qRed(srcLine[x]) * weights[0];
            float g = qGreen(srcLine[x]) * weights[0];
            float b = qBlue(srcLine[x]) * weights[0];
            
            for (int i = 1; i <= 4; ++i) {
                int offset = qRound(i * blurRadius);
                int left = qMax(0, x - offset);
                int right = qMin(width - 1, x + offset);
                
                r += (qRed(srcLine[left]) + qRed(srcLine[right])) * weights[i];
                g += (qGreen(srcLine[left]) + qGreen(srcLine[right])) * weights[i];
                b += (qBlue(srcLine[left]) + qBlue(srcLine[right])) * weights[i];
            }
            
            dstLine[x] = qRgb(qBound(0, qRound(r), 255),
                              qBound(0, qRound(g), 255),
                              qBound(0, qRound(b), 255));
        }
    }
    
    // 垂直Pass + 混合
    for (int y = 0; y < height; ++y) {
        QRgb* dstLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        
        for (int x = 0; x < width; ++x) {
            float r = qRed(temp.pixel(x, y)) * weights[0];
            float g = qGreen(temp.pixel(x, y)) * weights[0];
            float b = qBlue(temp.pixel(x, y)) * weights[0];
            
            for (int i = 1; i <= 4; ++i) {
                int offset = qRound(i * blurRadius);
                int top = qMax(0, y - offset);
                int bottom = qMin(height - 1, y + offset);
                
                r += (qRed(temp.pixel(x, top)) + qRed(temp.pixel(x, bottom))) * weights[i];
                g += (qGreen(temp.pixel(x, top)) + qGreen(temp.pixel(x, bottom))) * weights[i];
                b += (qBlue(temp.pixel(x, top)) + qBlue(temp.pixel(x, bottom))) * weights[i];
            }
            
            // 混合原始和模糊结果（与GPU一致）
            float origR = qRed(dstLine[x]);
            float origG = qGreen(dstLine[x]);
            float origB = qBlue(dstLine[x]);
            
            float mixFactor = blurAmount * 0.6f;
            int finalR = qBound(0, qRound(origR * (1 - mixFactor) + r * mixFactor), 255);
            int finalG = qBound(0, qRound(origG * (1 - mixFactor) + g * mixFactor), 255);
            int finalB = qBound(0, qRound(origB * (1 - mixFactor) + b * mixFactor), 255);
            
            dstLine[x] = qRgb(finalR, finalG, finalB);
        }
    }
}
```

### 2. quickMedian9实现

```cpp
static float quickMedian3(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

static float quickMedian9(float v[9])
{
    float min1 = std::min(std::min(v[0], v[1]), v[2]);
    float min2 = std::min(std::min(v[3], v[4]), v[5]);
    float min3 = std::min(std::min(v[6], v[7]), v[8]);
    float max1 = std::max(std::max(v[0], v[1]), v[2]);
    float max2 = std::max(std::max(v[3], v[4]), v[5]);
    float max3 = std::max(std::max(v[6], v[7]), v[8]);
    
    float mid1 = v[0] + v[1] + v[2] - min1 - max1;
    float mid2 = v[3] + v[4] + v[5] - min2 - max2;
    float mid3 = v[6] + v[7] + v[8] - min3 - max3;
    
    float minMid = std::min(std::min(mid1, mid2), mid3);
    float maxMid = std::max(std::max(mid1, mid2), mid3);
    float centerMid = mid1 + mid2 + mid3 - minMid - maxMid;
    
    float minMin = std::min(min1, std::min(min2, min3));
    float maxMax = std::max(max1, std::max(max2, max3));
    
    return quickMedian3(minMin, centerMid, maxMax);
}
```

### 3. smoothstep实现

```cpp
static float smoothstep(float edge0, float edge1, float x)
{
    float t = qBound(0.0f, (x - edge0) / (edge1 - edge0), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
```

---

## 预期效果

| 指标 | 优化前 | 优化后（预期） |
|------|--------|----------------|
| 算法一致性 | 有差异 | 完全一致 |
| 处理速度 | 基准 | 提升2-4倍 |
| 内存占用 | 较高 | 降低30% |
| 边界处理 | 不完整 | 完整处理 |

---

## 实施文件清单

### 需要修改的文件
1. `src/core/ImageProcessor.cpp` - 算法修复和性能优化
2. `include/EnhanceVision/core/ImageProcessor.h` - 接口更新

### 需要创建的文件
1. `tests/ImageProcessorTest.cpp` - 单元测试

---

## 风险评估

1. **算法修改**: 可能影响现有处理结果，需充分测试
2. **性能优化**: 多线程可能引入竞态条件，需仔细设计
3. **兼容性**: SIMD优化需要考虑CPU支持情况
