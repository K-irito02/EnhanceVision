# ShaderEffect参数一致性解决方案

## 问题背景

在EnhanceVision项目中，需要在消息预览区域（缩略图）和放大查看区域实现完全一致的Shader效果。当用户调整Shader参数时，需要确保所有显示区域的实时效果保持完全同步和一致。

## 搜索到的解决方案

### 方案1：Qt官方示例 - QML Video Shader Effects

**来源**: Qt Multimedia官方示例

**核心思路**:
- 使用统一的ShaderEffect组件
- 通过属性绑定实现参数同步
- 示例代码结构：
```qml
// 统一的Effect组件
Effect {
    id: sharedEffect
    property real brightness: 0.5
    property real contrast: 1.0
    // ... 其他参数
}

// 在多个地方使用
ShaderEffect {
    property var effectSource: sharedEffect
    brightness: effectSource.brightness
    contrast: effectSource.contrast
}
```

**优点**:
- 官方示例，稳定可靠
- 支持视频和图像处理
- 包含完整的参数面板实现

**缺点**:
- 需要为每个Effect单独创建组件
- 属性绑定可能带来性能开销

---

### 方案2：QML Singleton共享参数

**来源**: Qt文档和社区讨论

**核心思路**:
- 创建ShaderParameters单例
- 所有ShaderEffect绑定到单例属性
```qml
// ShaderParameters.qml
pragma Singleton
QtObject {
    property real brightness: 0.5
    property real contrast: 1.0
    property real saturation: 1.0
    // ... 其他参数
}

// 使用方式
ShaderEffect {
    brightness: ShaderParameters.brightness
    contrast: ShaderParameters.contrast
    saturation: ShaderParameters.saturations
}
```

**优点**:
- 全局统一管理
- 属性变化自动同步到所有实例
- 内存效率高

**缺点**:
- 需要注册单例类型
- 调试相对复杂

---

### 方案3：共享ShaderEffect源组件

**来源**: Woboq博客和KDAB教程

**核心思路**:
- 创建一个主ShaderEffect作为参数源
- 其他ShaderEffect实例绑定到主组件的属性
```qml
// 主ShaderEffect
ShaderEffect {
    id: masterEffect
    property real brightness: 0.5
    property real contrast: 1.0
    // 实际渲染逻辑
}

// 从属ShaderEffect
ShaderEffect {
    property var master: masterEffect
    brightness: master.brightness
    contrast: master.contrast
    source: differentSource
}
```

**优点**:
- 实现简单直观
- 可以有主从关系
- 易于控制

**缺点**:
- 依赖关系复杂
- 主组件删除会影响所有从组件

---

## 我的推荐方案

### 方案4：混合方案 - Singleton + 组件化

基于以上研究和项目实际需求，我推荐以下混合方案：

#### 4.1 架构设计

```
ShaderParameters (Singleton)
├── 管理所有Shader参数
├── 提供参数变化信号
└── 支持参数预设保存/加载

UniformShaderEffect (Component)
├── 封装通用ShaderEffect逻辑
├── 绑定到ShaderParameters
├── 支持不同源(图像/视频)
└── 优化性能的缓存机制

MediaShaderEffect (继承UniformShaderEffect)
├── 专门用于媒体文件
├── 添加媒体特定功能
└── 处理缩略图优化
```

#### 4.2 实现代码

**ShaderParameters.qml**:
```qml
pragma Singleton
import QtQuick 2.15

QtObject {
    id: root
    
    // 基础参数
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real hue: 0.0
    property real gamma: 1.0
    
    // 高级参数
    property real vignetteStrength: 0.0
    property real blurAmount: 0.0
    property real sharpenAmount: 0.0
    
    // 颜色调整
    property color tintColor: "white"
    property real tintStrength: 0.0
    
    // 信号通知参数变化
    signal parametersChanged()
    
    // 批量更新参数（减少重复渲染）
    function updateParameters(params) {
        for (var key in params) {
            if (root.hasOwnProperty(key)) {
                root[key] = params[key];
            }
        }
        parametersChanged();
    }
    
    // 重置为默认值
    function reset() {
        updateParameters({
            brightness: 0.0,
            contrast: 1.0,
            saturation: 1.0,
            hue: 0.0,
            gamma: 1.0,
            vignetteStrength: 0.0,
            blurAmount: 0.0,
            sharpenAmount: 0.0,
            tintColor: "white",
            tintStrength: 0.0
        });
    }
    
    // 保存/加载预设
    function savePreset(name) {
        // 实现预设保存逻辑
    }
    
    function loadPreset(name) {
        // 实现预设加载逻辑
    }
}
```

**UniformShaderEffect.qml**:
```qml
import QtQuick 2.15

ShaderEffect {
    id: root
    
    // 必须提供的源
    property var source
    
    // 绑定到全局参数
    property real brightness: ShaderParameters.brightness
    property real contrast: ShaderParameters.contrast
    property real saturation: ShaderParameters.saturation
    property real hue: ShaderParameters.hue
    property real gamma: ShaderParameters.gamma
    property real vignetteStrength: ShaderParameters.vignetteStrength
    property real blurAmount: ShaderParameters.blurAmount
    property real sharpenAmount: ShaderParameters.sharpenAmount
    property color tintColor: ShaderParameters.tintColor
    property real tintStrength: ShaderParameters.tintStrength
    
    // 性能优化：使用layer.enable=true减少重复计算
    layer.enabled: true
    layer.smooth: true
    
    // 统一的着色器代码
    vertexShader: "
        uniform highp mat4 qt_Matrix;
        attribute highp vec4 qt_Vertex;
        attribute highp vec2 qt_MultiTexCoord0;
        varying highp vec2 qt_TexCoord0;
        void main() {
            qt_TexCoord0 = qt_MultiTexCoord0;
            gl_Position = qt_Matrix * qt_Vertex;
        }
    "
    
    fragmentShader: "
        varying highp vec2 qt_TexCoord0;
        uniform sampler2D source;
        uniform lowp float qt_Opacity;
        uniform lowp float brightness;
        uniform lowp float contrast;
        uniform lowp float saturation;
        uniform lowp float hue;
        uniform lowp float gamma;
        uniform lowp float vignetteStrength;
        uniform lowp float blurAmount;
        uniform lowp float sharpenAmount;
        uniform lowp vec4 tintColor;
        uniform lowp float tintStrength;
        
        // RGB到HSV转换
        vec3 rgb2hsv(vec3 c) {
            vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
            vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
            vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
            float d = q.x - min(q.w, q.y);
            float e = 1.0e-10;
            return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
        }
        
        // HSV到RGB转换
        vec3 hsv2rgb(vec3 c) {
            vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }
        
        void main() {
            vec4 tex = texture2D(source, qt_TexCoord0);
            vec3 color = tex.rgb;
            
            // 亮度调整
            color += brightness;
            
            // 对比度调整
            color = (color - 0.5) * contrast + 0.5;
            
            // 饱和度调整
            vec3 hsv = rgb2hsv(color);
            hsv.y *= saturation;
            color = hsv2rgb(hsv);
            
            // 色调调整
            hsv = rgb2hsv(color);
            hsv.x = mod(hsv.x + hue / 360.0, 1.0);
            color = hsv2rgb(hsv);
            
            // 伽马调整
            color = pow(color, vec3(1.0 / gamma));
            
            // 晕影效果
            vec2 center = vec2(0.5, 0.5);
            float dist = distance(qt_TexCoord0, center);
            float vignette = 1.0 - smoothstep(0.0, 0.7, dist * vignetteStrength);
            color *= vignette;
            
            // 着色
            color = mix(color, color * tintColor.rgb, tintStrength);
            
            gl_FragColor = vec4(color, tex.a) * qt_Opacity;
        }
    "
    
    // 连接参数变化信号
    Connections {
        target: ShaderParameters
        function onParametersChanged() {
            // 触发重新渲染
            root.markDirty();
        }
    }
}
```

**MediaShaderEffect.qml**:
```qml
import QtQuick 2.15

UniformShaderEffect {
    id: root
    
    // 媒体特定属性
    property bool isThumbnail: false
    property bool isVideo: false
    
    // 缩略图优化
    property int thumbnailQuality: isThumbnail ? 0 : 1
    
    // 视频特定处理
    property real playbackTime: 0.0
    
    // 根据类型调整渲染质量
    layer.smooth: !isThumbnail
    layer.mipmap: !isThumbnail
    
    // 缩略图简化着色器
    onIsThumbnailChanged: {
        if (isThumbnail) {
            // 使用简化版本的着色器提高性能
            fragmentShader = simplifiedFragmentShader;
        } else {
            fragmentShader = fullFragmentShader;
        }
    }
    
    property string simplifiedFragmentShader: "
        varying highp vec2 qt_TexCoord0;
        uniform sampler2D source;
        uniform lowp float qt_Opacity;
        uniform lowp float brightness;
        uniform lowp float contrast;
        
        void main() {
            vec4 tex = texture2D(source, qt_TexCoord0);
            vec3 color = tex.rgb;
            color += brightness;
            color = (color - 0.5) * contrast + 0.5;
            gl_FragColor = vec4(color, tex.a) * qt_Opacity;
        }
    "
    
    property string fullFragmentShader: "
        // 完整的着色器代码（同UniformShaderEffect）
        varying highp vec2 qt_TexCoord0;
        uniform sampler2D source;
        uniform lowp float qt_Opacity;
        uniform lowp float brightness;
        uniform lowp float contrast;
        uniform lowp float saturation;
        uniform lowp float hue;
        uniform lowp float gamma;
        uniform lowp float vignetteStrength;
        uniform lowp float blurAmount;
        uniform lowp float sharpenAmount;
        uniform lowp vec4 tintColor;
        uniform lowp float tintStrength;
        
        // ... 完整的实现代码
    "
}
```

#### 4.3 使用示例

**在MediaThumbnailStrip中使用**:
```qml
MediaShaderEffect {
    source: thumbnailImage
    isThumbnail: true
    width: thumbnailWidth
    height: thumbnailHeight
}
```

**在MediaViewerWindow中使用**:
```qml
MediaShaderEffect {
    source: fullSizeImage
    isThumbnail: false
    width: parent.width
    height: parent.height
}
```

**参数控制面板**:
```qml
Column {
    Slider {
        value: ShaderParameters.brightness
        onValueChanged: ShaderParameters.brightness = value
    }
    Slider {
        value: ShaderParameters.contrast
        onValueChanged: ShaderParameters.contrast = value
    }
    // ... 其他参数控制
}
```

## 方案对比分析

| 方案 | 实现复杂度 | 性能 | 维护性 | 扩展性 | 推荐度 |
|------|------------|------|--------|--------|--------|
| Qt官方示例 | 中 | 中 | 高 | 中 | ⭐⭐⭐ |
| QML Singleton | 低 | 高 | 高 | 高 | ⭐⭐⭐⭐ |
| 共享源组件 | 中 | 中 | 中 | 低 | ⭐⭐ |
| 混合方案 | 中 | 高 | 高 | 高 | ⭐⭐⭐⭐⭐ |

## 实施注意事项

### 1. 性能优化
- 使用`layer.enabled: true`减少重复计算
- 缩略图使用简化着色器
- 批量更新参数避免频繁重绘
- 考虑使用`Qt.binding()`优化属性绑定

### 2. 内存管理
- 避免创建过多ShaderEffect实例
- 使用`Component.onDestruction`清理资源
- 对于大量缩略图，考虑对象池模式

### 3. 调试技巧
- 使用`console.log`输出参数变化
- 添加着色器编译错误处理
- 提供参数重置功能

### 4. 兼容性考虑
- 测试不同OpenGL版本兼容性
- 考虑软件渲染降级方案
- 处理不同硬件性能差异

## 实施步骤

1. **创建ShaderParameters单例**
   - 定义所有需要的参数
   - 实现参数变化通知机制
   - 添加预设保存/加载功能

2. **实现UniformShaderEffect基础组件**
   - 编写完整的着色器代码
   - 绑定到ShaderParameters
   - 添加性能优化

3. **创建MediaShaderEffect特化组件**
   - 添加媒体特定功能
   - 实现缩略图优化
   - 处理视频特殊情况

4. **集成到现有组件**
   - 修改MediaThumbnailStrip
   - 更新MediaViewerWindow
   - 添加参数控制面板

5. **测试和优化**
   - 验证效果一致性
   - 性能测试和优化
   - 边界情况处理

## 总结

推荐使用**混合方案（Singleton + 组件化）**，它结合了QML Singleton的全局管理优势和组件化的灵活性，能够很好地解决Shader效果一致性问题，同时保持良好的性能和可维护性。

该方案的核心理念是：
- **统一管理**：通过Singleton统一管理所有Shader参数
- **组件复用**：通过基础组件实现Shader逻辑复用
- **性能优化**：针对不同使用场景进行优化
- **易于扩展**：便于添加新的Shader效果和参数

这样不仅解决了当前的一致性问题，还为未来的功能扩展打下了良好的基础。
