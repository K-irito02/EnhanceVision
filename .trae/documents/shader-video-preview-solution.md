# Shader 模式视频实时预览方案分析

## 问题背景

当前 Shader 模式在处理视频时存在以下限制：
1. **视频播放时无法应用 Shader 参数调整**：`VideoOutput` 组件直接渲染视频帧，无法通过 `ShaderEffect` 实时处理
2. **快速预设风格对视频无效**：切换风格时，视频预览不会变化
3. **预览与最终效果可能不一致**：发送消息后，视频通过 FFmpeg 逐帧处理，效果可能与预览不同

## 当前实现分析

### 图片处理流程
```
用户调整参数 → ShaderParamsPanel.paramChanged → MainPage 更新参数
    ↓
FullShaderEffect 组件接收参数 → GPU Shader 实时渲染
    ↓
发送消息 → OffscreenShaderRenderer GPU 导出 → 效果完全一致
```

### 视频处理流程（当前）
```
用户调整参数 → 参数变化但视频不响应
    ↓
VideoOutput 直接渲染原始帧（无 Shader）
    ↓
发送消息 → VideoProcessor FFmpeg 逐帧处理 → 效果可能不一致
```

## 技术挑战

1. **VideoOutput 的限制**：
   - `VideoOutput` 是 Qt Multimedia 提供的高性能视频渲染组件
   - 它直接将解码后的帧渲染到屏幕，不暴露纹理接口
   - `ShaderEffect` 需要 `source` 属性作为输入纹理

2. **实时性与性能**：
   - 视频帧率高（24-60fps），需要每帧都应用 Shader
   - CPU 处理太慢，必须使用 GPU 加速

3. **效果一致性**：
   - 预览使用 QML Shader（GLSL）
   - 最终处理使用 FFmpeg + CPU 图像处理
   - 两者的算法实现可能不完全一致

## 方案对比

### 方案一：ShaderEffectSource + ShaderEffect 方案

#### 实现原理
使用 `ShaderEffectSource` 将 `VideoOutput` 的内容捕获为纹理，然后通过 `ShaderEffect` 处理后显示。

```qml
VideoOutput {
    id: videoOutput
    visible: false  // 隐藏原始视频
}

ShaderEffectSource {
    id: videoSource
    sourceItem: videoOutput
    live: true
    hideSource: true
}

FullShaderEffect {
    source: videoSource
    // Shader 参数绑定
}
```

#### 优点
1. **实现简单**：纯 QML 实现，无需修改 C++ 代码
2. **实时性好**：GPU 加速，帧率不受影响
3. **效果一致**：使用相同的 Shader 代码，预览与导出效果一致
4. **代码复用**：复用现有的 `FullShaderEffect` 组件

#### 缺点
1. **性能开销**：`ShaderEffectSource` 需要额外的纹理拷贝
2. **兼容性问题**：某些平台/驱动可能不支持
3. **内存占用**：需要额外的显存存储中间纹理

#### 性能评估
- 额外 GPU 开销：约 5-15%（纹理拷贝 + Shader 处理）
- 内存开销：视频分辨率 × 4 字节（RGBA）
- 1080p 视频额外内存：约 8MB

#### 实现复杂度
- **低**（约 2-3 天）
- 主要工作：修改 `MediaViewerWindow.qml` 和 `EmbeddedMediaViewer.qml`

---

### 方案二：自定义 VideoFilter 方案

#### 实现原理
创建自定义 `VideoFilter` 组件，继承 `QAbstractVideoFilter`，在 C++ 层拦截视频帧并应用 Shader。

```cpp
class ShaderVideoFilter : public QAbstractVideoFilter {
    Q_OBJECT
    Q_PROPERTY(float brightness MEMBER m_brightness)
    Q_PROPERTY(float contrast MEMBER m_contrast)
    // ... 其他参数
    
public:
    QVideoFilterRunnable* createFilterRunnable() override;
};

class ShaderVideoFilterRunnable : public QVideoFilterRunnable {
    QOpenGLShaderProgram m_program;
    // ... Shader 处理逻辑
};
```

```qml
VideoOutput {
    id: videoOutput
    
    ShaderVideoFilter {
        brightness: shaderBrightness
        contrast: shaderContrast
        // ... 参数绑定
    }
}
```

#### 优点
1. **性能最优**：直接在视频渲染管线中处理，无额外拷贝
2. **专业级实现**：类似专业视频编辑软件的实现方式
3. **完全控制**：可以精确控制每一帧的处理

#### 缺点
1. **实现复杂**：需要深入理解 OpenGL/Vulkan 和 Qt Multimedia
2. **维护成本高**：需要处理不同平台的兼容性问题
3. **调试困难**：GPU 代码调试复杂
4. **Qt 版本依赖**：不同 Qt 版本的 API 可能有变化

#### 性能评估
- 额外 GPU 开销：约 3-8%（仅 Shader 处理）
- 内存开销：几乎无额外开销

#### 实现复杂度
- **高**（约 1-2 周）
- 主要工作：创建 C++ VideoFilter 组件，编写 GLSL Shader，处理平台兼容性

---

### 方案三：逐帧提取 + Shader 处理方案

#### 实现原理
使用 `QVideoSink`（Qt 6 新 API）提取视频帧，转换为 QImage，应用 Shader 后显示。

```qml
VideoOutput {
    id: videoOutput
    videoSink: videoSink
}

VideoSink {
    id: videoSink
    
    onVideoFrameChanged: {
        var frame = videoSink.videoFrame
        // 转换为 Image，应用 Shader，显示
    }
}
```

#### 优点
1. **完全控制**：可以精确控制每一帧的处理
2. **灵活性强**：可以实现复杂的帧处理逻辑

#### 缺点
1. **性能差**：CPU/GPU 数据传输开销大
2. **实时性差**：帧率可能下降到 15fps 以下
3. **内存占用高**：每帧都需要 CPU 内存

#### 性能评估
- 帧率：可能下降到 10-20fps
- CPU 占用：高（30-50%）
- 内存开销：高（每帧 8MB+）

#### 实现复杂度
- **中等**（约 3-5 天）
- 不推荐：性能问题严重

---

### 方案四：FFmpeg 硬件加速 + Vulkan 方案

#### 实现原理
使用 FFmpeg 的硬件解码（D3D11VA/VAAPI），直接在 GPU 上处理视频帧，避免 CPU 传输。

```cpp
// FFmpeg 硬件解码
AVBufferRef* hwDeviceCtx;
av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_D3D11VA, ...);

// Vulkan 计算 Shader 处理
VkImage videoFrame;  // 硬件解码后的帧
vkCmdDispatch(computeShader);  // 应用 Shader 效果
```

#### 优点
1. **性能最高**：全程 GPU 处理，无 CPU 瓶颈
2. **专业级**：类似专业视频处理软件的实现
3. **可扩展**：支持更复杂的视频处理功能

#### 缺点
1. **实现极其复杂**：需要深入理解 FFmpeg、Vulkan、D3D11
2. **平台依赖强**：不同平台需要不同的实现
3. **开发周期长**：可能需要数周时间
4. **维护成本极高**：底层代码难以维护

#### 性能评估
- 额外 GPU 开销：约 2-5%
- CPU 占用：极低（<5%）

#### 实现复杂度
- **极高**（约 2-4 周）
- 不推荐：投入产出比低

---

## 推荐方案

### 推荐：方案一（ShaderEffectSource + ShaderEffect）

#### 推荐理由

1. **投入产出比最高**：
   - 实现简单，开发周期短
   - 效果好，能满足用户需求
   - 维护成本低

2. **技术风险低**：
   - 纯 QML 实现，不涉及底层图形 API
   - 复用现有 Shader 代码
   - 跨平台兼容性好

3. **性能可接受**：
   - 额外开销约 10%，在可接受范围内
   - 现代显卡性能充足

4. **效果一致性保证**：
   - 预览和导出使用相同的 Shader 代码
   - 无需担心算法差异

#### 实现步骤

##### 第一阶段：视频预览 Shader 支持（2 天）

1. **修改 MediaViewerWindow.qml**
   - 添加 `ShaderEffectSource` 捕获视频内容
   - 添加 `FullShaderEffect` 处理视频帧
   - 处理视频播放状态切换

2. **修改 EmbeddedMediaViewer.qml**
   - 同样的修改逻辑
   - 确保内嵌模式和独立窗口模式一致

##### 第二阶段：视频导出 Shader 一致性（1 天）

1. **修改 VideoProcessor.cpp**
   - 使用 GPU Shader 处理替代 CPU 图像处理
   - 或使用相同的 GLSL Shader 代码

2. **添加 Shader 导出服务**
   - 复用 `OffscreenShaderRenderer` 的逻辑
   - 逐帧提取视频帧，应用 Shader，编码输出

##### 第三阶段：测试和优化（1 天）

1. **功能测试**
   - 参数调整实时预览
   - 预设风格切换
   - 发送消息后效果一致性

2. **性能测试**
   - 不同分辨率视频的帧率
   - 内存占用监控
   - GPU 占用监控

3. **兼容性测试**
   - Windows (D3D11/Vulkan)
   - 不同显卡驱动

#### 风险和缓解措施

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| ShaderEffectSource 性能问题 | 中 | 中 | 添加性能监控，必要时降级到原始显示 |
| 平台兼容性问题 | 低 | 高 | 添加平台检测，不支持时回退 |
| 显存不足 | 低 | 中 | 添加显存检测，大视频时降低处理分辨率 |

## 备选方案

如果方案一性能不达标，可以考虑：

### 方案二简化版：QQuickVideoTexture（Qt 6.8+）

Qt 6.8 引入了 `QQuickVideoTexture`，可以更高效地将视频帧作为纹理使用：

```qml
VideoOutput {
    videoTextureProvider: VideoTextureProvider {
        // 直接提供纹理给 ShaderEffect
    }
}
```

这需要检查 Qt 6.10.2 是否支持此 API。

## 总结

| 方案 | 实现复杂度 | 性能 | 效果一致性 | 推荐度 |
|------|-----------|------|-----------|--------|
| 方案一：ShaderEffectSource | 低 | 中 | 高 | ⭐⭐⭐⭐⭐ |
| 方案二：VideoFilter | 高 | 高 | 高 | ⭐⭐⭐ |
| 方案三：逐帧提取 | 中 | 低 | 中 | ⭐ |
| 方案四：Vulkan加速 | 极高 | 极高 | 高 | ⭐⭐ |

**最终推荐：方案一（ShaderEffectSource + ShaderEffect）**

该方案在实现复杂度、性能、效果一致性之间取得了最佳平衡，能够满足用户需求，同时保持代码的可维护性。
