# 会话标签排序与拖动动画优化方案

## 问题分析

### 问题1：会话标签排序混乱

**根本原因**：数据同步架构缺陷

```
SessionController                    SessionModel
┌─────────────────┐                ┌─────────────────┐
│ m_sessions      │                │ m_sessions      │
│ (独立副本)      │ ──updateSessions──> │ (独立副本)      │
└─────────────────┘                └─────────────────┘
```

**问题代码路径**：
1. `SessionController::switchSession()` 调用 `m_sessionModel->updateSessions(m_sessions)`
2. `SessionModel::updateSessions()` 使用 `beginResetModel()` / `endResetModel()` 重置整个模型
3. `SessionController` 的 `m_sessions` 列表没有维护正确的排序状态
4. 每次切换会话都会用未排序的数据覆盖 Model 的已排序列表

**具体问题点**：
- [SessionController.cpp:96](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/SessionController.cpp#L96)：`switchSession` 调用 `updateSessions` 导致模型重置
- [SessionModel.cpp:293-298](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/models/SessionModel.cpp#L293)：`updateSessions` 使用 `beginResetModel()` 破坏排序
- 数据双写：`SessionController` 和 `SessionModel` 各自维护独立的会话列表

### 问题2：拖动动画效果差

**当前实现缺陷**：
- 仅使用透明度变化 (`opacity: 0.5`) 表示拖动状态
- 只有一条简单的指示线
- 缺少平滑的位移动画
- 没有拖动项的视觉提升效果（缩放、阴影）
- ListView 没有配置 `displaced` 动画

---

## 解决方案

### 方案1：修复排序混乱 - 统一数据源架构

**设计原则**：
- `SessionModel` 作为唯一数据源
- `SessionController` 只负责业务逻辑，不维护数据副本
- 切换会话时只更新状态，不重置模型

**修改内容**：

#### 1.1 修改 SessionController::switchSession
```cpp
// 修改前：调用 updateSessions 重置整个模型
m_sessionModel->updateSessions(m_sessions);

// 修改后：只调用 Model 的 switchSession 方法
m_sessionModel->switchSession(sessionId);
```

#### 1.2 移除 SessionController 中的 m_sessions 副本
- 将 `QList<Session> m_sessions` 改为从 `SessionModel` 获取数据
- 所有操作都通过 `SessionModel` 进行

#### 1.3 修改 SessionModel::switchSession
- 只更新 `isActive` 状态
- 不触发重新排序
- 只发送 `dataChanged` 信号

### 方案2：改进拖动动画

**设计目标**：
- 现代化的拖动视觉反馈
- 平滑的列表项位移动画
- 清晰的拖动目标指示

**实现方案**：

#### 2.1 添加 ListView 动画配置
```qml
ListView {
    id: sessionListView
    
    // 添加 displaced 动画 - 列表项平滑位移
    displaced: Transition {
        NumberAnimation {
            properties: "y"
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    
    // 添加 move 动画
    move: Transition {
        NumberAnimation {
            properties: "y"
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
}
```

#### 2.2 优化拖动项视觉效果
```qml
// 拖动项容器
Item {
    id: delegateRoot
    
    // 添加缩放动画
    scale: delegateRoot.isBeingDragged ? 1.02 : 1.0
    z: delegateRoot.isBeingDragged ? 100 : 1
    
    Behavior on scale {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }
    
    // 拖动时的阴影效果
    Rectangle {
        id: dragShadow
        anchors.fill: parent
        anchors.margins: -2
        radius: sessionItemBg.radius + 2
        color: "transparent"
        visible: delegateRoot.isBeingDragged
        
        // 使用多层阴影模拟深度
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: delegateRoot.isBeingDragged ? 4 : 0
            radius: 8
            samples: 16
            color: Qt.rgba(0, 0, 0, 0.15)
        }
    }
}
```

#### 2.3 改进拖动目标指示器
```qml
// 拖动目标指示器 - 改为更明显的样式
Rectangle {
    id: dropIndicator
    visible: delegateRoot.isDragTarget
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    height: 3
    radius: 1.5
    color: Theme.colors.primary
    
    // 添加发光效果
    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: parent.height + 4
        radius: parent.radius + 2
        color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, 
                       Theme.colors.primary.b, 0.3)
    }
    
    // 入场动画
    SequentialAnimation on opacity {
        running: delegateRoot.isDragTarget
        loops: Animation.Infinite
        NumberAnimation { to: 0.6; duration: 500 }
        NumberAnimation { to: 1.0; duration: 500 }
    }
}
```

#### 2.4 添加拖动状态背景
```qml
Rectangle {
    id: sessionItemBg
    
    // 拖动时的背景变化
    color: {
        if (delegateRoot.isActive) return Theme.colors.primary
        if (delegateRoot.isBeingDragged) return Theme.colors.accent
        if (delegateRoot.isDragTarget) return Theme.colors.primarySubtle
        if (delegateRoot.isHovered) return Theme.colors.sidebarAccent
        return "transparent"
    }
    
    // 拖动时的边框
    border.width: delegateRoot.isBeingDragged ? 2 : 0
    border.color: Theme.colors.primary
}
```

---

## 实施步骤

### 阶段1：修复排序问题（核心修复）

| 步骤 | 文件 | 修改内容 |
|------|------|----------|
| 1.1 | SessionModel.h | 添加 `sessions()` getter 返回引用 |
| 1.2 | SessionController.h | 移除 `m_sessions` 成员变量 |
| 1.3 | SessionController.cpp | 修改 `switchSession` 调用 Model 方法 |
| 1.4 | SessionController.cpp | 修改所有方法从 Model 获取数据 |
| 1.5 | SessionModel.cpp | 确保 `switchSession` 不触发重排序 |

### 阶段2：优化拖动动画

| 步骤 | 文件 | 修改内容 |
|------|------|----------|
| 2.1 | SessionList.qml | 添加 ListView displaced/move 动画 |
| 2.2 | SessionList.qml | 添加拖动项缩放和阴影效果 |
| 2.3 | SessionList.qml | 改进拖动目标指示器样式 |
| 2.4 | SessionList.qml | 添加拖动状态背景变化 |

### 阶段3：测试验证

| 步骤 | 测试内容 |
|------|----------|
| 3.1 | 置顶会话后，切换其他会话，验证排序不变 |
| 3.2 | 不置顶任何会话，切换会话，验证排序不变 |
| 3.3 | 拖动排序，验证动画流畅 |
| 3.4 | 跨置顶区域拖动，验证正确拦截 |

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 数据迁移可能导致状态丢失 | 中 | 保持 Session 数据结构不变，只改访问方式 |
| 动画性能影响 | 低 | 使用硬件加速的属性动画 |
| 拖动逻辑变化 | 低 | 保持原有拖动逻辑，只改视觉表现 |

---

## 预期效果

### 排序修复后：
- 置顶会话始终在列表顶部
- 切换会话不会改变排序
- 拖动排序后位置保持稳定

### 动画优化后：
- 拖动时项目平滑位移
- 拖动项有缩放和阴影效果
- 目标位置有清晰的视觉指示
- 整体动画流畅自然
