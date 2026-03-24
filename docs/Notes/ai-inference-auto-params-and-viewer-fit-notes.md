# AI 推理模式自动参数 & 查看器适配修复笔记

## 概述

**创建日期**: 2026-03-24

本次改动涵盖两个独立方向：

1. **AI 推理自动参数**：为分块大小（tileSize）等依赖图像尺寸的参数实现「默认自动、可手动覆盖」机制。
2. **媒体查看器适配**：确保超分辨率处理后的大图在嵌入式窗口和独立窗口中均能完整显示，不溢出边界。

---

## 一、变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/models/DataTypes.h` | `AIParams` 新增 `autoTileSize` 布尔字段 |
| `include/EnhanceVision/core/AIEngine.h` | 新增 `computeAutoTileSize(QSize)` 方法声明及 `autoParamsComputed` 信号 |
| `src/core/AIEngine.cpp` | 实现 `computeAutoTileSize()`；`process()` 中改用自动分块决策逻辑 |
| `qml/components/AIParamsPanel.qml` | 分块大小控件改为自动/手动双模式，新增媒体尺寸感知属性 |
| `qml/components/EmbeddedMediaViewer.qml` | `FullShaderEffect` 改为 `anchors.fill: parent`，保留 `clip: true` |
| `qml/components/MediaViewerWindow.qml` | `contentArea`、`videoContainer` 增加 `clip: true`；`FullShaderEffect` 改为 fill parent |

---

## 二、AI 推理自动参数

### 2.1 问题背景

原实现中 `tileSize` 直接使用 `ModelInfo.tileSize`（来自 `models.json` 的静态值），对于不同分辨率的图像可能导致：

- 低分辨率图像被强制分块 → 多余的边界伪影 + 不必要的性能损耗。
- 高分辨率图像分块过大 → 超出 GPU 显存导致推理失败或崩溃。

### 2.2 设计方案

采用「**默认自动，支持手动覆盖**」的双模式设计：

- **自动模式（默认）**：应用程序在推理前调用 `computeAutoTileSize(inputSize)` 计算最优分块大小，用户无需关心。
- **手动模式**：用户通过 `AIParamsPanel` 中的开关切换到手动模式，可精确指定分块大小（64~1024 px，步长 64）。
- **一键重置**：在手动模式下提供「重置为自动」按钮，可随时回到自动模式。

### 2.3 实现细节

#### `DataTypes.h` — AIParams 新增字段

```cpp
struct AIParams {
    int tileSize = 0;          ///< 自定义分块大小（0=自动）
    bool autoTileSize = true;  ///< true=自动计算，false=使用 tileSize 手动值
    // ...
};
```

#### `AIEngine::computeAutoTileSize(QSize inputSize)` 算法

```
优先级决策树：
1. model.tileSize == 0  →  返回 0（模型声明不分块）
2. GPU 模式可用显存默认估算 2048 MB，CPU 模式 512 MB
3. 每块显存消耗估算：
   mem(tile) = tile² × channels × sizeof(float) × scale² × kFactor(6.0)
4. 在 [64, min(1024, max(W,H))] 范围，步长 64，找最大满足 mem ≤ availableMB 的 tile
5. 若 W <= bestTile && H <= bestTile  →  返回 0（整图一次推理，无需分块）
```

#### `AIEngine::process()` 分块决策逻辑

```cpp
// 优先级：用户手动设置 > 自动计算 > 0（不分块）
if (effectiveParams["tileSize"] > 0) {
    tileSize = effectiveParams["tileSize"].toInt();  // 手动模式
} else {
    tileSize = computeAutoTileSize(input.size());    // 自动模式
    emit autoParamsComputed(tileSize);               // 通知 UI 更新显示
}
```

#### `autoParamsComputed` 信号

每次推理前，若使用自动分块，会发出 `autoParamsComputed(int autoTileSize)` 信号。`AIParamsPanel` 通过 `Connections` 接收并实时更新 UI 中的「自动 (xxx px)」显示。

### 2.4 AIParamsPanel.qml 交互设计

| 状态 | UI 表现 |
|------|---------|
| 自动模式（默认） | 标题显示「自动 (256 px)」，滑块半透明不可操作 |
| 自动模式但未有图像 | 标题显示「自动（不分块）」 |
| 手动模式 | 标题显示实际 px 值，滑块可操作，显示「重置为自动」按钮 |
| 切换模型 | 自动重置为自动模式，`computedAutoTileSize` 清零后重新预计算 |

外部通过 `currentMediaSize` 属性绑定当前选中文件的图像尺寸，实现实时预计算。

---

## 三、媒体查看器溢出修复

### 3.1 问题根源

超分辨率模型（如 2x、4x）会将输出图像放大为原始的 2~4 倍。若查看器容器未正确约束图像渲染区域，会导致：

- 图像在窗口内溢出到标题栏、控制栏甚至窗口外。
- 在嵌入式模式下遮挡消息列表等其他组件。

### 3.2 修复方案

#### 根本保证：`fillMode: Image.PreserveAspectFit`

QML 的 `Image` 组件只要设置 `fillMode: Image.PreserveAspectFit` 且 `anchors.fill: parent`，图像就会按比例缩放到父容器内，**不会超出父容器**。所有图像组件均已使用此模式。

#### 关键修复 1：`FullShaderEffect` 锚点

**原代码（有问题）**：
```qml
Image { id: imgSrc; visible: false; ... }
FullShaderEffect { anchors.fill: imgSrc; source: imgSrc; ... }
```

**问题**：`imgSrc` 设为 `visible: false` 时，其布局尺寸依然基于图像的 `implicitSize`（即原始像素大小），可能大于父容器，导致 `FullShaderEffect` 跟随溢出。

**修复后**：
```qml
Image { id: imgSrc; anchors.fill: parent; visible: false; fillMode: Image.PreserveAspectFit; ... }
FullShaderEffect { anchors.fill: parent; source: imgSrc; ... }  // fill parent，不跟随 imgSrc
```

#### 关键修复 2：容器 `clip: true`

在 `MediaViewerWindow.qml` 的 `contentArea` 和 `videoContainer` 上增加 `clip: true`，作为防御性边界，防止任何子元素渲染溢出。

```qml
Item {
    id: contentArea
    clip: true  // 新增：截断所有超出容器的子渲染
    // ...
}
```

`EmbeddedMediaViewer.qml` 的 `MediaContentArea` 已有 `clip: true`，无需修改。

### 3.3 修复覆盖范围

| 组件 | 修复内容 |
|------|----------|
| `EmbeddedMediaViewer` - 嵌入式图像层 | `FullShaderEffect` 改为 `fill parent` |
| `MediaViewerWindow` - 独立窗口图像区 | `contentArea` 增加 `clip: true`；`FullShaderEffect` 改为 `fill parent` |
| `MediaViewerWindow` - 独立窗口视频区 | `videoContainer` 增加 `clip: true` |

---

## 四、遇到的问题及解决方案

### 问题 1：NCNN 无法获取实时 GPU 剩余显存

NCNN 的 Vulkan 后端没有暴露「当前剩余显存」查询接口。

**解决方案**：使用保守固定值（GPU 模式 2048 MB，CPU 模式 512 MB）作为估算基准。`kFactor=6.0` 已包含安全裕量，实测低端 4 GB GPU 上不会 OOM。后续可考虑通过 `VkPhysicalDeviceMemoryProperties` 查询实际值。

### 问题 2：切换模型后自动分块值需即时更新

用户在选择了文件后切换模型，此时需要立即重新预计算分块大小。

**解决方案**：`_updateModelInfo()` 函数在切换模型时，若 `currentMediaSize` 有效（宽高 > 0），立即调用 `engine.computeAutoTileSize()` 更新 `computedAutoTileSize`。

---

## 五、测试验证要点

| 场景 | 预期结果 |
|------|----------|
| 小图（< 256px）+ 自动模式 | `computeAutoTileSize` 返回 0，整图一次推理 |
| 大图（> 1024px）+ 自动模式 | 返回合适分块大小，分块推理无溢出 |
| 手动模式切换滑块 | 滑块可操作，值实时更新，显示「重置为自动」按钮 |
| 切换模型 | 自动重置为自动模式，UI 标签更新 |
| 4x 超分结果在嵌入式窗口查看 | 图像完整显示在窗口内，不溢出 |
| 4x 超分结果在独立窗口查看 | 图像完整显示，拖拽调整窗口大小后自适应 |
| 独立窗口全屏 | 图像自适应全屏，无溢出 |

---

## 六、后续工作

- [ ] 通过 `VkPhysicalDeviceMemoryProperties` 获取真实 GPU 可用显存，替换固定估算值。
- [ ] 为视频帧逐帧推理提供类似的自动分块感知（视频分辨率在推理前已知）。
- [ ] 考虑将 `computeAutoTileSize` 结果写入推理日志，便于性能调优。
