# AI推理模式参数自动优化设计方案

## 一、需求分析

### 1.1 用户需求
1. **参数自动调整**：每个模型的某些参数应该保持默认自动（支持用户手动调整），应用程序根据用户上传的多媒体文件自动后台调整参数
2. **窗口显示完整性**：处理后的多媒体文件必须能够完整显示在放大查看窗口中，不因参数影响而超出窗口

### 1.2 当前系统分析

#### 已实现的自动功能
| 功能 | 实现位置 | 说明 |
|------|---------|------|
| 自动分块大小计算 | `AIEngine::computeAutoTileSize()` | 根据图像尺寸、显存自动计算 |
| GPU可用性检测 | `AIEngine::detectGpu()` | 初始化时检测Vulkan |
| 参数范围验证 | `AIEngine::mergeWithDefaultParams()` | 自动修正超出范围的参数 |

#### 存在的问题
1. **窗口尺寸固定**：默认900×650，不考虑图像实际尺寸和放大倍数
2. **缺少输出尺寸预估**：用户不知道处理后的图像会有多大
3. **部分参数未自动优化**：如`outscale`可能导致输出图像过大
4. **视频处理参数不完善**：缺少针对视频的自动参数调整

---

## 二、参数分类与自动策略

### 2.1 参数分类定义

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           AI参数自动调整策略                                  │
└─────────────────────────────────────────────────────────────────────────────┘

                    ┌───────────────────┐
                    │   所有模型参数     │
                    └─────────┬─────────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
              ▼               ▼               ▼
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │  完全自动型   │  │  半自动型     │  │  手动型       │
    │  (Auto)      │  │  (Semi-Auto) │  │  (Manual)    │
    └──────────────┘  └──────────────┘  └──────────────┘
    • tileSize        • outscale         • modelId
    • useGpu          • noise_level      • modelParams
    • tilePadding     • time_step          (模型特定)
                      • render_factor
```

### 2.2 各模型参数详细配置

#### 2.2.1 超分辨率模型 (SuperResolution)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `tileSize` | int | **完全自动** | 图像尺寸、显存、scaleFactor | 0(不分块) 或 64-1024 |
| `tilePadding` | int | 固定值 | 模型默认(10) | 0-32 |
| `outscale` | float | **半自动** | 输出尺寸上限、窗口尺寸 | 1.0-4.0 |
| `face_enhance` | bool | 手动 | 用户选择 | true/false |
| `tta_mode` | bool | 手动 | 用户选择 | true/false |

**outscale 自动计算逻辑**：
```cpp
// 计算建议的 outscale，确保输出图像不超过最大显示尺寸
float computeAutoOutscale(const QSize& inputSize, int modelScale, int maxDisplaySize) {
    // 输出尺寸 = 输入尺寸 × outscale
    // 限制：输出尺寸 ≤ maxDisplaySize
    float maxOutscale = (float)maxDisplaySize / std::max(inputSize.width(), inputSize.height());
    // outscale 不能超过模型支持的放大倍数
    float effectiveOutscale = std::min(maxOutscale, (float)modelScale);
    // outscale 不能小于1
    return std::max(1.0f, effectiveOutscale);
}
```

#### 2.2.2 去噪模型 (Denoising)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `tileSize` | int | **完全自动** | 图像尺寸、显存 | 0 或 64-1024 |
| `noise_level` | int | **半自动** | 图像噪声分析(可选) | 0-10 |
| `tta_mode` | bool | 手动 | 用户选择 | true/false |

**noise_level 自动估算逻辑**（可选高级功能）：
```cpp
// 简单噪声估计：计算图像局部方差
int estimateNoiseLevel(const QImage& image) {
    // 使用拉普拉斯算子估计噪声
    // 返回建议的 noise_level 值
}
```

#### 2.2.3 去模糊模型 (Deblurring)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `tileSize` | int | **完全自动** | 图像尺寸、显存 | 0 或 64-1024 |

#### 2.2.4 去雾模型 (Dehazing)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `tileSize` | int | **完全自动** | 图像尺寸、显存 | 0 或 64-1024 |

#### 2.2.5 上色模型 (Colorization)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `render_factor` | int | **半自动** | 图像尺寸 | 7-45 |
| `artistic_mode` | bool | 手动 | 用户选择 | true/false |

**render_factor 自动计算逻辑**：
```cpp
// render_factor 影响输出细节程度
// 较大图像使用较大的 render_factor
int computeAutoRenderFactor(const QSize& inputSize) {
    int pixels = inputSize.width() * inputSize.height();
    // 小图 (< 512×512): render_factor = 15-25
    // 中图 (512×512 - 1024×1024): render_factor = 25-35
    // 大图 (> 1024×1024): render_factor = 35-45
    if (pixels < 512 * 512) return 20;
    if (pixels < 1024 * 1024) return 30;
    return 40;
}
```

#### 2.2.6 低光增强模型 (LowLight)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `enhancement_strength` | float | **半自动** | 图像亮度分析 | 0.0-1.0 |
| `exposure_correction` | float | **半自动** | 图像曝光分析 | -1.0-1.0 |

**自动亮度分析逻辑**：
```cpp
// 分析图像平均亮度，自动调整增强强度
float computeAutoEnhancementStrength(const QImage& image) {
    // 计算图像平均亮度
    float avgBrightness = calculateAverageBrightness(image);
    // 亮度越低，增强强度越高
    // 亮度范围 0-1，建议强度范围 0.3-0.8
    return std::clamp(1.0f - avgBrightness, 0.3f, 0.8f);
}
```

#### 2.2.7 帧插值模型 (FrameInterpolation)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `time_step` | float | 手动 | 用户选择 | 0.0-1.0 |
| `uhd_mode` | bool | **半自动** | 视频分辨率 | true/false |
| `tta_spatial` | bool | 手动 | 用户选择 | true/false |
| `tta_temporal` | bool | 手动 | 用户选择 | true/false |

**uhd_mode 自动判断逻辑**：
```cpp
// 分辨率 ≥ 4K 时自动启用 UHD 模式
bool shouldEnableUhdMode(const QSize& videoSize) {
    return videoSize.width() >= 3840 || videoSize.height() >= 2160;
}
```

#### 2.2.8 图像修复模型 (Inpainting)

| 参数名 | 类型 | 自动策略 | 计算依据 | 范围限制 |
|--------|------|---------|---------|---------|
| `tileSize` | int | **完全自动** | 图像尺寸、显存 | 0 或 64-1024 |

---

## 三、窗口显示优化设计

### 3.1 窗口尺寸智能调整

#### 3.1.1 设计原则
1. **自适应初始尺寸**：根据图像/视频尺寸智能设置窗口初始大小
2. **最大尺寸限制**：不超过屏幕工作区的80%
3. **最小尺寸保障**：确保控制按钮可点击（480×360）
4. **放大倍数感知**：考虑AI处理后的输出尺寸

#### 3.1.2 窗口尺寸计算逻辑

```cpp
// 计算建议的窗口尺寸
QSize computeOptimalWindowSize(const QSize& mediaSize, const QSize& screenSize, float scaleFactor) {
    // 计算输出尺寸（考虑放大倍数）
    QSize outputSize(
        static_cast<int>(mediaSize.width() * scaleFactor),
        static_cast<int>(mediaSize.height() * scaleFactor)
    );
    
    // 屏幕工作区（留出任务栏和边距）
    QSize maxWindowSize(
        static_cast<int>(screenSize.width() * 0.8),
        static_cast<int>(screenSize.height() * 0.8)
    );
    
    // 最小窗口尺寸
    const QSize minWindowSize(480, 360);
    
    // 计算适配尺寸
    int targetWidth = std::min(outputSize.width() + 32, maxWindowSize.width());   // +32 为边距
    int targetHeight = std::min(outputSize.height() + 120, maxWindowSize.height()); // +120 为标题栏+控制栏
    
    // 确保不小于最小尺寸
    targetWidth = std::max(targetWidth, minWindowSize.width());
    targetHeight = std::max(targetHeight, minWindowSize.height());
    
    return QSize(targetWidth, targetHeight);
}
```

#### 3.1.3 窗口尺寸分级策略

| 输出图像尺寸 | 窗口默认尺寸 | 说明 |
|-------------|-------------|------|
| ≤ 640×480 | 640×520 | 小图，窗口紧凑 |
| 640×480 - 1280×720 | 900×650 | 中图，当前默认值 |
| 1280×720 - 1920×1080 | 1280×800 | 大图，适当放大窗口 |
| 1920×1080 - 3840×2160 | 屏幕的70% | 超大图，限制窗口尺寸 |
| > 3840×2160 | 屏幕的80% | 极大图，最大窗口限制 |

### 3.2 图像显示适配优化

#### 3.2.1 当前实现分析

当前使用 `Image.PreserveAspectFit` 模式，图像自动缩放适应窗口：
```qml
Image {
    fillMode: Image.PreserveAspectFit  // 保持宽高比，自动适配
}
```

**优点**：图像始终完整显示，不会溢出
**缺点**：大图会被缩小，细节丢失；缺少缩放控制

#### 3.2.2 优化方案：增加缩放控制

```qml
// 新增属性
property real zoomLevel: 1.0          // 缩放级别
property bool autoFit: true           // 自适应模式
property real minZoom: 0.1            // 最小缩放
property real maxZoom: 10.0           // 最大缩放

// 计算自适应缩放级别
function computeAutoFitZoom() {
    if (!imageSource || imageSource.status !== Image.Ready) return 1.0
    var imgAspect = imageSource.sourceSize.width / imageSource.sourceSize.height
    var containerAspect = container.width / container.height
    if (imgAspect > containerAspect) {
        return container.width / imageSource.sourceSize.width
    } else {
        return container.height / imageSource.sourceSize.height
    }
}

// 鼠标滚轮缩放
MouseArea {
    onWheel: function(wheel) {
        if (wheel.modifiers & Qt.ControlModifier) {
            var delta = wheel.angleDelta.y / 120.0
            var newZoom = zoomLevel * Math.pow(1.1, delta)
            zoomLevel = Math.max(minZoom, Math.min(maxZoom, newZoom))
            autoFit = false
        }
    }
}
```

#### 3.2.3 显示模式切换

| 模式 | 快捷键 | 说明 |
|------|--------|------|
| 自适应 | `F` 或双击 | 图像完整显示在窗口内 |
| 1:1 | `1` | 像素级显示（100%缩放） |
| 放大 | `Ctrl++` | 放大10% |
| 缩小 | `Ctrl+-` | 缩小10% |

### 3.3 输出尺寸预估与警告

#### 3.3.1 处理前预估显示

在处理确认前，显示预估输出信息：

```
┌─────────────────────────────────────────────────────────────┐
│  输入: 1920×1080 (2.1 MP)                                   │
│  模型: Real-ESRGAN x4plus (4x放大)                          │
│  ─────────────────────────────────────────────────────────  │
│  预估输出: 7680×4320 (33.2 MP)                              │
│  ⚠️ 输出尺寸较大，建议调整 outscale 参数                      │
│                                                             │
│  [自动调整参数] [继续处理] [取消]                             │
└─────────────────────────────────────────────────────────────┘
```

#### 3.3.2 大尺寸警告阈值

| 输出尺寸 | 警告级别 | 建议 |
|---------|---------|------|
| > 4096×4096 | ⚠️ 警告 | 建议降低 outscale |
| > 8192×8192 | ⚠️⚠️ 严重警告 | 强烈建议降低 outscale |
| > 16384×16384 | ❌ 阻止 | 超出处理能力，必须调整参数 |

---

## 四、架构设计

### 4.1 新增类：AutoParamOptimizer

```cpp
/**
 * @brief AI参数自动优化器
 * 
 * 根据输入媒体特征自动计算最优参数
 */
class AutoParamOptimizer : public QObject {
    Q_OBJECT
    
public:
    explicit AutoParamOptimizer(QObject* parent = nullptr);
    
    /**
     * @brief 计算最优参数
     * @param modelInfo 模型信息
     * @param mediaInfo 媒体信息（尺寸、类型等）
     * @param userParams 用户手动设置的参数（优先级最高）
     * @return 优化后的参数
     */
    Q_INVOKABLE QVariantMap computeOptimalParams(
        const QVariantMap& modelInfo,
        const QVariantMap& mediaInfo,
        const QVariantMap& userParams
    );
    
    /**
     * @brief 预估输出尺寸
     */
    Q_INVOKABLE QSize estimateOutputSize(
        const QSize& inputSize,
        const QString& modelId,
        const QVariantMap& params
    );
    
    /**
     * @brief 计算最优窗口尺寸
     */
    Q_INVOKABLE QSize computeOptimalWindowSize(
        const QSize& mediaSize,
        const QSize& screenSize,
        float scaleFactor
    );
    
    /**
     * @brief 检查参数是否会导致问题
     * @return 警告信息列表（空列表表示无问题）
     */
    Q_INVOKABLE QStringList validateParams(
        const QString& modelId,
        const QVariantMap& params,
        const QSize& inputSize
    );
    
signals:
    /**
     * @brief 参数自动调整通知
     */
    void paramsAutoAdjusted(const QString& paramKey, const QVariant& oldValue, const QVariant& newValue);
    
private:
    // 各类参数的自动计算方法
    int computeAutoTileSize(const QSize& inputSize, const ModelInfo& model);
    float computeAutoOutscale(const QSize& inputSize, int modelScale, int maxDisplaySize);
    int computeAutoRenderFactor(const QSize& inputSize);
    float computeAutoEnhancementStrength(const QImage& image);
    bool shouldEnableUhdMode(const QSize& videoSize);
    
    // 辅助方法
    float calculateAverageBrightness(const QImage& image);
    qint64 estimateMemoryUsage(const QSize& tileSize, int channels, int scale);
};
```

### 4.2 参数优先级机制

```
参数值决策优先级（从高到低）：

1. 用户手动设置值（userParams）
   ↓ 如果未设置
2. 自动计算值（AutoParamOptimizer）
   ↓ 如果无法计算
3. 模型默认值（ModelInfo.supportedParams.default）
   ↓ 如果未定义
4. 系统全局默认值
```

### 4.3 数据流设计

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           参数自动优化流程                                    │
└─────────────────────────────────────────────────────────────────────────────┘

用户上传媒体文件
        │
        ▼
┌───────────────────┐
│ MediaAnalyzer     │ 分析媒体特征
│ • 尺寸            │
│ • 类型            │
│ • 帧率(视频)      │
│ • 亮度(可选)      │
└─────────┬─────────┘
          │
          ▼
┌───────────────────────────────────────────────────────────────┐
│                    AutoParamOptimizer                          │
│                                                                │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐  │
│  │ 完全自动参数    │  │ 半自动参数      │  │ 窗口尺寸计算    │  │
│  │ • tileSize     │  │ • outscale     │  │ • 初始尺寸      │  │
│  │ • useGpu       │  │ • noise_level  │  │ • 最大限制      │  │
│  │ • tilePadding  │  │ • render_factor│  │ • 缩放建议      │  │
│  └────────────────┘  └────────────────┘  └────────────────┘  │
│           │                  │                    │           │
│           └──────────────────┼────────────────────┘           │
│                              ▼                                │
│                    ┌─────────────────┐                       │
│                    │ 参数验证与警告   │                       │
│                    └─────────────────┘                       │
└──────────────────────────────┬────────────────────────────────┘
                               │
                               ▼
                    ┌─────────────────┐
                    │ AIParamsPanel   │ 显示优化后的参数
                    │ • 显示自动值     │
                    │ • 允许用户覆盖   │
                    │ • 显示警告信息   │
                    └─────────────────┘
```

---

## 五、UI改进设计

### 5.1 AIParamsPanel 增强

#### 5.1.1 参数状态显示

```
┌─────────────────────────────────────────────────────────────┐
│  分块大小                              自动 (512 px)  ↻     │
│  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━  │
│  [手动覆盖] 当前：自动                                      │
│  应用程序将根据图像尺寸和显存自动选择最佳分块大小。          │
└─────────────────────────────────────────────────────────────┘

参数状态图标：
• 🔵 自动计算值（用户未修改）
• 🟡 用户覆盖值（与自动值不同）
• ⚪ 固定值（无自动计算）
```

#### 5.1.2 输出预估面板

```
┌─────────────────────────────────────────────────────────────┐
│  📊 输出预估                                                 │
│  ─────────────────────────────────────────────────────────  │
│  输入尺寸: 1920 × 1080 (2.1 MP)                             │
│  输出尺寸: 7680 × 4320 (33.2 MP)                            │
│  放大倍数: 4×                                                │
│  预估内存: ~2.1 GB                                           │
│                                                             │
│  ⚠️ 输出尺寸较大，可能影响显示和处理速度                      │
│  [自动调整参数以优化]                                        │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 MediaViewerWindow 增强

#### 5.2.1 窗口标题栏信息

```
┌─────────────────────────────────────────────────────────────┐
│ image.png (1920×1080 → 7680×4320)              [嵌入][最小化][全屏][关闭] │
└─────────────────────────────────────────────────────────────┘
```

#### 5.2.2 缩放控制栏

```
┌─────────────────────────────────────────────────────────────┐
│  🔍 [适应] [100%] [200%]  ━━━━━━━━━━━━━●━━━━━━━━━━━  150%   │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、实现计划

### 6.1 阶段一：参数自动优化核心（优先级：高）

| 任务 | 文件 | 说明 |
|------|------|------|
| 创建 AutoParamOptimizer 类 | `AutoParamOptimizer.h/cpp` | 参数自动计算核心逻辑 |
| 增强 tileSize 自动计算 | `AIEngine.cpp` | 考虑更多因素（视频帧数等） |
| 实现 outscale 自动限制 | `AutoParamOptimizer.cpp` | 防止输出过大 |
| 参数验证与警告系统 | `AutoParamOptimizer.cpp` | 处理前检查 |

### 6.2 阶段二：窗口显示优化（优先级：高）

| 任务 | 文件 | 说明 |
|------|------|------|
| 窗口尺寸智能调整 | `MediaViewerWindow.qml` | 根据图像尺寸设置初始窗口 |
| 输出尺寸预估显示 | `AIParamsPanel.qml` | 处理前显示预估信息 |
| 大尺寸警告机制 | `ProcessingController.cpp` | 超过阈值时警告用户 |

### 6.3 阶段三：UI增强（优先级：中）

| 任务 | 文件 | 说明 |
|------|------|------|
| 参数状态可视化 | `AIParamsPanel.qml` | 显示自动/手动状态 |
| 缩放控制功能 | `MediaViewerWindow.qml` | 鼠标滚轮缩放、快捷键 |
| 输出预估面板 | `AIParamsPanel.qml` | 显示输入/输出尺寸对比 |

### 6.4 阶段四：高级功能（优先级：低）

| 任务 | 文件 | 说明 |
|------|------|------|
| 图像亮度分析 | `AutoParamOptimizer.cpp` | 自动调整低光增强参数 |
| 噪声分析 | `AutoParamOptimizer.cpp` | 自动调整去噪参数 |
| 视频参数优化 | `AutoParamOptimizer.cpp` | 针对视频的参数优化 |

---

## 七、测试要点

### 7.1 参数自动优化测试

| 测试场景 | 输入 | 预期行为 |
|---------|------|---------|
| 小图超分 | 320×240 | tileSize=0(不分块)，outscale=模型最大值 |
| 大图超分 | 4096×2160 | tileSize=自动计算，显示警告 |
| 极大图超分 | 8192×4320 | tileSize=自动计算，outscale自动限制，严重警告 |
| 视频帧插值 | 4K视频 | uhd_mode=true |

### 7.2 窗口显示测试

| 测试场景 | 输出尺寸 | 预期窗口行为 |
|---------|---------|-------------|
| 小输出 | 640×480 | 窗口紧凑，图像完整显示 |
| 中输出 | 1920×1080 | 窗口适中，图像完整显示 |
| 大输出 | 3840×2160 | 窗口最大化限制，图像缩小显示 |
| 超大输出 | 7680×4320 | 窗口最大化，图像大幅缩小，提示缩放控制 |

### 7.3 边界情况测试

| 测试场景 | 预期行为 |
|---------|---------|
| 显存不足 | 自动降低tileSize，提示用户 |
| 输出超限 | 阻止处理，强制调整参数 |
| GPU不可用 | 自动切换CPU模式，调整参数 |

---

## 八、总结

本设计方案通过以下措施实现AI推理模式的参数自动优化和窗口显示完整性：

1. **参数自动优化**：
   - 创建 `AutoParamOptimizer` 类统一管理参数自动计算
   - 建立参数分类体系（完全自动/半自动/手动）
   - 实现参数优先级机制

2. **窗口显示保障**：
   - 智能窗口尺寸调整
   - 输出尺寸预估与警告
   - 增加缩放控制功能

3. **用户体验提升**：
   - 参数状态可视化
   - 处理前预估信息显示
   - 智能警告和建议

该方案确保每个模型的每个参数都有明确的自动策略，同时保证处理后的多媒体文件能够完整显示在窗口中。
