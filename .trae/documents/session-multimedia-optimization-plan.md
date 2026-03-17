# 会话消息统计与多媒体展示优化计划

## 问题分析

### 问题1：会话标签消息条数实时统计

**当前实现状态：**
- `SessionModel::data()` 返回 `session.messages.size()` 作为消息计数
- `SessionModel::notifySessionDataChanged()` 方法已存在
- 消息添加时通过 `SessionController::syncCurrentMessagesToSession()` 同步

**问题根源：**
1. 消息删除时未正确触发会话数据更新
2. `MessageModel::removeMessage()` 和 `MessageModel::removeMediaFile()` 后未同步到 SessionModel
3. 缺少从 MessageModel 到 SessionModel 的自动同步机制

### 问题2：多媒体消息展示与展开/收缩功能

**当前实现状态：**
- `MediaThumbnailStrip.qml` 使用水平 ListView
- 展开收缩按钮固定在底部
- 收缩时限制显示数量，但无法滚动查看更多

**问题根源：**
1. **布局问题**：用户期望网格布局（从左到右、从上到下），当前是水平滚动
2. **滚动限制**：收缩状态下 `model` 被截断为 `Math.min(count, collapsedMaxVisible)`，导致无法滚动查看全部
3. **按钮位置**：收缩按钮固定在底部，浏览中途无法快速操作

---

## 实施方案

### 任务1：会话消息条数实时统计优化

#### 1.1 修改 `MessageModel.cpp` - 添加同步信号

**文件：** `src/models/MessageModel.cpp`

**修改内容：**
- 在 `removeMessage()` 方法中，删除消息后发出 `messageCountChanged` 信号
- 在 `removeMediaFile()` 方法中，当消息被删除时发出信号
- 在 `clear()` 方法中发出信号

#### 1.2 修改 `SessionController.cpp` - 监听消息变化

**文件：** `src/controllers/SessionController.cpp`

**修改内容：**
- 连接 `MessageModel::countChanged` 信号到会话数据更新
- 确保所有消息变更都触发 `notifySessionDataChanged()`

#### 1.3 修改 `ProcessingController.cpp` - 同步删除操作

**文件：** `src/controllers/ProcessingController.cpp`

**修改内容：**
- 在任务删除/取消后同步更新会话消息计数

---

### 任务2：多媒体消息展示与展开/收缩功能优化

#### 2.1 重构 `MediaThumbnailStrip.qml` - 网格布局

**文件：** `qml/components/MediaThumbnailStrip.qml`

**修改内容：**

1. **改为网格布局**
   - 使用 `GridLayout` 或 `Grid` 替代水平 `ListView`
   - 图片按"从左到右、从上到下"顺序排列
   - 根据容器宽度自动计算列数

2. **修复收缩状态滚动问题**
   - 收缩状态下不截断 model，而是限制容器高度
   - 允许用户通过滚动查看所有文件
   - 显示"收缩"按钮让用户可以随时收缩

3. **重新设计展开/收缩按钮**
   - 按钮位置改为悬浮在右上角或顶部
   - 收缩时显示"展开查看全部 N 个文件"提示
   - 展开时显示"收缩"按钮
   - 按钮样式：半透明背景，不遮挡内容浏览

#### 2.2 具体实现方案

**网格布局实现：**
```qml
// 使用 GridView 替代水平 ListView
GridView {
    id: thumbGridView
    cellWidth: root.thumbSize + root.thumbSpacing
    cellHeight: root.thumbSize + root.thumbSpacing
    
    // 收缩时限制高度
    implicitHeight: root.expanded ? 
        Math.ceil(filteredModel.count / columns) * cellHeight :
        Math.min(Math.ceil(filteredModel.count / columns) * cellHeight, maxHeight)
    
    // 允许滚动
    clip: true
    boundsBehavior: Flickable.StopAtBounds
}
```

**展开/收缩按钮设计：**
```qml
// 悬浮按钮在顶部
Rectangle {
    id: expandButton
    anchors.top: parent.top
    anchors.right: parent.right
    anchors.margins: 8
    width: expandText.implicitWidth + 24
    height: 28
    radius: 14
    color: Qt.rgba(0, 0, 0, 0.6)
    visible: root.expandable && filteredModel.count > root.collapsedMaxVisible
    
    Row {
        anchors.centerIn: parent
        spacing: 4
        Text { text: root.expanded ? "收缩" : "展开全部" }
        ColoredIcon { source: root.expanded ? "chevron-up" : "chevron-down" }
    }
}
```

---

## 详细任务列表

### 任务1：会话消息条数实时统计

| 序号 | 任务 | 文件 | 优先级 |
|------|------|------|--------|
| 1.1 | 确保消息删除时触发会话数据更新 | `MessageModel.cpp` | 高 |
| 1.2 | 在 SessionController 中连接消息计数变化信号 | `SessionController.cpp` | 高 |
| 1.3 | 确保媒体文件删除后同步更新消息计数 | `MessageModel.cpp` | 高 |

### 任务2：多媒体展示优化

| 序号 | 任务 | 文件 | 优先级 |
|------|------|------|--------|
| 2.1 | 将水平 ListView 改为网格 GridView | `MediaThumbnailStrip.qml` | 高 |
| 2.2 | 实现收缩状态下的滚动功能 | `MediaThumbnailStrip.qml` | 高 |
| 2.3 | 重新设计展开/收缩按钮位置和样式 | `MediaThumbnailStrip.qml` | 高 |
| 2.4 | 添加"查看全部"提示信息 | `MediaThumbnailStrip.qml` | 中 |

---

## 技术要点

### 消息计数同步流程

```
MessageModel 变化 → countChanged 信号 → SessionController 监听 
    → syncCurrentMessagesToSession() → SessionModel::notifySessionDataChanged()
    → emit dataChanged(MessageCountRole) → QML UI 更新
```

### 网格布局计算

```javascript
// 计算列数
property int columns: Math.max(1, Math.floor(availableWidth / (thumbSize + thumbSpacing)))

// 计算行数
property int rows: Math.ceil(filteredModel.count / columns)

// 计算高度
property int contentHeight: rows * (thumbSize + thumbSpacing) - thumbSpacing
```

### 收缩状态处理

```javascript
// 收缩时显示的最大高度（约3行）
property int collapsedMaxHeight: 3 * (thumbSize + thumbSpacing)

// 实际显示高度
height: expanded ? contentHeight : Math.min(contentHeight, collapsedMaxHeight)
```

---

## 验收标准

### 任务1验收
- [ ] 添加消息后，会话标签消息数立即更新
- [ ] 删除消息后，会话标签消息数立即更新
- [ ] 删除媒体文件导致消息删除时，消息数正确更新
- [ ] 清空会话后，消息数显示为 0

### 任务2验收
- [ ] 多媒体文件按网格布局显示（从左到右、从上到下）
- [ ] 收缩状态下可以通过滚动查看所有文件
- [ ] 展开/收缩按钮位置合理，不遮挡内容
- [ ] 按钮样式美观，符合 UI/UX 标准
- [ ] 动画过渡流畅自然
