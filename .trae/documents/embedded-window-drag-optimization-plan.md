# 嵌入窗口拖拽交互优化方案

## 问题描述

当前在拖拽嵌入窗口使其脱离主界面的过程中存在明显的卡顿现象：
1. 窗口无法跟随鼠标实时移动
2. 拖拽过程中窗口突然停在界面上
3. 之后才能进行自由拖拽

## 问题根因分析

### 当前实现流程

```
用户按下标题栏 → MouseArea.onPressed 记录起始位置
    ↓
用户拖动鼠标 → MouseArea.onPositionChanged 检测移动距离
    ↓
移动距离 > 60px → 调用 switchToDetached(gp.x, gp.y)
    ↓
显示独立窗口 → detachedWindow.show()
    ↓
【问题点】窗口显示后，用户需要重新点击才能继续拖拽
```

### 核心问题

1. **拖拽过程中无视觉反馈**：在移动距离未超过 60px 阈值前，窗口完全静止
2. **切换后拖拽中断**：`switchToDetached` 只是显示窗口，没有启动系统拖拽
3. **缺少连续性**：从嵌入模式到独立窗口的过渡不流畅

## 优化方案

### 方案概述

实现**拖拽预览 + 无缝切换**机制：

1. **拖拽预览**：拖拽开始时立即显示一个跟随鼠标的预览窗口
2. **无缝切换**：释放鼠标时，将预览窗口转换为真正的独立窗口
3. **系统拖拽接力**：切换后自动启动系统拖拽，保持拖拽连续性

### 技术实现

#### 1. 添加拖拽预览窗口

在 `EmbeddedMediaViewer.qml` 中添加一个预览窗口组件：

```qml
// 拖拽预览窗口 - 跟随鼠标移动
Window {
    id: dragPreviewWindow
    width: _savedW
    height: _savedH
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    visible: false
    
    // 半透明预览效果
    Rectangle {
        anchors.fill: parent
        color: Theme.colors.background
        opacity: 0.9
        radius: 8
        border.width: 2
        border.color: Theme.colors.primary
        
        // 显示文件名
        Text {
            anchors.centerIn: parent
            text: root.currentFile ? root.currentFile.fileName : ""
            color: Theme.colors.foreground
        }
    }
}
```

#### 2. 修改拖拽逻辑

修改 `EmbeddedMediaViewer.qml` 中的 MouseArea：

```qml
MouseArea {
    id: titleDragArea
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.rightMargin: btnRow.width + 8
    
    property real startX: 0
    property real startY: 0
    property bool isDraggingOut: false
    
    cursorShape: Qt.OpenHandCursor
    
    onPressed: function(m) {
        startX = m.x
        startY = m.y
        cursorShape = Qt.ClosedHandCursor
    }
    
    onPositionChanged: function(m) {
        if (!pressed) return
        
        var dx = m.x - startX
        var dy = m.y - startY
        var distance = Math.sqrt(dx * dx + dy * dy)
        
        if (distance > 10 && !isDraggingOut) {
            // 开始拖拽脱离
            isDraggingOut = true
            
            // 计算预览窗口位置（鼠标在标题栏中的相对位置）
            var globalPos = mapToGlobal(m.x, m.y)
            dragPreviewWindow.x = globalPos.x - startX
            dragPreviewWindow.y = globalPos.y - startY
            dragPreviewWindow.width = _savedW
            dragPreviewWindow.height = _savedH
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
        cursorShape = Qt.OpenHandCursor
        
        if (isDraggingOut) {
            isDraggingOut = false
            dragPreviewWindow.hide()
            
            // 切换到独立窗口模式
            var finalX = dragPreviewWindow.x
            var finalY = dragPreviewWindow.y
            switchToDetached(finalX, finalY)
            
            // 启动系统拖拽（关键！）
            Qt.callLater(function() {
                winHelper.startSystemMove()
            })
        }
    }
}
```

#### 3. 增强 SubWindowHelper

在 `SubWindowHelper` 中添加 `startSystemMove()` 方法（已存在，需确保正确调用）：

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

#### 4. 优化 switchToDetached 函数

```qml
function switchToDetached(gx, gy) {
    if (displayMode === "detached") return
    
    displayMode = "detached"
    embeddedOverlay.visible = false
    
    // 使用传入的位置或默认居中
    detachedWindow.x = gx !== undefined ? gx : Screen.width / 2 - 400
    detachedWindow.y = gy !== undefined ? gy : Screen.height / 2 - 300
    detachedWindow.width = _savedW
    detachedWindow.height = _savedH
    
    detachedWindow.show()
    detachedWindow.raise()
    detachedWindow.requestActivate()
}
```

### 实现步骤

#### 步骤 1：修改 EmbeddedMediaViewer.qml

1. 添加拖拽预览窗口组件 `dragPreviewWindow`
2. 重构标题栏 MouseArea 的拖拽逻辑
3. 添加 `isDraggingOut` 状态标志
4. 实现预览窗口跟随鼠标移动
5. 释放时调用 `startSystemMove()` 保持拖拽连续性

#### 步骤 2：确保 SubWindowHelper.startSystemMove() 可用

1. 检查 QML 中是否正确注册了该方法
2. 确保在窗口显示后调用

#### 步骤 3：添加视觉优化

1. 预览窗口添加圆角和阴影效果
2. 添加拖拽提示文本（可选）
3. 添加吸附区域高亮（已有）

### 预期效果

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

### 关键改进点

| 改进项 | 优化前 | 优化后 |
|--------|--------|--------|
| 拖拽阈值 | 60px | 10px（更快响应） |
| 视觉反馈 | 无（窗口静止） | 预览窗口跟随鼠标 |
| 切换连续性 | 中断，需重新点击 | 自动启动系统拖拽 |
| 用户体验 | 卡顿、不流畅 | 平滑、即时响应 |

## 文件修改清单

| 文件 | 修改内容 |
|------|----------|
| `qml/components/EmbeddedMediaViewer.qml` | 添加预览窗口、重构拖拽逻辑 |
| `include/EnhanceVision/utils/SubWindowHelper.h` | 确保 Q_INVOKABLE 标记 |
| `src/utils/SubWindowHelper.cpp` | 确认 startSystemMove 实现 |

## 测试验证

1. **基础拖拽测试**：从嵌入模式拖拽脱离，验证预览窗口跟随
2. **吸附测试**：拖拽到容器区域，验证吸附提示和切换
3. **连续拖拽测试**：释放后验证系统拖拽是否自动启动
4. **边界测试**：快速拖拽、慢速拖拽、取消拖拽等场景

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| startSystemMove 在某些情况下不生效 | 窗口可能不跟随 | 添加备选方案：手动跟踪鼠标 |
| 预览窗口性能开销 | 轻微延迟 | 使用轻量级预览内容 |
| 多显示器支持 | 位置计算错误 | 使用 Screen 正确计算坐标 |

## 备选方案

如果 `startSystemMove()` 无法满足需求，可采用**手动跟踪鼠标**方案：

```qml
// 在独立窗口中手动跟踪鼠标
Window {
    id: detachedWindow
    
    property bool isManualDragging: false
    property real dragOffsetX: 0
    property real dragOffsetY: 0
    
    function startManualDrag(mouseX, mouseY) {
        isManualDragging = true
        dragOffsetX = mouseX
        dragOffsetY = mouseY
    }
    
    // 使用全局鼠标位置跟踪
    onActiveFocusItemChanged: {
        if (isManualDragging) {
            // 跟踪全局鼠标移动
        }
    }
}
```

此方案需要更复杂的实现，但可以提供更精细的控制。
