# Shader 参数问题修复计划

## 问题分析

### 问题1：降噪参数不起作用

**根本原因**：`App.qml` 中缺少 `shaderDenoise` 的 alias 定义

**代码位置**：[App.qml:26-38](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/App.qml#L26-L38)

```qml
// 当前定义了这些 alias：
property alias shaderBrightness: controlPanel.brightness
property alias shaderContrast: controlPanel.contrast
property alias shaderSaturation: controlPanel.saturation
property alias shaderHue: controlPanel.hue
property alias shaderSharpness: controlPanel.sharpness
property alias shaderBlur: controlPanel.blur
property alias shaderExposure: controlPanel.exposure
property alias shaderGamma: controlPanel.gamma
property alias shaderTemperature: controlPanel.temperature
property alias shaderTint: controlPanel.tint
property alias shaderVignette: controlPanel.vignette
property alias shaderHighlights: controlPanel.highlights
property alias shaderShadows: controlPanel.shadows

// 缺少：property alias shaderDenoise: controlPanel.denoise
```

**影响**：
- `_getShaderParam("shaderDenoise", 0.0)` 总是返回默认值 0.0
- 预览区域和最终处理都无法应用降噪效果

---

### 问题2：消息展示区域与预览区域效果不一致

**根本原因**：Shader (GPU实时渲染) 和 C++ ImageUtils (CPU处理保存) 的实现存在显著差异

#### 差异对比

| 方面 | Shader (预览区域) | C++ ImageUtils (最终结果) |
|------|------------------|--------------------------|
| **处理顺序** | Exposure → Brightness → Contrast → Saturation → Hue → Gamma → Temperature → Tint → Highlights → Shadows → Vignette → Denoise → Blur → Sharpness | Blur/Sharpness/Denoise (先处理邻域) → Exposure → Brightness → Contrast → Saturation → Hue → Gamma → Temperature → Tint → Highlights → Shadows → Vignette |
| **降噪强度** | `mix(rgb, medianColor, denoise * 0.5)` 强度减半 | `r = r * (1.0f - denoise) + reds[mid] * denoise` 完整强度 |
| **模糊算法** | 5x5 盒式模糊 | 动态半径高斯模糊近似 |
| **锐化采样源** | 使用原始颜色 `originalColor` | 使用处理后的颜色 |

#### 算法实现差异详解

**1. 降噪 (Denoise)**

Shader 实现 ([fullshader.frag:126-163](file:///e:/QtAudio-VideoLearning/EnhanceVision/resources/shaders/fullshader.frag#L126-L163)):
```glsl
// 强度减半
rgb = mix(rgb, medianColor, denoise * 0.5);
```

C++ 实现 ([ImageUtils.cpp:364-366](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/utils/ImageUtils.cpp#L364-L366)):
```cpp
// 完整强度
r = r * (1.0f - denoise) + reds[mid] * denoise;
```

**2. 模糊 (Blur)**

Shader 实现 ([fullshader.frag:166-185](file:///e:/QtAudio-VideoLearning/EnhanceVision/resources/shaders/fullshader.frag#L166-L185)):
```glsl
// 5x5 盒式模糊，固定范围
for (int x = -2; x <= 2; x++) {
    for (int y = -2; y <= 2; y++) {
        // ...
    }
}
rgb = mix(rgb, blurColor / totalWeight, blurAmount * 0.5);  // 强度减半
```

C++ 实现 ([ImageUtils.cpp:287-310](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/utils/ImageUtils.cpp#L287-L310)):
```cpp
// 动态半径高斯模糊近似
int radius = static_cast<int>(blur * 3);
// ...
r = r * (1.0f - blur) + (sumR / sumWeight) * blur;  // 完整强度
```

**3. 处理顺序影响**

Shader 顺序：先调整颜色，最后处理邻域效果
```
颜色调整 → 邻域效果 (Denoise → Blur → Sharpness)
```

C++ 顺序：先处理邻域效果，再调整颜色
```
邻域效果 (Blur → Sharpness → Denoise) → 颜色调整
```

这导致：
- Shader 的锐化在颜色调整之后，使用原始颜色计算
- C++ 的锐化在颜色调整之前，使用处理后的颜色计算

---

## 修复方案

### 方案选择：统一使用 Shader 算法

**理由**：
1. 用户反馈预览区域效果更好（更细腻、清晰）
2. Shader 是实时预览的标准，应该让最终结果与预览一致
3. 修改 C++ 代码来匹配 Shader 算法

### 修复步骤

#### 步骤1：修复降噪参数传递

**文件**：[App.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/App.qml)

添加缺失的 alias：
```qml
property alias shaderDenoise: controlPanel.denoise
```

#### 步骤2：统一 C++ ImageUtils 算法与 Shader

**文件**：[ImageUtils.cpp](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/utils/ImageUtils.cpp)

修改内容：

1. **调整处理顺序**：先颜色调整，后邻域效果
2. **修改降噪强度**：`denoise * 0.5` 与 Shader 一致
3. **修改模糊强度**：`blur * 0.5` 与 Shader 一致
4. **修改锐化算法**：使用原始颜色计算锐化

#### 步骤3：验证修复

1. 构建项目
2. 测试降噪参数是否生效
3. 对比预览区域和最终结果是否一致

---

## 详细代码修改

### 1. App.qml 修改

位置：第 31 行后添加

```qml
property alias shaderDenoise: controlPanel.denoise
```

### 2. ImageUtils.cpp 修改

需要重构 `applyShaderEffects` 函数，使其与 Shader 实现一致：

```cpp
QImage ImageUtils::applyShaderEffects(const QImage &image, 
                                       float brightness,
                                       float contrast,
                                       float saturation,
                                       float hue,
                                       float exposure,
                                       float gamma,
                                       float temperature,
                                       float tint,
                                       float vignette,
                                       float highlights,
                                       float shadows,
                                       float sharpness,
                                       float blur,
                                       float denoise)
{
    if (image.isNull()) {
        return QImage();
    }
    
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    QImage original = result.copy();  // 保存原始图像用于锐化
    
    int width = result.width();
    int height = result.height();
    int centerX = width / 2;
    int centerY = height / 2;
    float maxDist = std::sqrt(centerX * centerX + centerY * centerY);
    
    // 第一阶段：像素级颜色调整（与 Shader 顺序一致）
    for (int y = 0; y < height; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            int a = qAlpha(line[x]);
            
            // 1. Exposure
            if (qAbs(exposure) > 0.001f) {
                r = r * std::pow(2.0f, exposure);
                g = g * std::pow(2.0f, g);
                b = b * std::pow(2.0f, b);
            }
            
            // 2. Brightness
            if (qAbs(brightness) > 0.001f) {
                r = qBound(0.0f, r + brightness, 1.0f);
                g = qBound(0.0f, g + brightness, 1.0f);
                b = qBound(0.0f, b + brightness, 1.0f);
            }
            
            // 3. Contrast
            if (qAbs(contrast - 1.0f) > 0.001f) {
                r = qBound(0.0f, (r - 0.5f) * contrast + 0.5f, 1.0f);
                g = qBound(0.0f, (g - 0.5f) * contrast + 0.5f, 1.0f);
                b = qBound(0.0f, (b - 0.5f) * contrast + 0.5f, 1.0f);
            }
            
            // 4. Saturation
            if (qAbs(saturation - 1.0f) > 0.001f) {
                float gray = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r = qBound(0.0f, gray + saturation * (r - gray), 1.0f);
                g = qBound(0.0f, gray + saturation * (g - gray), 1.0f);
                b = qBound(0.0f, gray + saturation * (b - gray), 1.0f);
            }
            
            // 5. Hue
            if (qAbs(hue) > 0.001f) {
                float h, s, v;
                rgbToHsv(r, g, b, h, s, v);
                h = h + hue;
                if (h < 0.0f) h += 1.0f;
                if (h > 1.0f) h -= 1.0f;
                hsvToRgb(h, s, v, r, g, b);
            }
            
            // 6. Gamma
            if (qAbs(gamma - 1.0f) > 0.001f) {
                float invGamma = 1.0f / gamma;
                r = std::pow(r, invGamma);
                g = std::pow(g, invGamma);
                b = std::pow(b, invGamma);
            }
            
            // 7. Temperature
            if (qAbs(temperature) > 0.001f) {
                r = qBound(0.0f, r + temperature * 0.1f, 1.0f);
                b = qBound(0.0f, b - temperature * 0.1f, 1.0f);
            }
            
            // 8. Tint
            if (qAbs(tint) > 0.001f) {
                g = qBound(0.0f, g + tint * 0.1f, 1.0f);
            }
            
            // 9. Highlights
            if (qAbs(highlights) > 0.001f) {
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance > 0.5f) {
                    float factor = (luminance - 0.5f) * 2.0f;
                    float adjustment = highlights * factor * 0.2f;
                    r = qBound(0.0f, r + adjustment, 1.0f);
                    g = qBound(0.0f, g + adjustment, 1.0f);
                    b = qBound(0.0f, b + adjustment, 1.0f);
                }
            }
            
            // 10. Shadows
            if (qAbs(shadows) > 0.001f) {
                float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (luminance < 0.5f) {
                    float factor = (0.5f - luminance) * 2.0f;
                    float adjustment = shadows * factor * 0.2f;
                    r = qBound(0.0f, r + adjustment, 1.0f);
                    g = qBound(0.0f, g + adjustment, 1.0f);
                    b = qBound(0.0f, b + adjustment, 1.0f);
                }
            }
            
            // 11. Vignette
            if (vignette > 0.001f) {
                float dx = (x - centerX) / (float)centerX;
                float dy = (y - centerY) / (float)centerY;
                float dist = std::sqrt(dx * dx + dy * dy) * 1.414f;
                float vignetteFactor = 1.0f - vignette * dist * dist;
                vignetteFactor = qBound(0.0f, vignetteFactor, 1.0f);
                r *= vignetteFactor;
                g *= vignetteFactor;
                b *= vignetteFactor;
            }
            
            line[x] = qRgba(static_cast<int>(qBound(0.0f, r, 1.0f) * 255),
                           static_cast<int>(qBound(0.0f, g, 1.0f) * 255),
                           static_cast<int>(qBound(0.0f, b, 1.0f) * 255),
                           a);
        }
    }
    
    // 第二阶段：邻域效果（与 Shader 顺序一致：Denoise → Blur → Sharpness）
    if (denoise > 0.01f || blur > 0.01f || sharpness > 0.01f) {
        QImage temp = result.copy();
        
        for (int y = 0; y < height; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < width; ++x) {
                float r = qRed(line[x]) / 255.0f;
                float g = qGreen(line[x]) / 255.0f;
                float b = qBlue(line[x]) / 255.0f;
                int a = qAlpha(line[x]);
                
                // 12. Denoise (中值滤波，强度减半与 Shader 一致)
                if (denoise > 0.01f) {
                    QVector<float> reds, greens, blues;
                    
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = qBound(0, x + dx, width - 1);
                            int ny = qBound(0, y + dy, height - 1);
                            QRgb neighbor = temp.pixel(nx, ny);
                            reds.append(qRed(neighbor) / 255.0f);
                            greens.append(qGreen(neighbor) / 255.0f);
                            blues.append(qBlue(neighbor) / 255.0f);
                        }
                    }
                    
                    std::sort(reds.begin(), reds.end());
                    std::sort(greens.begin(), greens.end());
                    std::sort(blues.begin(), blues.end());
                    
                    int mid = reds.size() / 2;
                    float medianR = reds[mid];
                    float medianG = greens[mid];
                    float medianB = blues[mid];
                    
                    // 强度减半，与 Shader 一致
                    r = r * (1.0f - denoise * 0.5f) + medianR * denoise * 0.5f;
                    g = g * (1.0f - denoise * 0.5f) + medianG * denoise * 0.5f;
                    b = b * (1.0f - denoise * 0.5f) + medianB * denoise * 0.5f;
                }
                
                // 13. Blur (5x5 盒式模糊，强度减半与 Shader 一致)
                if (blur > 0.01f) {
                    float sumR = 0, sumG = 0, sumB = 0, sumWeight = 0;
                    
                    for (int dy = -2; dy <= 2; ++dy) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            int nx = qBound(0, x + dx, width - 1);
                            int ny = qBound(0, y + dy, height - 1);
                            QRgb neighbor = temp.pixel(nx, ny);
                            float weight = 1.0f - std::sqrt(dx*dx + dy*dy) / 3.0f;
                            weight = qMax(0.0f, weight);
                            sumR += qRed(neighbor) / 255.0f * weight;
                            sumG += qGreen(neighbor) / 255.0f * weight;
                            sumB += qBlue(neighbor) / 255.0f * weight;
                            sumWeight += weight;
                        }
                    }
                    
                    if (sumWeight > 0) {
                        float blurR = sumR / sumWeight;
                        float blurG = sumG / sumWeight;
                        float blurB = sumB / sumWeight;
                        // 强度减半，与 Shader 一致
                        r = r * (1.0f - blur * 0.5f) + blurR * blur * 0.5f;
                        g = g * (1.0f - blur * 0.5f) + blurG * blur * 0.5f;
                        b = b * (1.0f - blur * 0.5f) + blurB * blur * 0.5f;
                    }
                }
                
                // 14. Sharpness (使用原始图像计算，与 Shader 一致)
                if (sharpness > 0.01f) {
                    // 使用原始图像计算模糊
                    float blurR = 0, blurG = 0, blurB = 0;
                    blurR += qRed(original.pixel(qBound(0, x - 1, width - 1), y)) / 255.0f;
                    blurR += qRed(original.pixel(qBound(0, x + 1, width - 1), y)) / 255.0f;
                    blurR += qRed(original.pixel(x, qBound(0, y - 1, height - 1))) / 255.0f;
                    blurR += qRed(original.pixel(x, qBound(0, y + 1, height - 1))) / 255.0f;
                    blurR /= 4.0f;
                    
                    blurG += qGreen(original.pixel(qBound(0, x - 1, width - 1), y)) / 255.0f;
                    blurG += qGreen(original.pixel(qBound(0, x + 1, width - 1), y)) / 255.0f;
                    blurG += qGreen(original.pixel(x, qBound(0, y - 1, height - 1))) / 255.0f;
                    blurG += qGreen(original.pixel(x, qBound(0, y + 1, height - 1))) / 255.0f;
                    blurG /= 4.0f;
                    
                    blurB += qBlue(original.pixel(qBound(0, x - 1, width - 1), y)) / 255.0f;
                    blurB += qBlue(original.pixel(qBound(0, x + 1, width - 1), y)) / 255.0f;
                    blurB += qBlue(original.pixel(x, qBound(0, y - 1, height - 1))) / 255.0f;
                    blurB += qBlue(original.pixel(x, qBound(0, y + 1, height - 1))) / 255.0f;
                    blurB /= 4.0f;
                    
                    // 使用原始颜色计算锐化量
                    float origR = qRed(original.pixel(x, y)) / 255.0f;
                    float origG = qGreen(original.pixel(x, y)) / 255.0f;
                    float origB = qBlue(original.pixel(x, y)) / 255.0f;
                    
                    float sharpenR = origR - blurR;
                    float sharpenG = origG - blurG;
                    float sharpenB = origB - blurB;
                    
                    r = qBound(0.0f, r + sharpness * sharpenR, 1.0f);
                    g = qBound(0.0f, g + sharpness * sharpenG, 1.0f);
                    b = qBound(0.0f, b + sharpness * sharpenB, 1.0f);
                }
                
                line[x] = qRgba(static_cast<int>(qBound(0.0f, r, 1.0f) * 255),
                               static_cast<int>(qBound(0.0f, g, 1.0f) * 255),
                               static_cast<int>(qBound(0.0f, b, 1.0f) * 255),
                               a);
            }
        }
    }
    
    return result;
}
```

---

## 预期结果

1. **降噪参数生效**：调整降噪滑块时，预览区域和最终结果都会显示降噪效果
2. **效果一致**：消息展示区域的缩略图/放大效果与预览区域的最终效果完全一致
3. **质量提升**：最终保存的图像质量与预览时看到的效果相同
