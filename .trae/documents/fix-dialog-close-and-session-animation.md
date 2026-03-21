# 修复计划：对话框关闭按钮与会话标签动画问题

## 问题分析

### 问题 1：Shader 模式对话框关闭按钮不起作用

**问题文件**: `qml/components/ShaderParamsPanel.qml`

**涉及的对话框**:
- 新建类别对话框 (`addCategoryDialog`)
- 保存风格对话框 (`savePresetDialog`)
- 删除类别对话框 (`deleteCategoryDialog`)
- 重命名类别对话框 (`renameCategoryDialog`)
- 重命名风格对话框 (`renamePresetDialog`)

**根本原因分析**:

在 `DialogTitleBar.qml` 中，关闭按钮使用 `IconButton` 组件，但该按钮被 `MouseArea` (用于拖拽窗口) 覆盖。虽然 `IconButton` 设置了 `z: 3`，但 `MouseArea` 的 `anchors.rightMargin: 44` 可能不足以完全避开按钮区域（按钮实际宽度为 32px，加上 padding 和边距可能超过 44px）。

**对比正常工作的实现**:

在 `MediaViewerWindow.qml` 中，关闭按钮直接放在 `RowLayout` 中，没有被 `MouseArea` 覆盖，因此可以正常工作。

**修复方案**:

修改 `DialogTitleBar.qml`，将 `MouseArea` 的 `anchors.rightMargin` 增大，确保完全避开关闭按钮区域。计算：
- `IconButton` 的 `btnSize` = 32
- `RowLayout` 的 `anchors.rightMargin` = 8
- 关闭按钮实际占用空间 ≈ 32 + 一些边距

当前 `rightMargin: 44` 可能不够，需要增加到至少 48-52。

---

### 问题 2：会话标签悬停时有黑色闪烁动画

**问题文件**: `qml/components/SessionList.qml`

**根本原因分析**:

在 `SessionList.qml` 中，会话标签项 (`delegate`) 有两个动画效果：

1. **蓝色边框动画** (第 190-205 行): `dropIndicator` 的 `SequentialAnimation`，这是拖拽目标指示器，只在 `isDragTarget` 为 true 时运行。

2. **背景颜色动画** (第 157-162 行): `sessionItemBg` 的 `Behavior on color`，当 `isHovered` 状态改变时触发颜色过渡动画。

用户描述的"黑色闪烁"很可能是背景颜色动画在深色主题下产生的视觉效果。当鼠标悬停时，背景从透明变为 `Theme.colors.sidebarAccent`，这个过渡可能产生不期望的闪烁效果。

**修复方案**:

移除 `sessionItemBg` 的 `Behavior on color` 动画，使背景颜色变化立即生效，避免闪烁。

---

## 实施步骤

### 步骤 1: 修复 DialogTitleBar.qml

修改 `qml/components/DialogTitleBar.qml`:
- 将 `MouseArea` 的 `anchors.rightMargin` 从 `44` 增加到 `52`，确保完全避开关闭按钮区域

### 步骤 2: 修复 SessionList.qml

修改 `qml/components/SessionList.qml`:
- 移除 `sessionItemBg` 的 `Behavior on color` 动画（第 157-162 行）

---

## 预期结果

1. **对话框关闭按钮**: 点击 `x` 按钮可以正常关闭对话框
2. **会话标签悬停**: 悬停时只有平滑的背景色变化，没有黑色闪烁效果
