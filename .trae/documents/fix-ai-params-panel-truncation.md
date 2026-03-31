# Bug修复计划：AI控制面板"模型参数"面板被截断问题

## 问题描述

右侧AI控制面板下的底部"模型参数"面板有时不会显示，感觉像是被截断了。这个问题出现的概率较小。

## 问题根因分析

### 1. 布局结构分析

**当前布局层次**：
```
App.qml
└── ControlPanel (宽度280px, clip: true)
    └── ColumnLayout (主布局)
        └── StackLayout (参数区域)
            └── ColumnLayout (aiContentLayout - AI模式)
                ├── AIModelPanel (Layout.fillHeight: true) ← 占用所有可用高度
                ├── Rectangle (分隔线)
                └── AIParamsPanel (Layout.preferredHeight: implicitHeight) ← 可能被截断
```

### 2. 核心问题

1. **`AIModelPanel` 的 `Layout.fillHeight: true` 问题**：
   - 模型选择面板会尽可能占用所有可用高度
   - 当 `AIParamsPanel` 内容较多时，可能没有足够空间显示

2. **`AIParamsPanel` 高度计算问题**：
   - 使用 `Layout.preferredHeight: aiParamsPanel.implicitHeight`
   - `implicitHeight` 是动态计算的，在布局更新时可能存在时序问题
   - 当模型参数动态加载/变化时，高度计算可能不及时

3. **缺少滚动机制**：
   - `aiContentLayout` 没有滚动支持
   - 当内容超出可见区域时，底部内容被截断无法滚动查看

4. **潜在的竞态条件**：
   - `AIParamsPanel` 的 `implicitHeight` 依赖其内容（模型信息卡片、自动优化状态栏、GPU开关、分块大小、模型特定参数等）
   - 这些内容的显示/隐藏和高度变化是动态的
   - 布局系统可能在 `implicitHeight` 更新前就完成了空间分配

### 3. 触发条件（概率小的原因）

这个问题可能由以下条件触发：
- 窗口高度较小
- 选择了具有多个可配置参数的模型
- 模型信息卡片内容较长
- 布局更新时序恰好出现竞态

## 修复方案

### 方案：为AI内容区域添加滚动支持

**修改文件**：`qml/components/ControlPanel.qml`

**修改内容**：

将 AI 模式的 `ColumnLayout` (`aiContentLayout`) 包装在 `ScrollView` 中，确保当内容超出可见区域时可以滚动查看。

**具体修改**：

```qml
// 修改前（第291-327行）
ColumnLayout {
    id: aiContentLayout
    Layout.fillWidth: true
    Layout.fillHeight: true
    spacing: 10

    Components.AIModelPanel { ... }
    Rectangle { ... }
    Components.AIParamsPanel { ... }
}

// 修改后
ScrollView {
    id: aiScrollView
    Layout.fillWidth: true
    Layout.fillHeight: true
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ColumnLayout {
        id: aiContentLayout
        width: aiScrollView.availableWidth - 4
        spacing: 10

        Components.AIModelPanel {
            id: aiModelPanel
            Layout.fillWidth: true
            Layout.preferredHeight: aiModelPanel.implicitHeight
            // 移除 Layout.fillHeight: true
            ...
        }

        Rectangle { ... }

        Components.AIParamsPanel {
            id: aiParamsPanel
            Layout.fillWidth: true
            // 保持 implicitHeight 自动计算
            ...
        }
    }
}
```

### 关键修改点

1. **添加 `ScrollView` 包装**：
   - 为整个AI内容区域提供滚动支持
   - 设置 `clip: true` 防止内容溢出

2. **调整 `AIModelPanel` 高度策略**：
   - 移除 `Layout.fillHeight: true`
   - 改为 `Layout.preferredHeight: aiModelPanel.implicitHeight`
   - 让模型列表根据内容自适应高度，而不是占用所有空间

3. **设置宽度约束**：
   - `aiContentLayout.width: aiScrollView.availableWidth - 4`
   - 确保内容宽度适应滚动视图

## 实施步骤

1. 修改 `qml/components/ControlPanel.qml` 文件
2. 将 AI 模式的 `ColumnLayout` 包装在 `ScrollView` 中
3. 调整 `AIModelPanel` 的高度策略
4. 构建并测试验证

## 验证要点

1. 选择不同模型，确认模型参数面板始终可见
2. 调整窗口大小，确认滚动条正确显示/隐藏
3. 确认模型列表滚动和参数面板滚动不冲突
4. 确认布局在不同窗口高度下表现正常

## 风险评估

- **风险等级**：低
- **影响范围**：仅影响AI控制面板的布局
- **回退方案**：恢复原有布局结构
