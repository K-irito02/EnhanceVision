# 会话标签处理状态指示器设计计划

## 需求分析

用户希望将会话标签图标上的"点"改为表示"正在处理中"的状态：
- 当会话中有消息正在处理时，显示"点"
- 当会话中没有消息或消息全部处理完成，不显示"点"

## 当前设计分析

当前"点"的实现（`SessionList.qml` 第307-319行）：
- 只在 `model.isPinned`（置顶）时显示
- 位于图标右上角，蓝色圆点
- 用于表示置顶状态

## 设计方案

### 方案选择

**推荐方案**：保留置顶指示器，添加独立的处理状态指示器

理由：
1. 置顶和处理状态是两个独立的概念，应该分别显示
2. 用户可能同时需要知道会话是否置顶和是否有正在处理的任务
3. 视觉上更清晰，不会混淆

### 视觉设计

| 状态 | 指示器位置 | 样式 |
|------|-----------|------|
| 置顶 | 图标右上角 | 蓝色实心圆点（现有） |
| 处理中 | 图标右下角 | 橙色动画圆点（脉冲效果） |

### 处理状态指示器设计

```
┌─────────────────┐
│  ┌───────┐      │
│  │  📌  │ ●(蓝) │  ← 置顶指示器（右上角）
│  └───────┘      │
│            ●(橙)│  ← 处理中指示器（右下角，带脉冲动画）
└─────────────────┘
```

**处理中指示器样式**：
- 颜色：`#F59E0B`（橙色，表示进行中）
- 大小：8px
- 动画：脉冲效果（透明度 0.5 ~ 1.0 循环）
- 位置：图标右下角

## 实施步骤

### 步骤 1: 修改 DataTypes.h

在 `Session` 结构体中添加 `isProcessing` 字段：

```cpp
struct Session {
    // ... 现有字段 ...
    bool isProcessing;  ///< 是否有正在处理的消息
    
    Session()
        : isActive(false)
        , isSelected(false)
        , isPinned(false)
        , sortIndex(0)
        , isProcessing(false)  // 新增
    {}
};
```

### 步骤 2: 修改 SessionModel.h

添加 `IsProcessingRole` 角色：

```cpp
enum Roles {
    // ... 现有角色 ...
    IsProcessingRole  // 新增
};

// 添加 Q_INVOKABLE 方法
Q_INVOKABLE void setSessionProcessing(const QString &sessionId, bool processing);
```

### 步骤 3: 修改 SessionModel.cpp

1. 在 `data()` 方法中添加 `IsProcessingRole` 处理
2. 在 `roleNames()` 中添加角色名称映射
3. 实现 `setSessionProcessing()` 方法

### 步骤 4: 修改 SessionList.qml

添加处理状态指示器：

```qml
// 处理中指示器（右下角）
Rectangle {
    id: processingIndicator
    visible: model.isProcessing && !root.batchMode
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.rightMargin: -2
    anchors.bottomMargin: -2
    width: 8
    height: 8
    radius: 4
    color: "#F59E0B"  // 橙色
    
    // 脉冲动画
    SequentialAnimation on opacity {
        running: processingIndicator.visible
        loops: Animation.Infinite
        NumberAnimation { from: 0.5; to: 1.0; duration: 800 }
        NumberAnimation { from: 1.0; to: 0.5; duration: 800 }
    }
}
```

### 步骤 5: 集成处理状态更新

在任务处理相关代码中调用 `setSessionProcessing()`：
- 任务开始时：`sessionModel->setSessionProcessing(sessionId, true)`
- 任务完成/失败/取消时：检查会话是否还有其他处理中的任务，更新状态

## 文件修改清单

| 文件 | 修改内容 |
|------|---------|
| `include/EnhanceVision/models/DataTypes.h` | 添加 `isProcessing` 字段 |
| `include/EnhanceVision/models/SessionModel.h` | 添加 `IsProcessingRole` 和 `setSessionProcessing()` |
| `src/models/SessionModel.cpp` | 实现角色和设置方法 |
| `qml/components/SessionList.qml` | 添加处理状态指示器 |

## 预期效果

1. **置顶会话**：图标右上角显示蓝色小圆点
2. **处理中会话**：图标右下角显示橙色脉冲圆点
3. **同时置顶和处理中**：两个指示器同时显示
4. **无特殊状态**：不显示任何指示器
