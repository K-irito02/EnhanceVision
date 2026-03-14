# 会话区域布局优化与弹窗修复计划

## 问题分析

### 问题1：会话区域按钮显示不完整
**原因**：Sidebar 宽度固定为 200px，顶部操作栏包含多个带文字的按钮，导致内容超出可用宽度被截断。

**涉及文件**：
- `qml/components/Sidebar.qml` - 侧边栏组件

**具体问题点**：
1. 批量操作按钮（图标+文字"批量操作"/"取消批量"）
2. 会话计数标签（文字"X 个会话"）
3. 新建会话按钮（图标+文字"新建会话"）
4. 批量操作工具栏中的全选按钮、选中计数标签

### 问题2：弹窗被截断
**原因**：Sidebar.qml 内部定义了一个 Dialog 组件，其父容器是 Sidebar（宽度200px），而 Dialog 的宽度计算 `Math.min(440, parent.width - 48)` 导致对话框宽度被压缩到 152px。

**涉及文件**：
- `qml/components/Sidebar.qml` - 内部 Dialog 定义
- `qml/components/Dialog.qml` - 对话框组件
- `qml/utils/NotificationManager.qml` - 通知管理器

---

## 修复方案

### 任务1：优化会话区域按钮布局

#### 1.1 批量操作按钮
- **当前**：图标 + 文字（"批量操作"/"取消批量"）
- **优化**：仅显示图标，使用 ToolTip 提示功能
- **图标**：使用 `list-select` 图标

#### 1.2 会话计数标签
- **当前**：显示"X 个会话"
- **优化**：改为小型徽章样式，显示数字即可，如"X"

#### 1.3 新建会话按钮
- **当前**：图标 + 文字"新建会话"
- **优化**：仅显示图标，使用 ToolTip 提示功能
- **图标**：使用 `plus` 图标

#### 1.4 批量操作工具栏
- **全选按钮**：改为仅图标，使用 ToolTip
- **选中计数**：保持显示，但更紧凑
- **清空/删除按钮**：已经是仅图标，保持不变

#### 1.5 布局调整
- 减少按钮间距
- 使用更紧凑的尺寸
- 确保所有元素在 200px 宽度内完整显示

---

### 任务2：修复弹窗截断问题

#### 2.1 移除 Sidebar 内部的 Dialog
删除 `Sidebar.qml` 中的内部 Dialog 组件定义（第401-436行）

#### 2.2 使用全局 NotificationManager
修改 Sidebar.qml 中的对话框调用逻辑：
- 使用 `NotificationManager.showConfirmDialog()` 替代本地 Dialog
- 通过回调函数处理确认/取消操作

#### 2.3 确保 Dialog 模态行为
Dialog.qml 已经实现了：
- 遮罩层阻止点击穿透
- 拖拽移动功能
- 边界限制

需要确保：
- Dialog 在 App.qml 中正确定义（已完成）
- z-index 足够高（已设置 z: 2000）

---

## 详细实现步骤

### 步骤1：修改 Sidebar.qml 顶部操作栏

```qml
// 顶部操作栏 - 优化后
RowLayout {
    Layout.fillWidth: true
    spacing: 6
    
    // 批量操作按钮 - 仅图标
    Rectangle {
        id: batchBtn
        width: 32
        height: 32
        radius: Theme.radius.md
        color: root.batchMode ? Theme.colors.primarySubtle : (batchMouse.containsMouse ? Theme.colors.sidebarAccent : "transparent")
        border.width: 1
        border.color: root.batchMode ? Theme.colors.primary : Theme.colors.border
        
        ColoredIcon {
            anchors.centerIn: parent
            source: Theme.icon("list-select")
            iconSize: 16
            color: root.batchMode ? Theme.colors.primary : Theme.colors.mutedForeground
        }
        
        MouseArea { ... }
        
        ToolTip {
            visible: batchMouse.containsMouse
            text: root.batchMode ? qsTr("取消批量") : qsTr("批量操作")
            delay: 500
        }
    }
    
    // 会话计数徽章 - 仅数字
    Rectangle {
        visible: !root.batchMode
        width: countText.implicitWidth + 12
        height: 20
        radius: Theme.radius.full
        color: Theme.colors.sidebarAccent
        
        Text {
            id: countText
            anchors.centerIn: parent
            text: root.totalCount.toString()
            font.pixelSize: 11
            font.weight: Font.Medium
            color: Theme.colors.mutedForeground
        }
    }
    
    Item { Layout.fillWidth: true }
    
    // 新建会话按钮 - 仅图标
    Rectangle {
        id: newBtn
        width: 32
        height: 32
        radius: Theme.radius.md
        color: newMouse.containsMouse ? Theme.colors.primarySubtle : Theme.colors.primary
        
        ColoredIcon {
            anchors.centerIn: parent
            source: Theme.icon("plus")
            iconSize: 16
            color: "#FFFFFF"
        }
        
        MouseArea { ... }
        
        ToolTip {
            visible: newMouse.containsMouse
            text: qsTr("新建会话")
            delay: 500
        }
    }
}
```

### 步骤2：修改批量操作工具栏

```qml
// 批量操作工具栏 - 优化后
RowLayout {
    visible: root.batchMode
    Layout.fillWidth: true
    spacing: 6
    
    // 全选按钮 - 仅图标
    Rectangle {
        width: 28
        height: 28
        radius: Theme.radius.sm
        color: selectAllMouse.containsMouse ? Theme.colors.sidebarAccent : "transparent"
        border.width: 1
        border.color: Theme.colors.border
        
        ColoredIcon {
            anchors.centerIn: parent
            source: Theme.icon(root.selectedCount === root.totalCount && root.totalCount > 0 ? "check-square" : "square")
            iconSize: 14
            color: Theme.colors.icon
        }
        
        MouseArea { ... }
        
        ToolTip {
            visible: selectAllMouse.containsMouse
            text: root.selectedCount === root.totalCount && root.totalCount > 0 ? qsTr("取消全选") : qsTr("全选")
            delay: 500
        }
    }
    
    // 选中计数 - 保持紧凑
    Rectangle {
        implicitWidth: selectedCountText.implicitWidth + 10
        height: 28
        radius: Theme.radius.sm
        color: Theme.colors.primarySubtle
        
        Text {
            id: selectedCountText
            anchors.centerIn: parent
            text: qsTr("%1/%2").arg(root.selectedCount).arg(root.totalCount)
            font.pixelSize: 11
            font.weight: Font.Medium
            color: Theme.colors.primary
        }
    }
    
    Item { Layout.fillWidth: true }
    
    // 清空和删除按钮保持不变
    ...
}
```

### 步骤3：修改对话框调用逻辑

```qml
// 删除单个会话
onDeleteRequested: function(sessionId) {
    root.pendingDeleteSessionId = sessionId
    var sessionName = sessionController ? sessionController.getSessionName(sessionId) : ""
    NotificationManager.showConfirmDialog(
        qsTr("删除会话"),
        qsTr("确定要删除会话「%1」吗？此操作不可恢复。").arg(sessionName),
        function() {
            // 确认回调
            if (sessionController) sessionController.deleteSession(root.pendingDeleteSessionId)
            root.pendingDeleteSessionId = ""
        },
        function() {
            // 取消回调
            root.pendingDeleteSessionId = ""
        }
    )
}
```

### 步骤4：移除内部 Dialog 组件

删除 Sidebar.qml 中第401-436行的 Dialog 定义。

---

## 验证步骤

1. 构建项目，确保无编译错误
2. 运行应用程序
3. 验证会话区域按钮显示完整
4. 验证按钮 ToolTip 正常显示
5. 触发删除会话操作，验证弹窗完整显示
6. 验证弹窗可以拖拽移动
7. 验证弹窗模态行为（必须关闭才能操作其他区域）

---

## 文件修改清单

| 文件 | 修改内容 |
|------|----------|
| `qml/components/Sidebar.qml` | 1. 优化顶部操作栏按钮布局（仅图标）<br>2. 优化批量操作工具栏布局<br>3. 移除内部 Dialog 组件<br>4. 改用 NotificationManager.showConfirmDialog() |

---

## 风险评估

- **低风险**：修改仅涉及 UI 布局调整，不影响业务逻辑
- **兼容性**：NotificationManager 已在 App.qml 中正确初始化
- **回滚**：如有问题可快速恢复原始布局
