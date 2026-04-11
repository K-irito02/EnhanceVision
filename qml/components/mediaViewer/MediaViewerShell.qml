import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../styles"
import "../../controls"
import "."

Rectangle {
    id: root

    property var viewer: null
    property var videoPlayer: null
    property var playbackController: null
    property bool showDetachButton: false
    property bool showEmbedButton: false
    property bool showMinimizeButton: false
    property bool showFullscreenButton: false
    property bool fullscreenActive: false
    property bool showThumbnailBar: false
    property alias titleBarItem: titleBar
    property alias buttonRowItem: buttonRow
    property alias compareButtonItem: compareButton
    property alias videoOutput: canvas.videoOutput
    property alias audioOutput: canvas.audioOutput

    signal detachRequested()
    signal embedRequested()
    signal minimizeRequested()
    signal fullscreenRequested()
    signal closeRequested()

    color: Theme.colors.background

    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 44
        color: Theme.colors.titleBar

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.colors.titleBarBorder
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            spacing: 12

            Text {
                text: root.viewer && root.viewer.currentFile ? root.viewer.currentFile.fileName : qsTr("媒体查看器")
                color: Theme.colors.foreground
                font.pixelSize: 14
                font.weight: Font.Medium
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: root.viewer && root.viewer.mediaFiles.length > 0 ? (root.viewer.currentIndex + 1) + "/" + root.viewer.mediaFiles.length : ""
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter
            }

            Row {
                id: buttonRow
                spacing: 2
                Layout.alignment: Qt.AlignVCenter

                IconButton {
                    id: compareButton
                    visible: root.viewer && root.viewer._hasShaderOrOriginal
                    iconName: root.viewer && root.viewer.showOriginal ? "view-result" : "view-source"
                    iconSize: 16
                    btnSize: 32
                    tooltip: root.viewer && root.viewer.showOriginal ? qsTr("点击查看成果") : qsTr("点击查看源件")
                    iconColor: root.viewer && root.viewer.showOriginal ? Theme.colors.primary : Theme.colors.icon
                    onClicked: if (root.viewer) root.viewer.showOriginal = !root.viewer.showOriginal
                }

                IconButton {
                    visible: root.showDetachButton
                    iconName: "external-link"
                    iconSize: 16
                    btnSize: 32
                    tooltip: qsTr("独立窗口")
                    onClicked: root.detachRequested()
                }

                IconButton {
                    visible: root.showEmbedButton
                    iconName: "minimize-2"
                    iconSize: 16
                    btnSize: 32
                    tooltip: qsTr("嵌入")
                    onClicked: root.embedRequested()
                }

                IconButton {
                    visible: root.showMinimizeButton
                    iconName: "minus"
                    iconSize: 16
                    btnSize: 32
                    tooltip: qsTr("最小化")
                    onClicked: root.minimizeRequested()
                }

                IconButton {
                    visible: root.showFullscreenButton
                    iconName: root.fullscreenActive ? "minimize-2" : "maximize"
                    iconSize: 16
                    btnSize: 32
                    tooltip: root.fullscreenActive ? qsTr("退出全屏") : qsTr("全屏")
                    onClicked: root.fullscreenRequested()
                }

                IconButton {
                    iconName: "x"
                    iconSize: 16
                    btnSize: 32
                    danger: true
                    tooltip: qsTr("关闭")
                    onClicked: root.closeRequested()
                }
            }
        }
    }

    MediaViewerCanvas {
        id: canvas
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: controls.visible ? controls.top : (thumbnailBar.visible ? thumbnailBar.top : parent.bottom)
        viewer: root.viewer
        videoPlayer: root.videoPlayer
        playbackController: root.playbackController
    }

    MediaViewerControls {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: thumbnailBar.visible ? thumbnailBar.top : parent.bottom
        visible: root.viewer && root.viewer.isVideo
        viewer: root.viewer
        videoPlayer: root.videoPlayer
        audioOutput: canvas.audioOutput
        playbackController: root.playbackController
    }

    MediaViewerThumbnailBar {
        id: thumbnailBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.showThumbnailBar && root.viewer && root.viewer.mediaFiles.length > 1
        viewer: root.viewer
    }
}
