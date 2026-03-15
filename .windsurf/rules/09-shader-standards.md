---
name: "shader-standards"
alwaysApply: false
globs: ['**/*.vert', '**/*.frag', '**/*.qsb', '**/shaders/**']
description: 'GLSL Shader standards - Reference for shader development, compilation, and usage'
trigger: glob
---
# GLSL Shader 规范

## 文件组织

### 目录结构

```
resources/shaders/
├── basic.vert           # 基础顶点着色器
├── brightness.frag      # 亮度调整
├── contrast.frag        # 对比度调整
├── saturation.frag      # 饱和度调整
├── hue.frag             # 色相调整
├── sharpen.frag         # 锐化
├── blur.frag            # 模糊（高斯/双边）
└── yuv.frag             # YUV 转换
```

### 文件命名

| 类型 | 后缀 | 示例 |
|------|------|------|
| 顶点着色器 | .vert | basic.vert |
| 片元着色器 | .frag | brightness.frag |
| 计算着色器 | .comp | process.comp |

## GLSL 版本

### 目标版本

| 后端 | GLSL 版本 | HLSL 版本 | MSL 版本 |
|------|-----------|-----------|----------|
| OpenGL ES 2.0 | 100 es | - | - |
| OpenGL ES 3.0 | 300 es | - | - |
| OpenGL | 120, 150 | - | - |
| Direct3D 11 | - | 50 | - |
| Metal | - | - | 12 |

## 顶点着色器模板

### 基础顶点着色器

```glsl
// basic.vert
#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

layout(location = 0) out vec2 qt_TexCoord0;

void main() {
    qt_TexCoord0 = qt_MultiTexCoord0;
    gl_Position = qt_Matrix * qt_Vertex;
}
```

## 片元着色器模板

### 亮度调整

```glsl
// brightness.frag
#version 440

layout(location = 0) in vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float brightness;  // -1.0 ~ 1.0
};

layout(binding = 1) uniform sampler2D source;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 color = texture(source, qt_TexCoord0);
    color.rgb += brightness;
    fragColor = color * qt_Opacity;
}
```

### 对比度调整

```glsl
// contrast.frag
#version 440

layout(location = 0) in vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float contrast;  // 0.0 ~ 2.0, 1.0 = 原始
};

layout(binding = 1) uniform sampler2D source;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 color = texture(source, qt_TexCoord0);
    color.rgb = (color.rgb - 0.5) * contrast + 0.5;
    fragColor = color * qt_Opacity;
}
```

### 饱和度调整

```glsl
// saturation.frag
#version 440

layout(location = 0) in vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float saturation;  // 0.0 ~ 2.0, 1.0 = 原始
};

layout(binding = 1) uniform sampler2D source;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 color = texture(source, qt_TexCoord0);
    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 gray = vec3(luminance);
    color.rgb = mix(gray, color.rgb, saturation);
    fragColor = color * qt_Opacity;
}
```

### 高斯模糊

```glsl
// blur.frag
#version 440

layout(location = 0) in vec2 qt_TexCoord0;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 texelSize;
    float radius;  // 模糊半径
};

layout(binding = 1) uniform sampler2D source;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 color = vec4(0.0);
    float total = 0.0;
    
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    
    for (int i = -4; i <= 4; i++) {
        for (int j = -4; j <= 4; j++) {
            vec2 offset = vec2(float(i), float(j)) * texelSize * radius;
            float weight = weights[abs(i)] * weights[abs(j)];
            color += texture(source, qt_TexCoord0 + offset) * weight;
            total += weight;
        }
    }
    
    fragColor = (color / total) * qt_Opacity;
}
```

## QML 中使用 Shader

### ShaderEffect 基础

```qml
ShaderEffect {
    id: effect
    
    property var source: previewImage
    property real brightness: brightnessSlider.value
    
    vertexShader: "qrc:/shaders/basic.vert.qsb"
    fragmentShader: "qrc:/shaders/brightness.frag.qsb"
}
```

### 多参数 Shader

```qml
ShaderEffect {
    id: colorAdjust
    
    property var source: previewImage
    property real brightness: brightnessSlider.value
    property real contrast: contrastSlider.value
    property real saturation: saturationSlider.value
    
    fragmentShader: "qrc:/shaders/colorAdjust.frag.qsb"
}
```

### 动态切换 Shader

```qml
Item {
    id: root
    
    property string currentShader: "brightness"
    
    Loader {
        id: shaderLoader
        sourceComponent: {
            switch (root.currentShader) {
                case "brightness": return brightnessEffect
                case "contrast": return contrastEffect
                case "blur": return blurEffect
                default: return null
            }
        }
    }
    
    Component {
        id: brightnessEffect
        ShaderEffect {
            property var source: previewImage
            property real brightness: brightnessSlider.value
            fragmentShader: "qrc:/shaders/brightness.frag.qsb"
        }
    }
}
```

## 性能优化

### 减少纹理采样

```glsl
// ❌ 避免
vec4 c1 = texture(source, uv);
vec4 c2 = texture(source, uv);

// ✅ 正确
vec4 c = texture(source, uv);
```

### 避免分支

```glsl
// ❌ 避免
if (condition) {
    color = vec4(1.0);
} else {
    color = vec4(0.0);
}

// ✅ 正确
color = mix(vec4(0.0), vec4(1.0), float(condition));
```

### 使用内置函数

```glsl
// ✅ 正确
float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

// ❌ 避免
float luminance = color.r * 0.2126 + color.g * 0.7152 + color.b * 0.0722;
```

## 构建集成

### CMake 配置

```cmake
qt_add_shaders(EnhanceVision "shaders"
    PREFIX "/shaders"
    BASE "${CMAKE_SOURCE_DIR}/resources/shaders"
    FILES
        "basic.vert"
        "brightness.frag"
        "contrast.frag"
        "saturation.frag"
        "sharpen.frag"
        "blur.frag"
)
```

### 手动编译

```powershell
qsb --glsl "100 es,120,150" --hlsl 50 --msl 12 -o output.qsb input.frag
```

## 常见问题

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 黑屏 | Shader 编译失败 | 检查 qsb 文件路径 |
| 颜色错误 | 颜色空间问题 | 确认输入是 RGB |
| 性能差 | 复杂计算 | 简化算法或降低分辨率 |
| 闪烁 | 每帧重新编译 | 缓存 ShaderEffect |
