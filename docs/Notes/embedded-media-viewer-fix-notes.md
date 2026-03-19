# EmbeddedMediaViewer 组件修复笔记

## 概述

修复 EmbeddedMediaViewer 组件的多个问题，包括 SVG 图标显示、窗口拖拽吸附、独立窗口功能等。

**创建日期**: 2026-03-19
**作者**: AI Assistant

---

## 一、完成的修复

### 1. 修复的问题

| 问题 | 解决方案 |
|------|---------|
| SVG 图标不显示 | 修复 Theme.icon() 路径，添加 icons-dark 资源 |
| 独立窗口自动吸附 | 为 SubWindowHelper 添加拖拽状态检测 |
| 独立窗口按钮不可点击 | 添加 excludeRegions 排除按钮区域 |
| 缩略图无圆角 | 使用 MultiEffect mask 实现圆角 |
| 消息区点击不打开查看器 | 修复 MessageList 属性传递 |

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `qml/styles/Theme.qml` | 修复 icon() 函数路径逻辑 |
| `resources/qml.qrc` | 添加 icons-dark 资源前缀 |
| `resources/icons-dark/external-link.svg` | 创建缺失的淡蓝色图标 |
| `include/EnhanceVision/utils/SubWindowHelper.h` | 添加 isDragging 属性和信号 |
| `src/utils/SubWindowHelper.cpp` | 实现拖拽状态检测 |
| `qml/components/EmbeddedMediaViewer.qml` | 添加 excludeRegions，修复缩略图圆角 |
| `qml/pages/MainPage.qml` | 修复 MessageList 属性传递 |

### 3. 实现的功能特性

- ✅ SVG 图标在暗色主题下显示为淡蓝色
- ✅ 独立窗口按钮可以正常点击
- ✅ 拖拽独立窗口到容器区域自动吸附
- ✅ 缩略图具有圆角设计
- ✅ 消息展示区点击图片打开查看器
- ✅ 内嵌模式拖拽标题栏可独立窗口

---

## 二、遇到的问题及解决方案

### 问题 1：SVG 图标显示为黑色

**现象**：所有图标在暗色主题下显示为黑色，看不清楚。

**原因**：Theme.icon() 函数路径错误，没有使用 icons-dark 资源。

**解决方案**：
1. 在 qml.qrc 中添加 icons-dark 资源前缀
2. 修复 Theme.icon() 函数根据主题选择正确路径
3. 创建缺失的 external-link.svg 图标

### 问题 2：独立窗口按钮不可点击

**现象**：独立窗口标题栏按钮无法点击，被拖拽区域覆盖。

**原因**：SubWindowHelper 将整个标题栏当作拖拽区域。

**解决方案**：
1. 添加 updateExcludeRegions() 函数
2. 在窗口显示和尺寸变化时更新按钮排除区域
3. 使用 addExcludeRegion() 排除按钮区域

### 问题 3：窗口拖拽吸附不工作

**现象**：拖拽独立窗口到容器区域后不会吸附回去。

**原因**：SubWindowHelper 缺少拖拽状态检测。

**解决方案**：
1. 为 SubWindowHelper 添加 isDragging 属性
2. 处理 WM_ENTERSIZEMOVE 和 WM_EXITSIZEMOVE 消息
3. 监听 draggingChanged 信号实现吸附

### 问题 4：缩略图无圆角效果

**现象**：缩略图显示为直角，不符合设计要求。

**原因**：Image 组件没有圆角裁剪。

**解决方案**：
使用 MultiEffect 的 maskEnabled 功能实现圆角裁剪。

### 问题 5：消息区点击不打开查看器

**现象**：点击消息中的图片没有反应。

**原因**：MessageList 的 messageViewer 属性没有正确传递。

**解决方案**：
修复 MainPage.qml 中 MessageList 的属性赋值，移除多余的 property 关键字。

---

## 三、技术实现细节

### 1. Theme.icon() 路径修复

```qml
function icon(name) {
    // 暗色主题使用淡蓝色图标，亮色主题使用黑色图标
    var iconPath = isDark ? "icons-dark" : "icons"
    return "qrc:/" + iconPath + "/" + iconPath + "/" + name + ".svg"
}
```

### 2. SubWindowHelper 拖拽检测

```cpp
case WM_ENTERSIZEMOVE: {
    helper->setDragging(true);
    break;
}
case WM_EXITSIZEMOVE: {
    helper->setDragging(false);
    break;
}
```

### 3. 排除区域设置

```qml
function updateExcludeRegions() {
    winHelper.clearExcludeRegions()
    if (detBtnRow && detBtnRow.width > 0) {
        var globalPos = detBtnRow.mapToItem(detTitle, 0, 0)
        winHelper.addExcludeRegion(globalPos.x, globalPos.y, detBtnRow.width, detBtnRow.height)
    }
}
```

### 4. MultiEffect 圆角实现

```qml
layer.enabled: true
layer.effect: MultiEffect {
    maskEnabled: true
    maskThresholdMin: 0.5
    maskSpreadAtMin: 1.0
    maskSource: thumbMask
}
```

---

## 四、参考资料

- [Qt Quick Effects MultiEffect](https://doc.qt.io/qt-6/qtquick-effects-multieffect.html)
- [Windows WM_ENTERSIZEMOVE Message](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-entersizemove)
- [QML Resource System](https://doc.qt.io/qt-6/qtquick-resources.html)
