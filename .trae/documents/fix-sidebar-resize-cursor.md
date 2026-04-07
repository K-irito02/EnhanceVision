# 修复侧边栏边缘鼠标光标样式问题

## 问题分析

### 当前现象
将鼠标移动到会话侧边栏右侧边缘时，鼠标图标没有变成水平双向箭头（←→）。

### 根本原因
当前实现在 `App.qml` 中使用了一个独立的 `sidebarResizeHandle` MouseArea（第 166-214 行）来处理侧边栏的拖拽调整宽度功能。虽然设置了 `cursorShape: Qt.SizeHorCursor`，但可能存在以下问题：

1. **位置定义问题**：MouseArea 使用 `anchors.left: sidebar.right` 和 `anchors.leftMargin: -6`，可能导致实际位置与预期不符
2. **层级遮挡**：虽然 z 值为 10001，但可能被其他元素干扰
3. **不符合设计文档**：根据 `docs/Notes/sidebar-layout-optimization-notes.md`，拖拽功能应该集成到 Sidebar.qml 中，而不是使用外部独立组件

### 设计文档要求
根据 `docs/Notes/sidebar-layout-optimization-notes.md` 的描述：
- 应该移除独立的调整手柄组件
- 将拖拽功能集成到 Sidebar 右边缘
- 在 Sidebar.qml 中添加 `resizeHandle` Rectangle 和 MouseArea

## 解决方案

### 方案选择
按照设计文档的要求，将拖拽功能集成到 Sidebar.qml 中，移除 App.qml 中的独立 `sidebarResizeHandle` 组件。

### 优势
1. **符合设计文档**：实现文档中描述的优化方案
2. **组件化设计**：拖拽功能内聚在 Sidebar 组件中
3. **避免遮挡问题**：MouseArea 直接在 Sidebar 内部，不会被外部元素遮挡
4. **更好的可维护性**：功能内聚，代码更清晰

## 实施步骤

### 步骤 1：修改 Sidebar.qml
在 Sidebar.qml 中添加拖拽功能：

1. 在第 98-104 行的右侧边框 Rectangle 之前，添加一个新的 `resizeHandle` Rectangle
2. 在 `resizeHandle` 中添加 MouseArea，设置：
   - `hoverEnabled: true`
   - `cursorShape: Qt.SizeHorCursor`（或 `Qt.SplitHCursor`）
   - `preventStealing: true`
3. 实现 MouseArea 的拖拽逻辑：
   - `onPressed`：记录初始位置，发出 `resizeStarted` 信号
   - `onReleased`：发出 `resizeFinished` 信号
   - `onPositionChanged`：计算 delta，发出 `resizeDelta` 信号
   - `onCanceled`：处理取消情况

### 步骤 2：修改 App.qml
移除独立的 `sidebarResizeHandle` 组件：

1. 删除第 163-214 行的 `sidebarResizeHandle` MouseArea
2. 保留 Sidebar 的信号处理（第 137-151 行已经存在）

### 步骤 3：验证和测试
1. 构建并运行项目
2. 测试鼠标光标样式：
   - 将鼠标移动到侧边栏右侧边缘
   - 验证鼠标图标变成水平双向箭头
3. 测试拖拽功能：
   - 拖拽调整侧边栏宽度
   - 验证宽度限制（最小 160px，最大 320px）
   - 验证拖拽流畅性

## 关键代码实现

### Sidebar.qml 中的拖拽功能

```qml
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
        
        onCanceled: {
            root.resizeFinished()
        }
    }
}
```

### App.qml 中的信号处理（已存在，无需修改）

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
        var clampedWidth = Math.max(root.sidebarMinWidth, 
                                   Math.min(root.sidebarMaxWidth, Math.round(newWidth)))
        root.sidebarWidth = clampedWidth
        UIStateController.sidebarWidth = clampedWidth
    }
}
```

## 注意事项

1. **光标样式选择**：
   - `Qt.SizeHorCursor`：水平双向箭头（←→）
   - `Qt.SplitHCursor`：水平分割箭头（与 `Qt.SizeHorCursor` 相同）
   - 建议使用 `Qt.SplitHCursor`，更符合分割调整的语义

2. **MouseArea 范围**：
   - `resizeHandle` 宽度为 6px
   - MouseArea 使用 `anchors.leftMargin: -4` 和 `anchors.rightMargin: -2`
   - 实际响应区域为 12px（从 sidebar 右边缘向左 4px 到向右 2px）

3. **防止事件窃取**：
   - 设置 `preventStealing: true`，确保拖拽事件不会被父组件窃取

4. **动画控制**：
   - 拖拽开始时禁用宽度动画（`sidebarAnimation.enabled = false`）
   - 拖拽结束后恢复动画（`sidebarAnimation.enabled = true`）

## 预期结果

1. ✅ 鼠标移动到侧边栏右侧边缘时，光标变成水平双向箭头
2. ✅ 拖拽调整侧边栏宽度功能正常
3. ✅ 宽度限制正常（最小 160px，最大 320px）
4. ✅ 拖拽流畅，无跳跃感
5. ✅ 符合设计文档要求
