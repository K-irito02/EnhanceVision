# 修复消息卡片缩略图删除按钮不起作用问题

## 问题分析

### 问题现象
消息卡片上，点击缩略图右上角的 `x` 删除按钮不起作用。

### 根本原因

经过深入分析，发现问题有两个层面：

#### 1. MouseArea 事件拦截问题（次要）

在 `MediaThumbnailStrip.qml` 中：
- `thumbMouse` 覆盖整个缩略图区域，处理点击事件
- `deleteBtn` 和 `deleteBtnForFailed` 虽然设置了 `z: 10`，但其内部的 MouseArea 可能被 `thumbMouse` 拦截

#### 2. 属性名错误（主要）

在 `MessageItem.qml` 第 368 行：
```qml
onDeleteFile: function(index) { var _msgId = root.messageId; ... }
```

这里使用的是 `root.messageId`，但 `MessageItem.qml` 中的属性是 `taskId`，不是 `messageId`。`root.messageId` 是 undefined，导致 `messageModel.removeMediaFile(undefined, index)` 无法正确执行删除操作。

## 解决方案

### 修复 1：为删除按钮的 MouseArea 添加 `preventStealing: true`

修改 `MediaThumbnailStrip.qml` 文件中的 4 处删除按钮 MouseArea：
- `horizontalComponent - deleteBtnMouse`
- `horizontalComponent - deleteBtnForFailedMouse`
- `gridComponent - deleteBtnMouse`
- `gridComponent - deleteBtnForFailedMouse`

### 修复 2：为 thumbMouse 添加事件传播

在 `thumbMouse` 中设置 `propagateComposedEvents: true`，并在 `onPressed` 中检查鼠标位置是否在删除按钮区域内，如果是则不处理事件。

### 修复 3：修正属性名（关键修复）

将 `MessageItem.qml` 中的 `root.messageId` 改为 `root.taskId`：
```qml
onDeleteFile: function(index) { var _msgId = root.taskId; var _idx = index; Qt.callLater(function() { messageModel.removeMediaFile(_msgId, _idx) }) }
```

## 修改的文件

| 文件路径 | 修改内容 |
|----------|----------|
| `qml/components/MediaThumbnailStrip.qml` | 为 4 处删除按钮 MouseArea 添加 `preventStealing: true`；为 2 处 `thumbMouse` 添加 `propagateComposedEvents: true` 和 `onPressed` 事件处理 |
| `qml/components/MessageItem.qml` | 将 `onDeleteFile` 中的 `root.messageId` 改为 `root.taskId` |

## 验证结果

✅ 测试通过：删除按钮功能正常，可以正确删除消息卡片上的多媒体文件。
