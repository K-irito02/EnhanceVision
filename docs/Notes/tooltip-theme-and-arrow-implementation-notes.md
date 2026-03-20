# Tooltip 主题支持和箭头指示器实现笔记

## 概述

为项目中所有鼠标悬停提示信息（Tooltip）实现了主题支持和箭头指示器功能。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

| 组件 | 文件路径 | 功能描述 |
|------|---------|---------|
| Tooltip | `qml/controls/Tooltip.qml` | 自定义 Tooltip 控件，支持亮/暗主题和箭头指示器 |

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `qml/styles/Colors.qml` | 添加 tooltip 颜色定义（亮色/暗色主题） |
| `qml/styles/Theme.qml` | 添加 tooltip 配置对象 |
| `qml/controls/IconButton.qml` | 集成自定义 Tooltip 组件 |
| `qml/components/Sidebar.qml` | 替换 5 处 ToolTip 为自定义组件 |
| `qml/components/ShaderParamsPanel.qml` | 替换 2 处 ToolTip 为自定义组件 |
| `resources/qml.qrc` | 注册 Tooltip.qml 资源 |
| `CMakeLists.txt` | 添加 Tooltip.qml 到 QML_FILES |

### 3. 实现的功能特性

- ✅ 亮色/暗色主题自动切换
- ✅ 四向箭头指示器（上、下）
- ✅ 平滑显示/隐藏动画
- ✅ 自动边界检测和位置调整
- ✅ 文字居中对齐
- ✅ 精确的尺寸自适应

---

## 二、遇到的问题及解决方案

### 问题 1：Tooltip 位置固定，不跟随目标元素

**现象**：Tooltip 总是出现在固定位置，而不是出现在鼠标悬停元素附近。

**原因**：使用了错误的坐标映射方式，Popup 的坐标需要相对于 Overlay。

**解决方案**：使用 Qt 官方推荐的方式：
```qml
parent: Overlay.overlay

function show(targetItem) {
    var globalPos = targetItem.mapToGlobal(targetItem.width / 2, targetItem.height)
    var localPos = Overlay.overlay.mapFromGlobal(globalPos.x, globalPos.y)
    // ...
}
```

### 问题 2：箭头方向反了

**现象**：箭头指向错误的方向。

**原因**：箭头方向命名与实际绘制逻辑不一致。

**解决方案**：统一命名规范：
- `"up"`：Tooltip 在元素下方，箭头在 Tooltip 顶部向上指
- `"down"`：Tooltip 在元素上方，箭头在 Tooltip 底部向下指

### 问题 3：Tooltip 尺寸过大

**现象**：Tooltip 标签尺寸比文字大很多。

**原因**：Text 组件的 padding 属性会增加 implicitWidth，导致重复计算。

**解决方案**：使用精确的尺寸计算：
```qml
implicitWidth: textLabel.implicitWidth + tooltipPadding * 2
implicitHeight: textLabel.implicitHeight + tooltipPadding * 2
```

### 问题 4：文字没有居中对齐

**现象**：提示文字在标签框内没有居中。

**原因**：使用了 y 属性定位，没有正确居中。

**解决方案**：使用 anchors.centerIn 和 verticalCenterOffset：
```qml
anchors.centerIn: parent
anchors.verticalCenterOffset: arrowCanvas.arrowDirection === "up" ? root.arrowSize / 2 + 2 : -root.arrowSize / 2 + 2
```

---

## 三、技术实现细节

### Canvas 箭头绘制

```javascript
if (arrowDirection === "up") {
    // 箭头在顶部，向上指
    ctx.moveTo(arrowX - size, size)
    ctx.lineTo(arrowX, 0)
    ctx.lineTo(arrowX + size, size)
} else {
    // 箭头在底部，向下指
    ctx.moveTo(arrowX - size, root.implicitHeight)
    ctx.lineTo(arrowX, root.implicitHeight + size)
    ctx.lineTo(arrowX + size, root.implicitHeight)
}
```

### 位置计算逻辑

```javascript
// 优先在元素下方显示
if (spaceBelow >= tooltipH + tooltipOffset) {
    preferredDirection = "up"
    posX = localPos.x - tooltipW / 2
    posY = localPos.y + tooltipOffset
} else {
    // 空间不足时在元素上方显示
    preferredDirection = "down"
    posY = localPosTop.y - tooltipH - tooltipOffset
}
```

### 颜色配置

**亮色主题**：
- 背景：白色 (#FFFFFF)
- 文字：深色 (#0A1628)
- 边框：浅灰色 (#D0DAE8)

**暗色主题**：
- 背景：淡蓝色 (#1E3A5F)
- 文字：浅色 (#E8EDF5)
- 边框：蓝色 (#2A5080)

---

## 四、参考资料

- [Qt Documentation - ToolTip](https://doc.qt.io/qt-6/qml-qtquick-controls-tooltip.html)
- [Qt Documentation - Popup Positioning](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html)
- [Qt Documentation - Overlay](https://doc.qt.io/qt-6/qml-qtquick-controls-overlay.html)
