# 修复待处理预览区域空白问题计划

## 问题描述

上传多媒体文件到待处理预览区域时，会多出来高度和"待处理预览区域"一样的空白区域。

## 期望效果

1. 根据最新消息卡片是否完整显示决定"待处理预览区域"和"最小化停靠区域"是否可覆盖消息展示区域中的卡片
2. 当"待处理预览区域"和"最小化停靠区域"将要出现时，若是当下新消息卡片完整显示，则消息展示区域中整体的消息卡片自动上移
3. 当"待处理预览区域"和"最小化停靠区域"将要出现时，若是当下新消息卡片没有完整显示，则消息展示区域中整体的消息卡片可以被进行覆盖

## 问题根因分析

### 当前实现分析

**MainPage.qml** 中的 `MessageList` 的 `anchors.bottomMargin` 计算逻辑：

```qml
anchors.bottomMargin: {
    var baseMargin = 12
    var dockHeight = pendingMinimizedDock.height
    if (!canOverlayLastMessage && root.hasFiles) {
        return baseMargin + dockHeight + pendingFilePreviewArea.height  // 问题所在
    }
    return baseMargin + dockHeight
}
```

**问题**：
- 当 `canOverlayLastMessage` 为 false（最新消息卡片完整显示）且有文件时，会额外添加 `pendingFilePreviewArea.height`（100px）
- 这导致消息列表底部有额外的空白区域

**MessageList.qml** 中的 `canOverlayLastMessage` 属性：
```qml
property bool isLastMessageFullyVisible: true
readonly property bool canOverlayLastMessage: !isLastMessageFullyVisible
```

### 逻辑问题

1. **空白区域来源**：`pendingFilePreviewArea.height` 被添加到 bottomMargin，但预览区域本身已经占据了空间（在 ColumnLayout 中），导致重复计算
2. **覆盖逻辑不完整**：需要根据最新消息卡片的可见性动态调整行为

## 解决方案

### 修改文件

1. **MainPage.qml** - 修改 MessageList 的 bottomMargin 计算逻辑
2. **MessageList.qml** - 增强可见性检测和自动滚动逻辑

### 具体修改

#### 1. MainPage.qml - 修改 bottomMargin 逻辑

**修改前**：
```qml
anchors.bottomMargin: {
    var baseMargin = 12
    var dockHeight = pendingMinimizedDock.height
    if (!canOverlayLastMessage && root.hasFiles) {
        return baseMargin + dockHeight + pendingFilePreviewArea.height
    }
    return baseMargin + dockHeight
}
```

**修改后**：
```qml
anchors.bottomMargin: {
    var baseMargin = 12
    var dockHeight = pendingMinimizedDock.height
    // 当最新消息卡片完整显示时，为预览区域预留空间（消息列表会上移）
    // 当最新消息卡片不完整显示时，不预留空间（允许预览区域覆盖）
    if (canOverlayLastMessage) {
        return baseMargin + dockHeight
    }
    // 最新消息卡片完整显示时，需要为预览区域和停靠区域预留空间
    if (root.hasFiles) {
        return baseMargin + dockHeight + pendingFilePreviewArea.height
    }
    return baseMargin + dockHeight
}
```

#### 2. MainPage.qml - 调整 z-index 确保覆盖效果

**修改前**：
```qml
MinimizedWindowDock {
    id: pendingMinimizedDock
    ...
    z: messageListView.canOverlayLastMessage ? 100 : 5
    ...
}
```

**修改后**：
```qml
MinimizedWindowDock {
    id: pendingMinimizedDock
    ...
    // 当允许覆盖时，z-index 要高于消息列表
    z: messageListView.canOverlayLastMessage ? 100 : 5
    ...
}
```

同时需要为 `pendingFilePreviewArea` 添加 z-index：
```qml
Rectangle {
    id: pendingFilePreviewArea
    ...
    z: messageListView.canOverlayLastMessage ? 100 : 5
    ...
}
```

#### 3. MessageList.qml - 增强可见性检测和自动滚动

添加监听预览区域变化的逻辑，当预览区域出现且最新消息卡片完整显示时，自动滚动：

```qml
// 监听预览区域高度变化
Connections {
    target: root.parent
    enabled: root.parent !== null
    function onHasFilesChanged() {
        if (root.parent.hasFiles && root.isLastMessageFullyVisible) {
            // 预览区域出现且最新消息完整显示时，自动滚动以保持可见
            Qt.callLater(messageList.scrollToBottomAnimated)
        }
        Qt.callLater(root._updateLastMessageVisibility)
    }
}
```

#### 4. MainPage.qml - 传递 hasFiles 属性给 MessageList

需要将 `hasFiles` 属性传递给 `MessageList`，以便其能够监听预览区域的变化：

```qml
MessageList {
    id: messageListView
    ...
    property bool hasFiles: root.hasFiles
    ...
}
```

## 实施步骤

1. **修改 MainPage.qml**
   - 修改 `MessageList` 的 `anchors.bottomMargin` 计算逻辑
   - 为 `pendingFilePreviewArea` 添加 `z` 属性
   - 将 `hasFiles` 属性传递给 `MessageList`

2. **修改 MessageList.qml**
   - 添加 `hasFiles` 属性
   - 添加监听 `hasFiles` 变化的 Connections
   - 增强自动滚动逻辑

3. **测试验证**
   - 测试场景1：无消息时上传文件，验证预览区域正常显示
   - 测试场景2：有消息且最新消息完整显示时上传文件，验证消息列表自动上移
   - 测试场景3：有消息且最新消息不完整显示时上传文件，验证预览区域覆盖消息卡片
   - 测试场景4：删除文件后，验证布局恢复正常

## 风险评估

1. **低风险**：修改仅涉及布局逻辑，不影响业务功能
2. **兼容性**：需要确保 `canOverlayLastMessage` 属性正确计算
3. **性能**：自动滚动逻辑使用 `Qt.callLater` 避免频繁更新

## 验收标准

1. 上传文件后不再出现空白区域
2. 最新消息卡片完整显示时，消息列表自动上移为预览区域腾出空间
3. 最新消息卡片不完整显示时，预览区域覆盖消息卡片
4. 删除文件后布局恢复正常
5. 现有功能不受影响
