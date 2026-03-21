# Tooltip 主题支持和箭头指示器实施计划

## 需求概述

为项目中所有鼠标悬停提示信息（Tooltip）实现：
1. **主题支持**：亮色/暗色主题自动切换
2. **箭头指示器**：提示卡片带有指向触发元素的箭头，清晰标识提示来源

## 当前状态分析

### 现有 Tooltip 使用位置

| 文件 | 使用方式 | 数量 |
|------|----------|------|
| `qml/controls/IconButton.qml` | ToolTip 附加属性 | 1处（模板） |
| `qml/components/Sidebar.qml` | 独立 ToolTip 组件 | 7处 |
| `qml/components/ShaderParamsPanel.qml` | 独立 ToolTip 组件 | 2处 |
| `qml/components/TitleBar.qml` | 通过 IconButton 使用 | 10+处 |
| `qml/components/ControlPanel.qml` | 通过 IconButton 使用 | 4处 |

### 当前问题

1. **无主题适配**：使用 Qt Quick Controls 默认 ToolTip 样式，不支持亮/暗主题切换
2. **无箭头指示器**：提示框是简单的矩形，无法直观显示与触发元素的关联
3. **样式不统一**：部分使用附加属性，部分使用独立组件

## 实施方案

### 1. 创建自定义 Tooltip 控件

**文件**: `qml/controls/Tooltip.qml`

**功能特性**:
- 支持亮/暗主题自动切换（通过 Theme.colors）
- 四向箭头指示器（上、下、左、右）
- 平滑显示/隐藏动画
- 可配置延迟时间
- 自动边界检测和位置调整

**组件结构**:
```
Popup {
    ├── background: Rectangle (主题适配背景 + 边框)
    │   ├── Canvas (箭头绘制)
    │   └── contentItem: Text
    └── 动画效果
}
```

**箭头实现方案**:
- 使用 Canvas 绘制三角形箭头
- 箭头位置根据 `arrowDirection` 属性动态调整
- 箭头颜色与背景色同步

### 2. 更新颜色系统

**文件**: `qml/styles/Colors.qml`

**新增颜色属性**:
```qml
// 亮色主题
readonly property color tooltip: "#FFFFFF"
readonly property color tooltipForeground: "#0A1628"
readonly property color tooltipBorder: "#D0DAE8"

// 暗色主题
readonly property color tooltip: "#1A2744"
readonly property color tooltipForeground: "#E8EDF5"
readonly property color tooltipBorder: "#2A3F5F"
```

### 3. 更新 Theme.qml

**文件**: `qml/styles/Theme.qml`

**新增 Tooltip 相关属性**:
```qml
readonly property var tooltip: QtObject {
    readonly property int delay: 500
    readonly property int maxWidth: 280
    readonly property int padding: 8
    readonly property int arrowSize: 6
    readonly property int offset: 4
}
```

### 4. 更新 IconButton 控件

**文件**: `qml/controls/IconButton.qml`

**修改内容**:
- 移除 Qt Quick Controls ToolTip 附加属性
- 集成自定义 Tooltip 组件
- 支持箭头方向配置

### 5. 更新其他使用 Tooltip 的组件

**文件列表**:
- `qml/components/Sidebar.qml` - 替换所有 ToolTip 为自定义组件
- `qml/components/ShaderParamsPanel.qml` - 替换所有 ToolTip
- 其他通过 IconButton 间接使用的组件自动生效

## 详细实施步骤

### Step 1: 扩展颜色系统
1. 在 `Colors.qml` 中添加 tooltip 相关颜色定义
2. 确保亮/暗主题都有完整的 tooltip 颜色

### Step 2: 扩展 Theme 单例
1. 在 `Theme.qml` 中添加 tooltip 配置对象
2. 定义统一的延迟、尺寸、间距参数

### Step 3: 创建 Tooltip 控件
1. 创建 `qml/controls/Tooltip.qml`
2. 实现主题适配背景
3. 实现 Canvas 箭头绘制
4. 实现四向箭头支持
5. 实现显示/隐藏动画
6. 实现边界检测和位置调整

### Step 4: 更新 IconButton
1. 修改 `IconButton.qml` 集成自定义 Tooltip
2. 添加 `tooltipDirection` 属性控制箭头方向
3. 测试各种场景下的显示效果

### Step 5: 更新其他组件
1. 更新 `Sidebar.qml` 中的 ToolTip 使用
2. 更新 `ShaderParamsPanel.qml` 中的 ToolTip 使用

### Step 6: 测试验证
1. 验证亮色主题下 Tooltip 显示
2. 验证暗色主题下 Tooltip 显示
3. 验证箭头指示器正确指向
4. 验证边界情况处理

## 技术细节

### 箭头绘制算法

```javascript
// Canvas 绘制三角形箭头
// arrowDirection: "up" | "down" | "left" | "right"
function drawArrow(ctx, direction, x, y, size, color) {
    ctx.fillStyle = color
    ctx.beginPath()
    
    switch(direction) {
        case "down": // 箭头向下（Tooltip 在上方）
            ctx.moveTo(x - size, y)
            ctx.lineTo(x, y + size)
            ctx.lineTo(x + size, y)
            break
        case "up": // 箭头向上（Tooltip 在下方）
            ctx.moveTo(x - size, y)
            ctx.lineTo(x, y - size)
            ctx.lineTo(x + size, y)
            break
        // ... left, right 类似
    }
    ctx.closePath()
    ctx.fill()
}
```

### 位置计算逻辑

```
Tooltip 位置 = 触发元素位置 + 偏移量 + 箭头尺寸

优先级：
1. 下方显示（箭头向上）- 默认
2. 上方显示（箭头向下）- 下方空间不足时
3. 左侧显示（箭头向右）- 右侧空间不足时
4. 右侧显示（箭头向左）- 左侧空间不足时
```

## 文件变更清单

| 操作 | 文件路径 | 说明 |
|------|----------|------|
| 新建 | `qml/controls/Tooltip.qml` | 自定义 Tooltip 控件 |
| 修改 | `qml/styles/Colors.qml` | 添加 tooltip 颜色 |
| 修改 | `qml/styles/Theme.qml` | 添加 tooltip 配置 |
| 修改 | `qml/controls/IconButton.qml` | 集成自定义 Tooltip |
| 修改 | `qml/components/Sidebar.qml` | 替换 ToolTip 组件 |
| 修改 | `qml/components/ShaderParamsPanel.qml` | 替换 ToolTip 组件 |

## 预期效果

### 亮色主题
- 背景：白色 (#FFFFFF)
- 文字：深色 (#0A1628)
- 边框：浅灰色 (#D0DAE8)
- 箭头：指向触发元素

### 暗色主题
- 背景：深蓝色 (#1A2744)
- 文字：浅色 (#E8EDF5)
- 边框：深灰色 (#2A3F5F)
- 箭头：指向触发元素

### 交互体验
- 悬停 500ms 后显示
- 平滑淡入动画 (150ms)
- 鼠标移出后淡出 (100ms)
- 自动边界检测避免溢出屏幕
