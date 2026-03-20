# 嵌入窗口拖拽交互优化

## 概述

优化嵌入窗口拖拽脱离主界面的交互体验，解决拖拽过程中窗口卡顿、无法实时跟随鼠标的问题。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、问题描述

### 原始问题

在拖拽嵌入窗口使其脱离主界面的过程中存在明显的卡顿现象：

1. 窗口无法跟随鼠标实时移动
2. 拖拽过程中窗口突然停在界面上
3. 之后才能进行自由拖拽

### 根因分析

1. **拖拽阈值过大**：原阈值 60px，在未超过阈值前窗口完全静止
2. **切换后拖拽中断**：`switchToDetached` 只显示窗口，没有启动系统拖拽
3. **缺少视觉反馈**：拖拽过程中无预览窗口跟随

---

## 二、解决方案

### 核心改进

实现**拖拽预览 + 无缝切换**机制：

1. **拖拽预览**：拖拽开始时立即显示跟随鼠标的预览窗口
2. **无缝切换**：释放鼠标时，预览窗口转换为真正的独立窗口
3. **系统拖拽接力**：切换后自动启动系统拖拽，保持拖拽连续性

### 关键改进点

| 改进项 | 优化前 | 优化后 |
|--------|--------|--------|
| 拖拽阈值 | 60px | 10px（更快响应） |
| 视觉反馈 | 无（窗口静止） | 预览窗口跟随鼠标 |
| 切换连续性 | 中断，需重新点击 | 自动启动系统拖拽 |
| 吸附提示 | 仅独立窗口有 | 预览窗口也有 |

---

## 三、修改的文件

| 文件 | 修改内容 |
|------|----------|
| `qml/components/EmbeddedMediaViewer.qml` | 添加拖拽预览窗口、重构标题栏 MouseArea 拖拽逻辑 |

---

## 四、技术实现细节

### 1. 拖拽预览窗口

新增 `dragPreviewWindow` 组件，提供实时视觉反馈：

```qml
Window {
    id: dragPreviewWindow
    width: _savedW
    height: _savedH
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    
    property bool _snapHint: false
    
    Rectangle {
        anchors.fill: parent
        color: Theme.colors.background
        opacity: 0.85
        radius: 8
        border.width: 2
        border.color: dragPreviewWindow._snapHint ? Theme.colors.primaryLight : Theme.colors.primary
        
        // 标题栏和提示文本...
    }
}
```

### 2. 重构拖拽逻辑

修改标题栏 MouseArea，实现平滑拖拽：

```qml
MouseArea {
    id: titleDragArea
    property real startX: 0
    property real startY: 0
    property bool isDraggingOut: false
    
    onPressed: function(m) {
        startX = m.x
        startY = m.y
        cursorShape = Qt.ClosedHandCursor
    }
    
    onPositionChanged: function(m) {
        if (!pressed) return
        
        var distance = Math.sqrt(Math.pow(m.x - startX, 2) + Math.pow(m.y - startY, 2))
        
        if (distance > 10 && !isDraggingOut) {
            isDraggingOut = true
            // 显示预览窗口
            dragPreviewWindow.show()
        }
        
        if (isDraggingOut) {
            // 实时更新预览窗口位置
            var globalPos = mapToGlobal(m.x, m.y)
            dragPreviewWindow.x = globalPos.x - startX
            dragPreviewWindow.y = globalPos.y - startY
        }
    }
    
    onReleased: {
        if (isDraggingOut) {
            isDraggingOut = false
            dragPreviewWindow.hide()
            
            if (shouldSnap) {
                root.switchToEmbedded()
            } else {
                root.switchToDetached(finalX, finalY)
                // 关键：启动系统拖拽保持连续性
                Qt.callLater(function() {
                    winHelper.startSystemMove()
                })
            }
        }
    }
}
```

### 3. 系统拖拽接力

通过 `SubWindowHelper.startSystemMove()` 启动 Windows 系统拖拽：

```cpp
void SubWindowHelper::startSystemMove()
{
    if (m_window) {
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        ReleaseCapture();
        SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE + HTCAPTION, 0);
#else
        m_window->startSystemMove();
#endif
    }
}
```

---

## 五、交互流程

```
用户按下标题栏 → 显示拖拽手势
    ↓
用户开始拖动（> 10px）→ 立即显示预览窗口，跟随鼠标
    ↓
用户继续拖动 → 预览窗口实时跟随，显示吸附提示
    ↓
用户释放鼠标 → 预览窗口消失，显示真正的独立窗口
    ↓
自动启动系统拖拽 → 窗口继续跟随鼠标，无需重新点击
```

---

## 六、视觉设计

### 预览窗口样式

- **背景**：半透明（opacity: 0.85）
- **圆角**：8px
- **边框**：2px 宽度
  - 普通状态：主题主色（`Theme.colors.primary`）
  - 吸附状态：淡蓝色（`Theme.colors.primaryLight`）

### 吸附提示

- 当预览窗口位于容器区域上方时，边框变为淡蓝色
- 提示文本切换为"释放鼠标吸附到消息区域"

---

## 七、测试验证

- ✅ 基础拖拽测试：从嵌入模式拖拽脱离，预览窗口跟随流畅
- ✅ 吸附测试：拖拽到容器区域，吸附提示正常显示
- ✅ 连续拖拽测试：释放后系统拖拽自动启动
- ✅ 边界测试：快速拖拽、慢速拖拽、取消拖拽均正常

---

## 八、相关文档

- [embedded-media-viewer-fix-notes.md](./embedded-media-viewer-fix-notes.md) - 内嵌媒体查看器修复笔记
- [custom-titlebar-implementation-notes.md](./custom-titlebar-implementation-notes.md) - 自定义标题栏实现笔记
