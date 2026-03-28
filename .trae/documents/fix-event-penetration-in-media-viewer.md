# 放大查看器事件穿透问题修复计划

## 问题描述

在对图片类型的多媒体文件进行放大查看时，疯狂点击图像上的某些地方，会穿透到消息展示区域的其它消息卡片的多媒体文件中。

## 问题根因分析

### 1. 嵌入式查看器 (EmbeddedMediaViewer.qml) 事件拦截不完整

在 `embeddedOverlay` 组件中（第359-368行），MouseArea 只处理了 `onClicked` 和 `onWheel` 事件：

```qml
MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.AllButtons
    onClicked: function(mouse) { mouse.accepted = true }
    onWheel: function(wheel) { wheel.accepted = true }
}
```

**缺失的事件处理**：
- `onPressed` - 鼠标按下事件未拦截
- `onReleased` - 鼠标释放事件未拦截  
- `onDoubleClicked` - 双击事件未拦截
- `onPressAndHold` - 长按事件未拦截

在 QML 中，如果一个 MouseArea 不处理 `pressed` 事件，事件可能会继续传递到下层的可视项。

### 2. MediaContentArea 内部 MouseArea 配置问题

在 `MediaContentArea` 组件中（第816-832行），用于检测鼠标悬停的 MouseArea 设置了：

```qml
MouseArea {
    acceptedButtons: Qt.NoButton  // 不拦截任何点击事件
    z: 10
}
```

这个 MouseArea 只用于悬停检测，不拦截任何点击，导致点击事件可能穿透到下层。

### 3. 图片显示区域缺少点击事件拦截

当用户点击图片区域时，事件路径：
1. `MediaContentArea` 内的图片 MouseArea (`acceptedButtons: Qt.NoButton`) 不拦截
2. 事件传递到 `embeddedOverlay` 的 MouseArea
3. 由于 `onPressed` 未处理，事件可能穿透到底层

### 4. 独立窗口查看器 (MediaViewerWindow.qml)

独立窗口不存在事件穿透问题，因为它是独立的 Window，事件不会传递到其他窗口。

## 修复方案

### 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `qml/components/EmbeddedMediaViewer.qml` | 完善 `embeddedOverlay` 的 MouseArea 事件拦截 |
| `qml/components/EmbeddedMediaViewer.qml` | 完善 `MediaContentArea` 内部事件处理 |

### 具体修改步骤

#### 步骤 1：完善 embeddedOverlay 的 MouseArea 事件拦截

**位置**：`EmbeddedMediaViewer.qml` 第359-368行

**修改前**：
```qml
MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.AllButtons
    onClicked: function(mouse) {
        mouse.accepted = true
    }
    onWheel: function(wheel) {
        wheel.accepted = true
    }
}
```

**修改后**：
```qml
MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.AllButtons
    hoverEnabled: true
    onPressed: function(mouse) {
        mouse.accepted = true
    }
    onReleased: function(mouse) {
        mouse.accepted = true
    }
    onClicked: function(mouse) {
        mouse.accepted = true
    }
    onDoubleClicked: function(mouse) {
        mouse.accepted = true
    }
    onPressAndHold: function(mouse) {
        mouse.accepted = true
    }
    onWheel: function(wheel) {
        wheel.accepted = true
    }
}
```

#### 步骤 2：完善 MediaContentArea 内图片区域的 MouseArea

**位置**：`EmbeddedMediaViewer.qml` 第816-832行

**修改前**：
```qml
MouseArea {
    anchors.fill: parent
    hoverEnabled: true
    acceptedButtons: Qt.NoButton
    z: 10
    
    onPositionChanged: {
        showNavButtonsAndResetTimer()
    }
    onContainsMouseChanged: {
        if (containsMouse) {
            showNavButtonsAndResetTimer()
        } else {
            startAutoHideIfNeeded()
        }
    }
}
```

**修改后**：
```qml
MouseArea {
    anchors.fill: parent
    hoverEnabled: true
    acceptedButtons: Qt.AllButtons
    z: 10
    
    onPressed: function(mouse) {
        mouse.accepted = true
    }
    onReleased: function(mouse) {
        mouse.accepted = true
    }
    onClicked: function(mouse) {
        mouse.accepted = true
    }
    onDoubleClicked: function(mouse) {
        mouse.accepted = true
    }
    onPositionChanged: {
        showNavButtonsAndResetTimer()
    }
    onContainsMouseChanged: {
        if (containsMouse) {
            showNavButtonsAndResetTimer()
        } else {
            startAutoHideIfNeeded()
        }
    }
}
```

#### 步骤 3：确保视频区域的 MouseArea 也正确拦截事件

**位置**：`EmbeddedMediaViewer.qml` 第944-966行

当前配置已正确设置 `acceptedButtons: Qt.LeftButton`，但建议改为拦截所有按钮以防止穿透：

**修改前**：
```qml
MouseArea {
    id: videoClickArea
    anchors.fill: parent
    hoverEnabled: true
    acceptedButtons: Qt.LeftButton
    z: 2
    // ...
}
```

**修改后**：
```qml
MouseArea {
    id: videoClickArea
    anchors.fill: parent
    hoverEnabled: true
    acceptedButtons: Qt.AllButtons
    z: 2
    
    onPressed: function(mouse) {
        mouse.accepted = true
    }
    onReleased: function(mouse) {
        mouse.accepted = true
    }
    onClicked: function(mouse) {
        if (mouse.button === Qt.LeftButton && videoPlayer) {
            videoPlayer.togglePlay()
        }
        mouse.accepted = true
    }
    onDoubleClicked: function(mouse) {
        if (mouse.button === Qt.LeftButton) {
            // 可选：双击切换全屏
        }
        mouse.accepted = true
    }
    // ... 其他处理
}
```

## 验证方案

### 测试场景

1. **基本点击测试**：在嵌入式查看器中点击图片各处，确认不会穿透到底层
2. **快速连续点击测试**：快速连续点击图片，确认所有点击都被正确拦截
3. **双击测试**：双击图片区域，确认不会触发底层元素
4. **导航按钮测试**：确认左右导航按钮仍能正常工作
5. **视频播放测试**：确认视频区域的点击播放功能正常
6. **缩略图栏测试**：确认底部缩略图栏的点击切换功能正常
7. **标题栏按钮测试**：确认标题栏的关闭、最小化、独立窗口按钮正常工作

### 构建验证

修改完成后使用 `qt-build-and-fix` 技能构建并运行项目，检查日志确保无警告或错误。

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 过度拦截导致功能失效 | 导航按钮或其他交互失效 | 保持各子组件 MouseArea 的独立性，只修改背景拦截层 |
| 触摸事件未处理 | 触摸屏设备可能仍有穿透 | 添加 TouchArea 或在 MouseArea 中处理触摸事件 |

## 预计工作量

- 代码修改：约 15 分钟
- 构建验证：约 5 分钟
- 手动测试：约 10 分钟
- **总计**：约 30 分钟
