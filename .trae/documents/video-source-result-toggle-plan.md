# 视频源件/成果切换按钮实施计划

## 需求概述

当多媒体文件为视频时，消息预览区域和消息展示区域的窗口顶部标题栏（嵌入式窗口和独立窗口）也需要有"源件/成果"按钮，并且可以正常在源件视频和成果视频间切换。

## 当前问题分析

### 1. EmbeddedMediaViewer.qml

**内嵌模式标题栏比较按钮**（第336-347行）：
- 当前条件：`visible: !root.isVideo && root._hasShaderOrOriginal`
- 问题：`!root.isVideo` 限制导致视频不显示按钮
- 按钮文字：`qsTr("效果")` / `qsTr("原图")`

**独立窗口模式标题栏比较按钮**（第463-474行）：
- 当前条件：`visible: !root.isVideo && root._hasShaderOrOriginal`
- 问题：同上
- 按钮文字：`qsTr("效果")` / `qsTr("原图")`

### 2. MediaViewerWindow.qml

**标题栏比较按钮**（第256-290行）：
- 当前条件：`visible: root.messageMode`
- 问题：没有限制视频类型，但按钮文字需要修改
- 按钮文字：`qsTr("查看修改")` / `qsTr("查看原图")`

### 3. 视频源切换逻辑

视频源切换逻辑已经正确实现：
- `currentSource` 属性已正确处理 `showOriginal` 和 `originalPath`
- 切换时视频播放器会自动加载新源

## 实施步骤

### 步骤 1：修改 EmbeddedMediaViewer.qml 内嵌模式按钮

**文件位置**：`qml/components/EmbeddedMediaViewer.qml`

**修改内容**（第336-347行）：

```qml
// 修改前
Rectangle {
    id: compareButton
    visible: !root.isVideo && root._hasShaderOrOriginal
    width: cmpRow.implicitWidth + 16; height: 28; radius: 6
    color: root.showOriginal ? Theme.colors.primary : Theme.colors.muted
    Layout.alignment: Qt.AlignVCenter
    Row { id: cmpRow; anchors.centerIn: parent; spacing: 6
        ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("eye"); iconSize: 14; color: root.showOriginal ? "#FFF" : Theme.colors.foreground }
        Text { anchors.verticalCenter: parent.verticalCenter; text: root.showOriginal ? qsTr("效果") : qsTr("原图"); color: root.showOriginal ? "#FFF" : Theme.colors.foreground; font.pixelSize: 12 }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.showOriginal = !root.showOriginal; z: 20 }
}

// 修改后
Rectangle {
    id: compareButton
    visible: root._hasShaderOrOriginal
    width: cmpRow.implicitWidth + 16; height: 28; radius: 6
    color: root.showOriginal ? Theme.colors.primary : Theme.colors.muted
    Layout.alignment: Qt.AlignVCenter
    Row { id: cmpRow; anchors.centerIn: parent; spacing: 6
        ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("eye"); iconSize: 14; color: root.showOriginal ? "#FFF" : Theme.colors.foreground }
        Text { anchors.verticalCenter: parent.verticalCenter; text: root.showOriginal ? qsTr("成果") : qsTr("源件"); color: root.showOriginal ? "#FFF" : Theme.colors.foreground; font.pixelSize: 12 }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.showOriginal = !root.showOriginal; z: 20 }
}
```

### 步骤 2：修改 EmbeddedMediaViewer.qml 独立窗口模式按钮

**文件位置**：`qml/components/EmbeddedMediaViewer.qml`

**修改内容**（第463-474行）：

```qml
// 修改前
Rectangle {
    id: detCompareButton
    visible: !root.isVideo && root._hasShaderOrOriginal
    width: cmpRow2.implicitWidth + 16; height: 28; radius: 6
    color: root.showOriginal ? Theme.colors.primary : Theme.colors.muted
    Layout.alignment: Qt.AlignVCenter
    Row { id: cmpRow2; anchors.centerIn: parent; spacing: 6
        ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("eye"); iconSize: 14; color: root.showOriginal ? "#FFF" : Theme.colors.foreground }
        Text { anchors.verticalCenter: parent.verticalCenter; text: root.showOriginal ? qsTr("效果") : qsTr("原图"); color: root.showOriginal ? "#FFF" : Theme.colors.foreground; font.pixelSize: 12 }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.showOriginal = !root.showOriginal; z: 20 }
}

// 修改后
Rectangle {
    id: detCompareButton
    visible: root._hasShaderOrOriginal
    width: cmpRow2.implicitWidth + 16; height: 28; radius: 6
    color: root.showOriginal ? Theme.colors.primary : Theme.colors.muted
    Layout.alignment: Qt.AlignVCenter
    Row { id: cmpRow2; anchors.centerIn: parent; spacing: 6
        ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("eye"); iconSize: 14; color: root.showOriginal ? "#FFF" : Theme.colors.foreground }
        Text { anchors.verticalCenter: parent.verticalCenter; text: root.showOriginal ? qsTr("成果") : qsTr("源件"); color: root.showOriginal ? "#FFF" : Theme.colors.foreground; font.pixelSize: 12 }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.showOriginal = !root.showOriginal; z: 20 }
}
```

### 步骤 3：修改 MediaViewerWindow.qml 标题栏按钮文字

**文件位置**：`qml/components/MediaViewerWindow.qml`

**修改内容**（第279行）：

```qml
// 修改前
text: showOriginal ? qsTr("查看修改") : qsTr("查看原图")

// 修改后
text: showOriginal ? qsTr("查看成果") : qsTr("查看源件")
```

## 修改汇总

| 文件 | 位置 | 修改内容 |
|------|------|----------|
| EmbeddedMediaViewer.qml | 第338行 | `visible: !root.isVideo && ...` → `visible: root._hasShaderOrOriginal` |
| EmbeddedMediaViewer.qml | 第344行 | `qsTr("效果")` → `qsTr("成果")`，`qsTr("原图")` → `qsTr("源件")` |
| EmbeddedMediaViewer.qml | 第465行 | `visible: !root.isVideo && ...` → `visible: root._hasShaderOrOriginal` |
| EmbeddedMediaViewer.qml | 第471行 | `qsTr("效果")` → `qsTr("成果")`，`qsTr("原图")` → `qsTr("源件")` |
| MediaViewerWindow.qml | 第279行 | `qsTr("查看修改")` → `qsTr("查看成果")`，`qsTr("查看原图")` → `qsTr("查看源件")` |

## 验证要点

1. **图片文件**：按钮显示"源件/成果"，点击可切换
2. **视频文件**：按钮显示"源件/成果"，点击可切换视频源
3. **嵌入式窗口**：视频和图片都显示切换按钮
4. **独立窗口**：视频和图片都显示切换按钮
5. **消息模式**：视频和图片都显示切换按钮

## 注意事项

- 视频切换时会自动停止当前播放并加载新源
- `currentSource` 属性已正确处理 `originalPath` 和 `filePath` 的切换逻辑
- 无需修改视频播放器的源加载逻辑
