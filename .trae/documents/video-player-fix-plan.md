# 视频播放器功能修复计划

## 问题分析

经过代码审查，发现问题出在 `EmbeddedMediaViewer.qml` 中的 `VideoControlBar` 组件（第557-583行）。与功能完整的 `MediaViewerWindow.qml` 相比，存在以下功能缺失：

### 1. 音量控制失效
**现状**：第579-580行的音量滑块 `value: 0.8` 是硬编码的，没有绑定到 `AudioOutput.volume`
```qml
Slider { implicitWidth: 80; from: 0; to: 1; value: 0.8 }  // 无效绑定
```
**对比**：`MediaViewerWindow.qml` 第909-936行正确实现了音量控制

### 2. 倍速选项不完整
**现状**：第577行只有 `[1.0, 1.5, 2.0]` 三个选项
```qml
Repeater { model: [1.0, 1.5, 2.0]; ... }
```
**对比**：`MediaViewerWindow.qml` 第840行有 `[0.5, 1.0, 1.5, 2.0, 2.5, 3.0]` 六个选项

### 3. 缺少视频横幅
**现状**：`MediaContentArea` 组件没有视频预览帧
**对比**：`MediaViewerWindow.qml` 第439-481行有 `videoPreviewFrame`，在视频停止或结束时显示缩略图

### 4. 点击画面暂停/播放缺失
**现状**：`MediaContentArea` 的视频区域没有点击事件处理
**对比**：`MediaViewerWindow.qml` 第579-606行有 `videoClickArea` 处理点击暂停/播放

---

## 修复方案

### 修改文件
- `qml/components/EmbeddedMediaViewer.qml`

### 具体修改

#### 修改 1：修复音量控制绑定
在 `VideoControlBar` 组件中，将音量滑块绑定到 `audioOutput.volume`

**修改位置**：第579-580行
```qml
// 修改前
ColoredIcon { source: Theme.icon("volume-2"); iconSize: 16; color: Theme.colors.foreground }
Slider { implicitWidth: 80; from: 0; to: 1; value: 0.8 }

// 修改后
IconButton { 
    iconName: contentArea.audioOutput.volume > 0 ? "volume-2" : "volume-x"
    iconSize: 16; btnSize: 28
    iconColor: Theme.colors.mediaControlIcon
    tooltip: qsTr("静音")
    onClicked: contentArea.audioOutput.volume = contentArea.audioOutput.volume > 0 ? 0 : 0.7
}
Slider { 
    implicitWidth: 80
    from: 0
    to: 1
    value: contentArea.audioOutput.volume
    onMoved: contentArea.audioOutput.volume = value
}
```

#### 修改 2：扩展倍速选项
**修改位置**：第577行
```qml
// 修改前
Repeater { model: [1.0, 1.5, 2.0]; ... }

// 修改后
Repeater { model: [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]; ... }
```

#### 修改 3：添加视频预览帧（横幅）
在 `MediaContentArea` 组件的视频区域添加预览帧，在视频停止或结束时显示

**修改位置**：第493-504行（视频容器 Item 内）
```qml
// 在 VideoOutput 之前添加预览帧
Image {
    id: videoPreviewFrame
    anchors.fill: parent
    fillMode: Image.PreserveAspectFit
    asynchronous: true
    smooth: true
    mipmap: true
    visible: viewer.isVideo && videoPlayer && (videoPlayer.playbackState === MediaPlayer.StoppedState || viewer._videoEnded)
    opacity: visible ? 1.0 : 0.0
    z: 1
    
    source: {
        if (!viewer.isVideo || !viewer.currentSource || viewer.currentSource === "") return ""
        var src = viewer.currentSource
        if (src.startsWith("file:///")) src = src.substring(8)
        return "image://thumbnail/" + src
    }
    
    ColoredIcon {
        anchors.centerIn: parent
        source: Theme.icon("video")
        iconSize: 64
        color: Theme.colors.mutedForeground
        visible: parent.status === Image.Error || parent.status === Image.Null
    }
    
    Behavior on opacity { NumberAnimation { duration: 200 } }
}
```

#### 修改 4：添加点击画面暂停/播放
在视频区域添加 MouseArea 处理点击事件

**修改位置**：第493-504行（视频容器 Item 内）
```qml
// 在播放按钮之后添加
MouseArea {
    id: videoClickArea
    anchors.fill: parent
    hoverEnabled: true
    propagateComposedEvents: true
    acceptedButtons: Qt.LeftButton
    z: 2
    
    onContainsMouseChanged: {
        if (containsMouse) {
            showNavButtons = true
            navButtonHideTimer.restart()
        }
    }
    
    onClicked: function(mouse) {
        if (videoPlayer) videoPlayer.togglePlay()
        mouse.accepted = false
    }
    
    onDoubleClicked: {
        // 可选：双击全屏
    }
}
```

#### 修改 5：添加视频结束状态追踪
需要在 `EmbeddedMediaViewer.qml` 根组件添加 `_videoEnded` 属性

**修改位置**：第51行附近
```qml
property var _mediaPlayer: null
property bool _videoEnded: false  // 新增
```

并在 MediaPlayer 的 `onPlaybackStateChanged` 中更新状态：

**修改位置**：第267-273行
```qml
MediaPlayer {
    id: vidPlayer
    videoOutput: contentArea.videoOutput
    audioOutput: contentArea.audioOutput
    function togglePlay() { playbackState === MediaPlayer.PlayingState ? pause() : play() }
    
    onPlaybackStateChanged: {
        if (playbackState === MediaPlayer.StoppedState) {
            if (position > 0 && duration > 0 && position >= duration - 500) {
                root._videoEnded = true
            }
        } else if (playbackState === MediaPlayer.PlayingState) {
            root._videoEnded = false
        }
    }
    
    Component.onCompleted: root._mediaPlayer = this
}
```

---

## 实施步骤

1. **修复音量控制**：绑定滑块到 AudioOutput.volume，添加静音按钮
2. **扩展倍速选项**：从 3 个选项扩展到 6 个
3. **添加视频预览帧**：在视频停止/结束时显示缩略图
4. **添加点击暂停/播放**：在视频区域添加点击事件处理
5. **添加视频结束状态**：追踪视频播放结束状态

---

## 验证方法

1. 打开一个视频文件
2. 验证点击视频画面可以暂停/播放
3. 验证视频开始时显示预览帧
4. 验证视频播放结束后显示预览帧
5. 验证倍速选项包含 0.5x、2.5x、3.0x
6. 验证音量滑块可以正常调节音量
7. 验证静音按钮可以切换静音状态
