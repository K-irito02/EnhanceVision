# 侧边栏布局优化笔记

## 概述

解决主程序界面左侧会话标签区域的右边没有和中间底部按钮功能区域贴合的问题，并优化侧边栏宽度拖拽调整功能。

**创建日期**: 2026-04-02
**作者**: AI Assistant

---

## 一、变更概述

### 1. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/Sidebar.qml` | 移除独立的调整手柄组件，将拖拽功能集成到 Sidebar 右边缘 |
| `qml/App.qml` | 移除独立的 sidebarResizeHandle 组件，更新 Sidebar 信号处理 |

---

## 二、实现的功能特性

- ✅ 左侧会话标签区域与中间底部按钮功能区域贴合
- ✅ 侧边栏宽度拖拽调整功能流畅跟随鼠标
- ✅ 拖拽时禁用动画，释放后恢复动画

---

## 三、技术实现细节

### 问题分析

原布局结构：
```
┌─────────────┬─────────────────────────────┬──────────────┐
│   Sidebar   │ sidebarResizeHandle (6px)  │   MainPage   │
└─────────────┴─────────────────────────────┴──────────────┘
```

Sidebar 和 MainPage 之间存在 6px 宽的调整手柄区域，导致视觉上不够贴合。

### 解决方案

移除独立的调整手柄组件，将拖拽功能集成到 Sidebar 右边缘：

```
┌──────────────────────────────────────────┬──────────────┐
│   Sidebar (含右边缘拖拽区域)              │   MainPage   │
└──────────────────────────────────────────┴──────────────┘
```

### 关键代码实现

#### Sidebar.qml 中的拖拽功能

```qml
signal resizeStarted()
signal resizeFinished()
signal resizeDelta(int delta)

Rectangle {
    id: resizeHandle
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.right: parent.right
    width: 6
    color: "transparent"
    
    MouseArea {
        id: resizeMouse
        anchors.fill: parent
        anchors.leftMargin: -4
        anchors.rightMargin: -2
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor
        preventStealing: true
        
        property real lastGlobalX: 0
        
        onPressed: function(mouse) {
            var globalPos = mapToGlobal(mouse.x, mouse.y)
            lastGlobalX = globalPos.x
            root.resizeStarted()
        }
        
        onReleased: {
            root.resizeFinished()
        }
        
        onPositionChanged: function(mouse) {
            if (pressed) {
                var globalPos = mapToGlobal(mouse.x, mouse.y)
                var delta = globalPos.x - lastGlobalX
                lastGlobalX = globalPos.x
                root.resizeDelta(delta)
            }
        }
    }
}
```

#### App.qml 中的信号处理

```qml
Sidebar {
    id: sidebar
    minWidth: root.sidebarMinWidth
    maxWidth: root.sidebarMaxWidth
    
    onResizeStarted: {
        sidebarAnimation.enabled = false
    }
    
    onResizeFinished: {
        sidebarAnimation.enabled = true
    }
    
    onResizeDelta: function(delta) {
        var newWidth = root.sidebarWidth + delta
        root.sidebarWidth = Math.max(root.sidebarMinWidth, 
                                     Math.min(root.sidebarMaxWidth, Math.round(newWidth)))
    }
}
```

### 设计决策

1. **增量式 delta 计算**：每次鼠标移动时计算与上一次位置的差值，而不是与初始位置的差值，确保拖拽流畅。

2. **动画禁用/启用**：拖拽开始时禁用宽度动画，拖拽结束后恢复动画，避免跳跃感。

3. **全局坐标计算**：使用 `mapToGlobal` 获取全局坐标，避免因组件内部坐标变化导致的抖动。

---

## 四、遇到的问题及解决方案

### 问题 1：拖拽时出现跳跃

**现象**：拖拽侧边栏时，宽度不会跟随鼠标平滑移动，而是跳跃式变化。

**原因**：
1. 原实现使用初始位置计算总 delta，导致精度问题
2. 拖拽过程中动画仍在运行，导致视觉跳跃

**解决方案**：
1. 改用增量式 delta 计算，每次只计算与上一帧的差值
2. 拖拽开始时禁用动画，拖拽结束后恢复

---

## 五、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 侧边栏右边框与 MainPage 对齐 | 视觉上贴合 | ✅ 通过 |
| 拖拽调整侧边栏宽度 | 流畅跟随鼠标 | ✅ 通过 |
| 拖拽时动画效果 | 无跳跃感 | ✅ 通过 |
| 悬停显示拖拽指示器 | 显示蓝色指示线 | ✅ 通过 |

### 边界条件

- 最小宽度限制（160px）：✅ 正常
- 最大宽度限制（320px）：✅ 正常

---

## 六、性能影响

| 指标 | 变更前 | 变更后 | 影响 |
|------|--------|--------|------|
| 布局复杂度 | 多一个组件 | 减少 | 优化 |
| 拖拽流畅度 | 有跳跃感 | 流畅 | 优化 |

---

## 七、后续工作

- 无
