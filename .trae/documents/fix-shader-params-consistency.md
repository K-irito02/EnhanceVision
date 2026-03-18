# 修复 Shader 参数一致性问题

## 问题分析

### 根本原因

**QML `MultiEffect` 只支持 4 个参数，而 Shader 控制面板有 14 个参数**

| 组件 | 支持的参数 | 数量 |
|------|-----------|------|
| **QML MultiEffect** (预览区放大查看) | brightness, contrast, saturation, blur | 4 |
| **C++ ImageUtils::applyShaderEffects** (最终处理) | brightness, contrast, saturation, hue, exposure, gamma, temperature, tint, vignette, highlights, shadows | 11 |
| **Shader 控制面板** | brightness, contrast, saturation, hue, sharpness, blur, denoise, exposure, gamma, temperature, tint, vignette, highlights, shadows | 14 |

### 效果不一致的具体表现

1. **预览区放大查看**：只能实时预览 4 个参数效果（brightness, contrast, saturation, blur）
2. **最终处理结果**：应用了 11 个参数效果
3. **缺失的预览效果**：hue, exposure, gamma, temperature, tint, vignette, highlights, shadows
4. **缺失的处理效果**：sharpness, blur, denoise

### 用户操作流程

```
用户调整 14 个参数 → 预览只显示 4 个参数效果 → 发送后得到 11 个参数效果 → 效果不一致！
```

## 解决方案

### 方案：创建自定义 ShaderEffect 组件

创建一个自定义的 `FullShaderEffect.qml` 组件，支持所有 14 个 Shader 参数的实时预览。

**优点：**
- 实时 GPU 渲染，性能好
- 与 Qt Scene Graph 集成
- 预览效果与最终结果一致

**技术实现：**
- 使用 `ShaderEffect` + GLSL Fragment Shader
- 实现 14 个参数的 GPU 着色器算法
- 与 C++ `ImageUtils::applyShaderEffects` 算法保持一致

## 实施步骤

### 步骤 1：创建 GLSL Shader 文件

创建 `resources/shaders/full_shader.frag`，实现所有 14 个参数的着色器算法。

### 步骤 2：创建 QML ShaderEffect 组件

创建 `qml/components/FullShaderEffect.qml`，封装 ShaderEffect 并暴露所有参数。

### 步骤 3：更新 MediaViewerWindow.qml

将 `MultiEffect` 替换为 `FullShaderEffect`，支持所有 14 个参数。

### 步骤 4：补充 C++ ImageUtils 缺失的参数

在 `ImageUtils::applyShaderEffects` 中添加 sharpness, blur, denoise 参数支持。

### 步骤 5：验证一致性

确保 QML Shader 和 C++ 处理算法完全一致。

## 详细设计

### 1. GLSL Fragment Shader 设计

```glsl
// full_shader.frag
uniform sampler2D source;
uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float hue;
uniform float sharpness;
uniform float blur;
uniform float exposure;
uniform float gamma;
uniform float temperature;
uniform float tint;
uniform float vignette;
uniform float highlights;
uniform float shadows;
uniform vec2 textureSize;

varying vec2 qtTexCoord0;

// HSV 转换函数
vec3 rgb2hsv(vec3 c) { ... }
vec3 hsv2rgb(vec3 c) { ... }

void main() {
    vec4 color = texture2D(source, qtTexCoord0);
    
    // 1. 曝光
    color.rgb *= pow(2.0, exposure);
    
    // 2. 亮度
    color.rgb = clamp(color.rgb + brightness, 0.0, 1.0);
    
    // 3. 对比度
    color.rgb = clamp((color.rgb - 0.5) * contrast + 0.5, 0.0, 1.0);
    
    // 4. 饱和度
    float gray = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    color.rgb = clamp(gray + saturation * (color.rgb - gray), 0.0, 1.0);
    
    // 5. 色相
    vec3 hsv = rgb2hsv(color.rgb);
    hsv.x = fract(hsv.x + hue);
    color.rgb = hsv2rgb(hsv);
    
    // 6. 伽马
    color.rgb = pow(color.rgb, vec3(1.0 / gamma));
    
    // 7. 色温
    color.r += temperature * 0.1;
    color.b -= temperature * 0.1;
    
    // 8. 色调
    color.g += tint * 0.1;
    
    // 9. 高光
    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (luminance > 0.5) {
        float factor = (luminance - 0.5) * 2.0;
        color.rgb += highlights * factor * 0.2;
    }
    
    // 10. 阴影
    if (luminance < 0.5) {
        float factor = (0.5 - luminance) * 2.0;
        color.rgb += shadows * factor * 0.2;
    }
    
    // 11. 晕影
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(qtTexCoord0, center) * 1.414;
    float vignetteFactor = 1.0 - vignette * dist * dist;
    color.rgb *= vignetteFactor;
    
    // 12. 锐度 (需要采样周围像素)
    // 13. 模糊 (需要采样周围像素)
    // 14. 降噪 (需要采样周围像素)
    
    gl_FragColor = clamp(color, 0.0, 1.0);
}
```

### 2. QML 组件设计

```qml
// FullShaderEffect.qml
import QtQuick

ShaderEffect {
    id: root
    
    property var source: null
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real hue: 0.0
    property real sharpness: 0.0
    property real blur: 0.0
    property real exposure: 0.0
    property real gamma: 1.0
    property real temperature: 0.0
    property real tint: 0.0
    property real vignette: 0.0
    property real highlights: 0.0
    property real shadows: 0.0
    
    fragmentShader: "qrc:/shaders/full_shader.frag"
    
    // 参数绑定
    onBrightnessChanged: updateUniforms()
    onContrastChanged: updateUniforms()
    // ... 其他参数
}
```

### 3. C++ 补充参数

在 `ImageUtils::applyShaderEffects` 中添加：

```cpp
// 锐度 (使用卷积核)
if (sharpness > 0.001f) {
    // 使用拉普拉斯算子锐化
}

// 模糊 (使用高斯模糊)
if (blur > 0.001f) {
    // 高斯模糊实现
}

// 降噪 (使用中值滤波或双边滤波)
if (denoise > 0.001f) {
    // 降噪实现
}
```

## 文件修改清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `resources/shaders/full_shader.frag` | 新建 | 完整 Shader 实现 |
| `resources/shaders/full_shader.vert` | 新建 | 顶点着色器 |
| `qml/components/FullShaderEffect.qml` | 新建 | QML ShaderEffect 组件 |
| `qml/components/MediaViewerWindow.qml` | 修改 | 替换 MultiEffect 为 FullShaderEffect |
| `src/utils/ImageUtils.cpp` | 修改 | 添加 sharpness, blur, denoise 支持 |
| `include/EnhanceVision/utils/ImageUtils.h` | 修改 | 更新函数签名 |
| `resources/qml.qrc` | 修改 | 添加 shader 文件 |

## 风险评估

1. **性能风险**：复杂的 Shader 可能影响渲染性能
   - 缓解：优化 Shader 代码，减少不必要的计算

2. **跨平台兼容性**：不同平台的 GLSL 版本可能不同
   - 缓解：使用 Qt Shader Tools 编译跨平台 Shader

3. **算法一致性**：确保 QML Shader 和 C++ 算法完全一致
   - 缓解：编写单元测试验证一致性

## 测试计划

1. **单元测试**：验证 C++ ImageUtils 各参数效果
2. **集成测试**：验证 QML Shader 与 C++ 处理结果一致性
3. **性能测试**：确保实时预览流畅
4. **跨平台测试**：在 Windows/macOS/Linux 上验证效果一致
