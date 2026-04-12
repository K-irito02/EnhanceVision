# 左侧边栏拉伸时右侧内容抖动/闪动问题 — 修复计划

## 问题现象

- **拉伸左侧边栏（窗口左边缘）**：右侧所有内容（标题栏按钮、底部操作按钮、消息卡片右侧按钮和信息、控制面板等）严重抖动、闪动、跳动
- **拉伸右侧边栏（窗口右边缘）**：一切正常，滑动平滑舒适
- 不对称行为表明问题与 **窗口左边缘移动导致的全局坐标变化** 密切相关

---

## 根因分析

### 布局架构总览

```
App.qml (FocusScope)
└── ColumnLayout
    ├── TitleBar (height:48, fillWidth)          ← 标题栏
    └── RowLayout (fillWidth, fillHeight)
        ├── sidebarContainer (Item, preferredWidth动态)  ← 左侧边栏容器
        │   └── Sidebar (anchors.fill)
        ├── StackLayout (fillWidth, fillHeight)           ← 主内容区
        │   └── MainPage
        │       ├── messageAreaContainer
        │       ├── pendingFilePreviewArea
        │       └── 底部操作栏 (添加文件+发送按钮)
        └── ControlPanel (preferredWidth动态)             ← 右侧面板
```

### 根因 #1（关键）：TitleBar `onWidthChanged` 每像素调用 C++（最高优先级）

**文件**: [TitleBar.qml:52-54](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/TitleBar.qml#L52-L54)

```qml
onWidthChanged: {
    updateExcludeRegions()  // ← 每次宽度变化立即调用！
}
```

`updateExcludeRegions()` 内部连续调用：
1. `WindowHelper.clearExcludeRegions()` — C++ 调用
2. `WindowHelper.addExcludeRegion(...)` — C++ 调用（左侧按钮区）
3. `WindowHelper.addExcludeRegion(...)` — C++ 调用（右侧按钮区）

**影响**: 窗口从左边缘拉伸时，每移动 1 像素就触发 3 次 QML→C++ 边界穿越。而其他组件（EmbeddedMediaViewer、ShaderParamsPanel 等）已使用 `Qt.callLater` 延迟调用，唯独 TitleBar 是**同步即时调用**。

### 根因 #2（重要）：sidebarResizeHandle 使用绝对位置绑定

**文件**: [App.qml:630-631](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/App.qml#L630-L631)

```qml
Rectangle {
    id: sidebarResizeHandle
    x: sidebarContainer.x + sidebarContainer.width - width  // ← 依赖 x 坐标
    y: titleBar.height
}
```

当窗口左边缘移动时，`sidebarContainer.x` 随之变化，触发 handle 的 `x` 绑定重算。此绑定链在每帧 resize 事件中被重新求值。

### 根因 #3（中等）：MessageItem 大量宽度阈值判断导致布局跳变

**文件**: [MessageItem.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/MessageItem.qml)

多处使用 `root.width < 350`、`root.width > 280`、`root.width > 320`、`root.width >= 350` 等条件控制：
- 按钮可见性 (`visible:`)
- 文字显示内容（完整 vs 缩写）
- 图标尺寸 (`iconSize: root.width < 350 ? 12 : 14`)
- 按钮尺寸 (`btnSize: root.width < 350 ? 22 : 26`)
- 字体大小 (`font.pixelSize: root.width < 350 ? 10 : 11`)

在拖拽过程中反复跨越这些阈值，会导致元素**突然出现/消失/改变尺寸**，视觉上表现为"跳动"。

### 根因 #4（辅助）：左右不对称的布局重计算特性

| 操作 | 窗口 x | 窗口 width | 内容 x 坐标 | 视觉效果 |
|------|--------|-----------|------------|---------|
| 右边缘拉伸 | 不变 | 变化 | 不变（左锚定） | 平滑 |
| 左边缘拉伸 | 变化 | 变化 | 全部偏移 | 需要更多重绘 |

左边缘拉伸时所有 Item 的绝对坐标都变化，Layout 引擎需要做更多工作。上述根因 #1-3 在此场景下被放大。

---

## 修复方案

### 修复步骤 1：TitleBar exclude regions 更新改为延迟调用（关键修复）

**文件**: [TitleBar.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/TitleBar.qml#L52-L54)

**改动**: 将 `onWidthChanged` 中的同步调用改为 `Qt.callLater` 延迟调用，与其他组件保持一致：

```qml
// 修改前
onWidthChanged: {
    updateExcludeRegions()
}

// 修改后
onWidthChanged: Qt.callLater(updateExcludeRegions)
```

**理由**:
- 排除区域用于 Windows DWM 自定义标题栏的命中测试，不需要实时精确到每一帧
- `Qt.callLater` 将调用合并到当前事件循环末尾，避免每像素触发一次 C++ 调用
- EmbeddedMediaViewer、MediaViewerWindow、ShaderParamsPanel 已全部采用此模式

### 修复步骤 2：sidebarResizeHandle 改用锚定布局替代绝对位置绑定

**文件**: [App.qml:629-637](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/App.qml#L629-L637)

**改动**: 将 `x` 的显式绑定替换为 `anchors`:

```qml
// 修改前
Rectangle {
    id: sidebarResizeHandle
    x: sidebarContainer.x + sidebarContainer.width - width
    y: titleBar.height
    ...
}

// 修改后
Rectangle {
    id: sidebarResizeHandle
    anchors.left: sidebarContainer.right
    anchors.leftMargin: -width  // 使 handle 居中于边界线上
    anchors.top: titleBar.bottom
    anchors.bottom: parent.bottom
    ...
}
```

**理由**:
- `anchors` 由 Qt Quick 布局引擎原生优化，比 JavaScript 表达式绑定更高效
- 消除了 `sidebarContainer.x + sidebarContainer.width - width` 的运行时表达式求值
- 锚定关系是声明式的，布局引擎可以更好地优化依赖图

### 修复步骤 3：MessageItem 宽度响应式断点增加过渡缓冲

**文件**: [MessageItem.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/components/MessageItem.qml)

**策略 A（推荐）— 统一断点值 + 行为分组**:
将散乱的断点值（280、320、350）统一为合理的阶梯式断点体系，并确保同一层级的元素使用相同阈值：

| 当前断点 | 统一后 | 影响范围 |
|---------|--------|---------|
| 280, 320, 350 | 300, 360 | 时间信息可见性 |
| 350 | 360 | 按钮/文字缩写切换 |

**策略 B（补充）— 对尺寸变化使用 Behavior 平滑过渡**:
对因宽度变化导致的 `iconSize`、`btnSize`、`font.pixelSize` 等数值属性添加 `Behavior on xxx { NumberAnimation { duration: 0 } }`（duration:0 即禁用动画但避免跳变感），或改用 `opacity` 渐隐渐现代替直接 visible 切换。

> 注：策略 A 为必须执行，策略 B 可选（视实际效果决定是否需要）。

### 修复步骤 4（可选增强）：窗口 resize 期间全局禁用非必要更新

**文件**: [App.qml](file:///e:/QtAudio-VideoLearning/EnhanceVision/qml/App.qml#L64-L74)

**改动**: 在已有的 `WindowHelper.onResizeStarted/Finished` 连接中，增加对 TitleBar 和各组件 exclude regions 更新的节流控制：

```qml
Connections {
    target: WindowHelper
    function onResizeStarted() {
        sidebarAnimation.enabled = false
        controlPanelAnimation.enabled = false
        // 新增：标记正在 resize，组件可据此跳过非必要更新
        root._isResizing = true
    }
    function onResizeFinished() {
        sidebarAnimation.enabled = true
        controlPanelAnimation.enabled = true
        root._isResizing = false
        // resize 结束后做一次最终的 exclude regions 更新
        if (titleBar) titleBar.updateExcludeRegions()
    }
}
```

同时在 TitleBar 中：
```qml
onWidthChanged: {
    if (!root._isResizing) {
        Qt.callLater(updateExcludeRegions)
    }
    // resize 期间完全跳过，resizeFinished 时统一更新一次即可
}
```

---

## 实施顺序

| 步骤 | 优先级 | 复杂度 | 预期效果 |
|------|--------|--------|---------|
| 步骤 1: TitleBar Qt.callLater | P0（关键） | 极低（改 1 行） | 消除每像素 C++ 调用，显著减少抖动 |
| 步骤 2: Handle 锚定重构 | P1（高） | 低（改 ~5 行） | 优化绑定链，减少运行时求值 |
| 步骤 3: MessageItem 断点统一 | P2（中） | 中（改 ~15 处） | 消除阈值交叉时的跳变 |
| 步骤 4: Resize 节流 | P3（增强） | 低（改 ~10 行） | 进一步减少 resize 期间的非必要工作 |

## 验证方法

1. 手动拖拽窗口**左边缘**进行拉伸/收缩，观察右侧内容是否平滑
2. 手动拖拽窗口**右边缘**进行拉伸/收缩，确认未引入回归
3. 快速来回拖拽左边缘，检查是否有残影或延迟异常
4. 拖拽侧边栏**内部调整手柄**（sidebarResizeHandle），确认功能不受影响
5. 在不同窗口尺寸下（窄/中/宽）验证标题栏按钮的命中测试正常
6. 验证消息卡片在不同宽度下的响应式表现自然
