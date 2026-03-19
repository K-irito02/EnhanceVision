# 媒体查看器优化修复

## 概述

本次更新完成了媒体查看器窗口的多项重要优化，包括窗口层级管理、会话隔离、独立窗口显示、全屏交互和导航按钮自动隐藏等功能。

**创建日期**: 2026-03-19
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `qml/components/EmbeddedMediaViewer.qml` | 优化z-index管理、独立窗口居中、全屏拖拽退出、导航按钮自动隐藏 |
| `qml/components/MinimizedWindowDock.qml` | 添加会话级别清理功能、优化标签垂直对齐 |
| `qml/pages/MainPage.qml` | 添加全局z-index计数器、会话切换监听 |

### 2. 实现的功能特性

- ✅ **窗口层级管理优化**：使用全局递增计数器，确保最后打开的查看器始终显示在最上层
- ✅ **会话隔离机制**：切换会话时自动关闭消息查看器，避免状态混乱
- ✅ **独立窗口内容居中**：媒体内容在独立窗口中完美居中显示
- ✅ **全屏拖拽退出**：全屏状态下拖拽标题栏自动退出全屏
- ✅ **导航按钮自动隐藏**：鼠标不活动2秒后自动隐藏导航按钮
- ✅ **最小化标签居中**：最小化标签与"全部恢复"按钮完美垂直对齐

---

## 二、遇到的问题及解决方案

### 问题 1：窗口层级管理混乱

**现象**：
- 两个查看器都最小化时，依次打开后层级不正确
- 重复点击缩略图导致查看器来回切换

**原因**：
- 每个EmbeddedMediaViewer实例都有独立的z-index计数器
- 使用`Date.now() % 100`可能导致相同或递减的值

**解决方案**：
```javascript
// 在MainPage中添加全局计数器
property int globalZIndexCounter: 1100

// 在EmbeddedMediaViewer中使用父级计数器
if (parent && parent.parent && parent.parent.globalZIndexCounter !== undefined) {
    parent.parent.globalZIndexCounter++
    root.z = parent.parent.globalZIndexCounter
}
```

### 问题 2：独立窗口内容未居中

**现象**：
- 独立窗口中媒体内容紧贴边缘，视觉上不居中

**原因**：
- 移除了所有margins，导致内容无视觉呼吸空间

**解决方案**：
```javascript
MediaContentArea {
    anchors.margins: 16  // 添加16px边距
    viewer: root
    videoPlayer: vidPlayer
}
```

### 问题 3：全屏拖拽退出不工作

**现象**：
- 全屏状态下拖拽标题栏无法退出全屏

**原因**：
- MouseArea的事件处理逻辑不完善，阻止了其他控件的交互

**解决方案**：
```javascript
MouseArea {
    acceptedButtons: Qt.LeftButton
    property bool isDragging: false
    
    onPressed: function(mouse) {
        if (detachedWindow.visibility === Window.FullScreen) {
            dragStartPos = Qt.point(mouse.x, mouse.y)
        } else {
            mouse.accepted = false
        }
    }
    
    onPositionChanged: function(mouse) {
        if (Math.abs(dx) > 10 || Math.abs(dy) > 10) {
            isDragging = true
            detachedWindow.showNormal()
        }
    }
}
```

### 问题 4：导航按钮闪烁

**现象**：
- 鼠标悬停在导航按钮上时，按钮快速闪烁

**原因**：
- visible和opacity都绑定到showNavButtons，导致布局循环

**解决方案**：
```javascript
Rectangle { 
    visible: viewer.currentIndex > 0  // 只依赖索引
    opacity: showNavButtons ? 1.0 : 0.0  // 只控制透明度
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
```

---

## 三、技术实现细节

### 1. 全局z-index管理

使用MainPage级别的全局计数器，确保所有EmbeddedMediaViewer实例共享同一计数器，避免层级混乱。

### 2. 会话隔离机制

通过监听sessionController的activeSessionChanged信号，在会话切换时自动关闭消息查看器并清理最小化标签。

### 3. 导航按钮自动隐藏

使用Timer和MouseArea组合实现：
- 鼠标移动时重置2秒计时器
- 鼠标静止2秒后自动隐藏
- 淡入淡出动画200ms

### 4. 最小化标签居中

调整ListView的Layout属性和delegate高度，确保与"全部恢复"按钮完美对齐。

---

## 四、参考资料

- Qt Quick Layouts 文档
- Qt Window 全屏管理文档
- QML 状态管理和属性绑定最佳实践
