# 视频查看器源件/成果切换与音量控制修复

## 概述

修复视频查看器中"源件/成果"切换按钮在视频模式下不显示的问题，以及独立窗口模式下音量控制不工作的问题，并添加音量记忆机制。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `qml/components/EmbeddedMediaViewer.qml` | 移除视频切换按钮的 isVideo 限制；修复独立窗口音量控制；添加音量记忆机制 |
| `qml/components/MediaViewerWindow.qml` | 修改按钮文字为"查看成果/查看源件" |

### 2. 实现的功能特性

- ✅ 视频文件显示"源件/成果"切换按钮
- ✅ 嵌入式窗口和独立窗口都支持视频切换
- ✅ 独立窗口模式下音量控制正常工作
- ✅ 嵌入式窗口和独立窗口之间音量同步记忆

---

## 二、遇到的问题及解决方案

### 问题 1：视频模式下不显示"源件/成果"切换按钮

**现象**：当多媒体文件为视频时，消息预览区域和消息展示区域的窗口顶部标题栏没有"源件/成果"按钮。

**原因**：`EmbeddedMediaViewer.qml` 中比较按钮的 `visible` 属性有 `!root.isVideo` 条件限制，导致视频不显示按钮。

**解决方案**：
移除 `!root.isVideo` 条件限制，改为 `visible: root._hasShaderOrOriginal`

```qml
// 修改前
visible: !root.isVideo && root._hasShaderOrOriginal

// 修改后
visible: root._hasShaderOrOriginal
```

### 问题 2：独立窗口模式下音量控制不起作用

**现象**：当窗口切换到独立窗口模式时，音量滑块和静音按钮无法控制音量。

**原因**：`VideoControlBar` 组件中的音量控制硬编码引用了 `contentArea.audioOutput`（内嵌模式的音频输出），但独立窗口模式应该引用 `detContentArea.audioOutput`。

**解决方案**：
1. 给 `VideoControlBar` 组件添加 `audioOutput` 属性
2. 在两个 `VideoControlBar` 实例中分别传入正确的 audioOutput

```qml
// VideoControlBar 组件添加属性
property var audioOutput

// 内嵌模式
VideoControlBar {
    audioOutput: contentArea.audioOutput
}

// 独立窗口模式
VideoControlBar {
    audioOutput: detContentArea.audioOutput
}
```

### 问题 3：嵌入式窗口和独立窗口之间音量不能相互继承

**现象**：切换窗口模式后，音量设置丢失，没有记忆机制。

**原因**：两个 `AudioOutput` 组件各自独立设置音量，没有共享状态。

**解决方案**：
在 root 组件添加 `sharedVolume` 共享属性，两个 `AudioOutput` 都绑定到它：

```qml
// 添加共享音量属性
property real sharedVolume: 0.8

// AudioOutput 绑定到共享属性
AudioOutput { id: ao; volume: root.sharedVolume }

// 音量控制修改共享属性
onClicked: root.sharedVolume = root.sharedVolume > 0 ? 0 : 0.7
Slider { value: root.sharedVolume; onMoved: root.sharedVolume = value }
```

---

## 三、技术实现细节

### 按钮文字修改

将所有"原图/效果"按钮文字统一改为"源件/成果"：

| 组件 | 修改前 | 修改后 |
|------|--------|--------|
| EmbeddedMediaViewer 内嵌模式 | `qsTr("效果")` / `qsTr("原图")` | `qsTr("成果")` / `qsTr("源件")` |
| EmbeddedMediaViewer 独立窗口 | `qsTr("效果")` / `qsTr("原图")` | `qsTr("成果")` / `qsTr("源件")` |
| MediaViewerWindow | `qsTr("查看修改")` / `qsTr("查看原图")` | `qsTr("查看成果")` / `qsTr("查看源件")` |

### 音量同步机制

使用 QML 属性绑定实现音量同步：

```
┌─────────────────────────────────────────────────────┐
│                    root 组件                         │
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │         sharedVolume: 0.8                    │   │
│  └─────────────────────────────────────────────┘   │
│              ↑                        ↑             │
│         绑定到                    绑定到            │
│              │                        │             │
│  ┌───────────┴───────────┐  ┌────────┴──────────┐  │
│  │  contentArea.         │  │  detContentArea.  │  │
│  │  audioOutput.volume   │  │  audioOutput.     │  │
│  │                       │  │  volume           │  │
│  │  (内嵌模式)            │  │  (独立窗口模式)    │  │
│  └───────────────────────┘  └───────────────────┘  │
│                                                     │
│  ┌───────────────────────────────────────────────┐ │
│  │              VideoControlBar                   │ │
│  │  静音按钮/音量滑块 → 修改 root.sharedVolume    │ │
│  └───────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

---

## 四、验证要点

1. **图片文件**：按钮显示"源件/成果"，点击可切换
2. **视频文件**：按钮显示"源件/成果"，点击可切换视频源
3. **嵌入式窗口**：视频和图片都显示切换按钮
4. **独立窗口**：视频和图片都显示切换按钮，音量控制正常
5. **音量记忆**：切换窗口模式后音量保持一致
