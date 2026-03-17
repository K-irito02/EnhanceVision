# 多媒体文件删除同步与延迟优化方案

## 问题分析

### 问题1：删除多媒体文件时窗口同步问题

**当前架构问题**：
- `MediaViewerWindow.qml` 的 `mediaFiles` 是打开窗口时从 `MessageModel` 复制的数据快照
- `MediaThumbnailStrip.qml` 通过 `mediaModel` 属性获取数据，使用 `filteredModel` 作为中间过滤层
- 主窗口删除文件时，`MessageModel` 更新，但 `MediaViewerWindow` 的 `mediaFiles` 不会自动更新
- 放大窗口中删除文件时，没有机制同步回 `MessageModel`

**数据流问题**：
```
MessageModel (C++) → MessageItem.qml → MediaThumbnailStrip.qml → filteredModel (副本)
                                          ↓
                              MediaViewerWindow.qml (mediaFiles 副本)
```
三层数据副本导致同步困难。

### 问题2：删除延迟问题

**原因分析**：
1. `MediaThumbnailStrip` 的 `_rebuildFiltered()` 每次重建整个列表，效率低
2. `Connections` 监听 `rowsRemoved` 信号时，没有正确处理删除事件
3. 缺少 `onRowsRemoved` 监听，只监听了 `rowsInserted` 和 `dataChanged`

## 优化方案

### 方案架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     MessageModel (C++)                          │
│  - removeMediaFile(messageId, fileIndex) 新增方法               │
│  - fileRemovedFromMessage(messageId, fileIndex) 新增信号        │
└──────────────────────────┬──────────────────────────────────────┘
                           │ 信号广播
           ┌───────────────┼───────────────┐
           ▼               ▼               ▼
┌──────────────────┐ ┌──────────────────┐ ┌──────────────────┐
│ MessageItem.qml  │ │ MediaThumbnail   │ │ MediaViewerWindow│
│                  │ │ Strip.qml        │ │ .qml             │
│ 监听 fileRemoved │ │ 监听 fileRemoved │ │ 监听 fileRemoved │
│ 更新 _cachedMedia│ │ 更新 filteredModel│ │ 更新 mediaFiles  │
└──────────────────┘ └──────────────────┘ └──────────────────┘
```

### 核心改动

#### 1. MessageModel 新增方法（C++层）

**文件**: `include/EnhanceVision/models/MessageModel.h`

新增方法：
```cpp
/**
 * @brief 从消息中删除指定索引的媒体文件
 * @param messageId 消息ID
 * @param fileIndex 文件索引
 * @return 是否成功
 */
Q_INVOKABLE bool removeMediaFile(const QString &messageId, int fileIndex);

/**
 * @brief 从消息中删除指定ID的媒体文件
 * @param messageId 消息ID
 * @param fileId 文件ID
 * @return 是否成功
 */
Q_INVOKABLE bool removeMediaFileById(const QString &messageId, const QString &fileId);
```

新增信号：
```cpp
/**
 * @brief 消息中的媒体文件被删除
 * @param messageId 消息ID
 * @param fileIndex 被删除的文件索引
 */
void mediaFileRemoved(const QString &messageId, int fileIndex);
```

#### 2. MediaThumbnailStrip 优化

**文件**: `qml/components/MediaThumbnailStrip.qml`

改动：
1. 新增 `messageId` 属性，用于标识所属消息
2. 新增 `fileRemoved(messageId, fileIndex)` 信号
3. 优化 `_rebuildFiltered()` 为增量更新
4. 新增 `onRowsRemoved` 监听
5. 删除时调用 C++ 层方法并广播信号

```qml
Item {
    id: root
    
    property var mediaModel: ListModel {}
    property string messageId: ""  // 新增：消息ID
    
    signal fileRemoved(string messageId, int fileIndex)  // 新增信号
    
    Connections {
        target: mediaModel
        function onDataChanged(topLeft, bottomRight, roles) { _rebuildFiltered() }
        function onRowsInserted(parent, first, last) { _rebuildFiltered() }
        function onRowsRemoved(parent, first, last) { _handleRowsRemoved(first, last) }  // 新增
        function onModelReset() { _rebuildFiltered() }
    }
    
    function _handleRowsRemoved(first, last) {
        // 增量更新，而非全量重建
        for (var i = filteredModel.count - 1; i >= 0; i--) {
            var item = filteredModel.get(i)
            if (item.origIndex >= first && item.origIndex <= last) {
                filteredModel.remove(i)
            } else if (item.origIndex > last) {
                // 更新索引
                filteredModel.setProperty(i, "origIndex", item.origIndex - (last - first + 1))
            }
        }
    }
    
    onDeleteFile: function(index) {
        var item = filteredModel.get(index)
        var origIndex = item ? item.origIndex : index
        root.fileRemoved(root.messageId, origIndex)
    }
}
```

#### 3. MessageItem 改动

**文件**: `qml/components/MessageItem.qml`

改动：
1. 传递 `messageId` 给 `MediaThumbnailStrip`
2. 监听 `fileRemoved` 信号并调用 C++ 层删除
3. 更新 `_cachedMedia`

```qml
MediaThumbnailStrip {
    Layout.fillWidth: true
    mediaModel: root.mediaFiles
    messageId: root.taskId  // 新增
    
    onViewFile: function(index) { root.viewMediaFile(index) }
    onSaveFile: function(index) { root.saveMediaFile(index) }
    onDeleteFile: function(index) { root.deleteMediaFile(index) }
    onFileRemoved: function(msgId, fileIndex) {  // 新增
        root.deleteMediaFile(fileIndex)
    }
}
```

#### 4. MessageList 改动

**文件**: `qml/components/MessageList.qml`

改动：
1. `onDeleteMediaFile` 调用 C++ 层删除方法
2. 监听 `MessageModel.mediaFileRemoved` 信号
3. 同步更新 `MediaViewerWindow` 的数据

```qml
Connections {
    target: root._hasRealModel ? messageModel : null
    function onMediaFileRemoved(messageId, fileIndex) {
        // 如果当前查看器窗口打开的是该消息，同步更新
        if (viewerWindow.visible && viewerWindow.currentMessageId === messageId) {
            _syncViewerWindow(messageId)
        }
    }
}

function _syncViewerWindow(messageId) {
    var files = []
    var allFiles = messageModel.getMediaFiles(messageId)
    // ... 重新构建 files 数组
    viewerWindow.mediaFiles = files
    
    // 如果当前查看的文件被删除，切换到邻近文件
    if (viewerWindow.currentIndex >= files.length) {
        viewerWindow.currentIndex = Math.max(0, files.length - 1)
    }
    
    // 如果没有文件了，关闭窗口
    if (files.length === 0) {
        viewerWindow.close()
    }
}
```

#### 5. MediaViewerWindow 改动

**文件**: `qml/components/MediaViewerWindow.qml`

改动：
1. 新增 `messageId` 属性标识当前查看的消息
2. 新增 `fileRemoved(messageId, fileIndex)` 信号
3. 底部缩略图条新增删除按钮
4. 删除时发出信号通知父组件

```qml
Window {
    id: root
    
    property string messageId: ""  // 新增：当前消息ID
    
    signal fileRemoved(string messageId, int fileIndex)
    
    // 底部缩略图 delegate 中添加删除按钮
    delegate: Item {
        // ... 现有代码 ...
        
        Rectangle {
            id: deleteBtn
            anchors.top: parent.top
            anchors.right: parent.right
            width: 16; height: 16
            radius: 8
            color: deleteBtnMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.6)
            visible: bottomThumbMouse.containsMouse
            
            Text {
                anchors.centerIn: parent
                text: "\u00D7"
                color: "#FFFFFF"
                font.pixelSize: 10
            }
            
            MouseArea {
                id: deleteBtnMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.fileRemoved(root.messageId, index)
                }
            }
        }
    }
    
    // 处理删除后的索引调整
    function handleFileRemoved(deletedIndex) {
        if (mediaFiles.length === 0) {
            close()
            return
        }
        
        // 调整当前索引
        if (currentIndex >= deletedIndex && currentIndex > 0) {
            currentIndex--
        }
        if (currentIndex >= mediaFiles.length) {
            currentIndex = mediaFiles.length - 1
        }
    }
}
```

### 实现步骤

#### 第一阶段：C++ 层改动

1. **MessageModel.h** - 添加新方法和信号声明
2. **MessageModel.cpp** - 实现 `removeMediaFile` 和 `removeMediaFileById` 方法

#### 第二阶段：QML 层改动

3. **MediaThumbnailStrip.qml** - 优化删除逻辑和同步机制
4. **MessageItem.qml** - 传递 messageId 并处理删除
5. **MessageList.qml** - 实现 C++ 调用和窗口同步
6. **MediaViewerWindow.qml** - 添加删除按钮和同步处理

#### 第三阶段：测试验证

7. 测试主窗口删除同步到放大窗口
8. 测试放大窗口删除同步到主窗口
9. 测试删除当前查看文件时的自动切换
10. 测试删除延迟是否改善

### 关键代码实现

#### MessageModel.cpp 新增实现

```cpp
bool MessageModel::removeMediaFile(const QString &messageId, int fileIndex)
{
    int msgIdx = findMessageIndex(messageId);
    if (msgIdx < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return false;
    }

    Message &message = m_messages[msgIdx];
    if (fileIndex < 0 || fileIndex >= message.mediaFiles.size()) {
        emit errorOccurred(tr("文件索引无效: %1").arg(fileIndex));
        return false;
    }

    message.mediaFiles.removeAt(fileIndex);
    
    QModelIndex modelIndex = createIndex(msgIdx, 0);
    emit dataChanged(modelIndex, modelIndex, {MediaFilesRole});
    emit mediaFileRemoved(messageId, fileIndex);
    
    return true;
}

bool MessageModel::removeMediaFileById(const QString &messageId, const QString &fileId)
{
    int msgIdx = findMessageIndex(messageId);
    if (msgIdx < 0) {
        return false;
    }

    Message &message = m_messages[msgIdx];
    for (int i = 0; i < message.mediaFiles.size(); ++i) {
        if (message.mediaFiles[i].id == fileId) {
            return removeMediaFile(messageId, i);
        }
    }
    return false;
}
```

### 性能优化点

1. **增量更新替代全量重建**：`_handleRowsRemoved` 只移除和更新受影响的项
2. **信号广播机制**：通过 C++ 信号统一广播删除事件，避免多层回调
3. **即时 UI 响应**：删除后立即更新 UI，不等待异步操作

### 边界情况处理

| 场景 | 处理方式 |
|------|---------|
| 删除正在查看的文件 | 自动切换到邻近文件 |
| 删除后无文件剩余 | 关闭放大窗口，显示空状态提示 |
| 删除第一个文件 | 切换到新的第一个文件 |
| 删除最后一个文件 | 切换到前一个文件 |
| 放大窗口未打开时删除 | 只更新主窗口数据 |
| 同时打开多个放大窗口 | 通过 messageId 区分，只更新对应窗口 |

### 文件修改清单

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `include/EnhanceVision/models/MessageModel.h` | 修改 | 新增方法和信号 |
| `src/models/MessageModel.cpp` | 修改 | 实现新方法 |
| `qml/components/MediaThumbnailStrip.qml` | 修改 | 优化删除和同步 |
| `qml/components/MessageItem.qml` | 修改 | 传递 messageId |
| `qml/components/MessageList.qml` | 修改 | 实现同步逻辑 |
| `qml/components/MediaViewerWindow.qml` | 修改 | 添加删除按钮和同步 |
