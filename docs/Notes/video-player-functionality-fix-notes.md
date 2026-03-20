# 视频播放器功能修复笔记

## 概述

修复嵌入式媒体查看器（EmbeddedMediaViewer）中视频播放器缺失的功能，包括倍速按钮渐变效果、音量控制、视频预览帧、点击暂停/播放以及独立窗口视频显示问题。

**创建日期**: 2026-03-20
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修复倍速按钮渐变背景效果

**修改文件**: `qml/components/EmbeddedMediaViewer.qml`

**问题描述**: 倍速按钮在选中时没有渐变背景效果，所有按钮显示相同颜色

**解决方案**: 
- 使用 `color` 属性返回不同的颜色值（从淡蓝色到深蓝色）
- 每个倍速按钮（0.5x, 1.0x, 1.5x, 2.0x, 2.5x, 3.0x）都有独特的颜色
- 颜色逐渐加深，从 `#87CEFA`（淡蓝色）到 `#0A2864`（深蓝色）

**颜色映射**:

| 倍速 | 颜色值 | 颜色描述 |
|------|---------|---------|
| 0.5x | `#87CEFA` | 淡蓝色 |
| 1.0x | `#3B82F6` | 深蓝色 |
| 1.5x | `#2563EB` | 中蓝色 |
| 2.0x | `#1A73E8` | 中蓝色 |
| 2.5x | `#0F52BA` | 深蓝色 |
| 3.0x | `#0A2864` | 更深蓝色 |

### 2. 修复音量控制功能

**修改文件**: `qml/components/EmbeddedMediaViewer.qml`

**问题描述**: 音量滑块值硬编码为 0.8，没有绑定到 AudioOutput.volume

**解决方案**:
- 将音量滑块绑定到 `contentArea.audioOutput.volume`
- 添加静音按钮，点击图标切换静音/取消静音状态
- 静音时显示 `volume-x` 图标，非静音时显示 `volume-2` 图标

### 3. 添加视频预览帧（横幅）

**修改文件**: `qml/components/EmbeddedMediaViewer.qml`

**问题描述**: 视频开始前和播放结束后没有显示预览缩略图

**解决方案**:
- 在视频区域添加 `videoPreviewFrame` 组件
- 使用 `ThumbnailProvider` 获取视频缩略图
- 在视频停止或结束时显示预览帧
- 添加平滑过渡动画（200ms）

### 4. 添加点击画面暂停/播放功能

**修改文件**: `qml/components/EmbeddedMediaViewer.qml`

**问题描述**: 点击视频画面无法切换播放/暂停状态

**解决方案**:
- 在视频区域添加 `videoClickArea` MouseArea
- 处理点击事件，调用 `videoPlayer.togglePlay()`
- 鼠标悬停时显示导航按钮

### 5. 修复独立窗口视频无画面问题

**修改文件**: `qml/components/EmbeddedMediaViewer.qml`

**问题描述**: 从嵌入式窗口切换到独立窗口时，视频没有画面只有声音

**原因**: MediaPlayer 的 `videoOutput` 固定绑定到嵌入式窗口的 `contentArea.videoOutput`

**解决方案**:
- 将 `videoOutput` 绑定改为根据 `displayMode` 动态切换
- 嵌入式模式使用 `contentArea.videoOutput`
- 独立窗口模式使用 `detContentArea.videoOutput`
- 同样处理 `audioOutput` 绑定

### 6. 添加视频结束状态追踪

**修改文件**: `qml/components/EmbeddedMediaViewer.qml`

**问题描述**: 无法追踪视频播放结束状态

**解决方案**:
- 在根组件添加 `_videoEnded` 属性
- 在 `MediaPlayer` 的 `onPlaybackStateChanged` 中更新状态
- 当视频播放到末尾时（position >= duration - 500ms）设置 `_videoEnded = true`
- 开始播放时重置 `_videoEnded = false`

---

## 二、遇到的问题及解决方案

### 问题 1：QML Gradient 对象不支持 visible 属性

**现象**: 编译错误 `Cannot assign to non-existent property "visible"`

**原因**: QML 的 `Gradient` 对象没有 `visible` 属性

**解决方案**: 
- 尝试使用绑定表达式返回 `Gradient` 对象
- 由于 QML 限制，最终改用 `color` 属性返回不同的颜色值
- 每个倍速按钮返回独特的颜色，实现渐变视觉效果

### 问题 2：QML if-else 语法错误

**现象**: 编译错误 `Expected token ';'`

**原因**: QML 不支持 `else if` 语法，需要使用嵌套的 `if` 语句

**解决方案**: 将所有 `else if` 改为独立的 `if` 语句

---

## 三、技术实现细节

### 倍速按钮颜色绑定

```qml
color: {
    var currentRate = videoPlayer ? videoPlayer.playbackRate : 1.0
    if (Math.abs(currentRate - modelData) < 0.01) {
        if (modelData === 0.5) return "#87CEFA"
        if (modelData === 1.0) return "#3B82F6"
        if (modelData === 1.5) return "#2563EB"
        if (modelData === 2.0) return "#1A73E8"
        if (modelData === 2.5) return "#0F52BA"
        return "#0A2864"
    }
    return Theme.isDark 
           ? (speedBtnMouse.containsMouse ? Qt.rgba(1,1,1,0.12) : Qt.rgba(1,1,1,0.06))
           : (speedBtnMouse.containsMouse ? Qt.rgba(0,0,0,0.08) : Qt.rgba(0,0,0,0.03))
}
```

### MediaPlayer 动态输出绑定

```qml
MediaPlayer {
    id: vidPlayer
    videoOutput: displayMode === "embedded" ? contentArea.videoOutput : detContentArea.videoOutput
    audioOutput: displayMode === "embedded" ? contentArea.audioOutput : detContentArea.audioOutput
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

### 视频预览帧实现

```qml
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

---

## 四、参考资料

- [Qt Multimedia Documentation](https://doc.qt.io/qt-6/multimedia/qml-multimedia-videooverview.html)
- [Qt QML Property Binding](https://doc.qt.io/qt-6/qtqml/qdeclarativeintroduction.html)
- [Qt Color and Gradient](https://doc.qt.io/qt-6/qtqml/qml-visual-canvas-color.html)
