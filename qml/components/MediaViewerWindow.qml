import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia
import "../styles"
import "../controls"

/**
 * @brief 媒体查看器独立窗口
 *
 * 功能：
 * - 独立窗口，支持自由拖拽和伸缩
 * - 全屏模式（按 Esc 或按钮恢复）
 * - 左右按钮浏览媒体文件
 * - 关闭按钮
 * - 视频播放控制（播放/暂停、快进/快退、倍速、进度条、音量）
 * - 原效果对比按钮（消息模式下）
 * - 图片自适应缩放显示
 */
Window {
    id: root

    // ========== 属性定义 ==========
    /** @brief 媒体文件列表 [{filePath, fileName, mediaType, thumbnail, resultPath}] */
    property var mediaFiles: []

    /** @brief 当前显示的文件索引 */
    property int currentIndex: 0

    /** @brief 是否为消息模式（启用原图对比功能） */
    property bool messageMode: false

    /** @brief 是否显示原始效果（用于对比） */
    property bool showOriginal: false

    // ========== 内部属性 ==========
    property var currentFile: mediaFiles.length > 0 && currentIndex >= 0 && currentIndex < mediaFiles.length
                              ? mediaFiles[currentIndex] : null
    property bool isVideo: currentFile ? (currentFile.mediaType === 1) : false
    property string currentSource: {
        if (!currentFile) return ""
        if (messageMode && !showOriginal && currentFile.resultPath && currentFile.resultPath !== "")
            return currentFile.resultPath
        return currentFile.filePath || ""
    }

    // 全屏前保存的窗口几何
    property real _savedX: 0
    property real _savedY: 0
    property real _savedW: 0
    property real _savedH: 0
    property bool _wasMaximized: false

    // ========== 窗口设置 ==========
    width: 900
    height: 650
    minimumWidth: 480
    minimumHeight: 360
    title: currentFile ? currentFile.fileName : qsTr("媒体查看器")
    color: "#0A0E1A"
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
           | Qt.WindowMinMaxButtonsHint

    // ========== 快捷键 ==========
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (root.visibility === Window.FullScreen) {
                _exitFullscreen()
            } else {
                root.close()
            }
        }
    }
    Shortcut { sequence: "Left";  onActivated: _prevFile() }
    Shortcut { sequence: "Right"; onActivated: _nextFile() }
    Shortcut { sequence: "Space"; onActivated: if (isVideo) videoPlayer.togglePlay() }
    Shortcut { sequence: "F";     onActivated: _toggleFullscreen() }

    // ========== 功能方法 ==========
    function _toggleFullscreen() {
        if (root.visibility === Window.FullScreen) {
            _exitFullscreen()
        } else {
            _savedX = root.x; _savedY = root.y
            _savedW = root.width; _savedH = root.height
            root.showFullScreen()
        }
    }

    function _exitFullscreen() {
        root.showNormal()
        root.x = _savedX; root.y = _savedY
        root.width = _savedW; root.height = _savedH
    }

    function _prevFile() {
        if (currentIndex > 0) {
            _stopCurrentMedia()
            currentIndex--
        }
    }

    function _nextFile() {
        if (currentIndex < mediaFiles.length - 1) {
            _stopCurrentMedia()
            currentIndex++
        }
    }

    function _stopCurrentMedia() {
        if (isVideo) {
            mediaPlayer.stop()
        }
        showOriginal = false
    }

    function openAt(index) {
        currentIndex = index
        showOriginal = false
        root.show()
        root.raise()
        root.requestActivate()
    }

    onCurrentIndexChanged: showOriginal = false

    // ========== 主布局 ==========
    Rectangle {
        anchors.fill: parent
        color: "#0A0E1A"

        // ========== 顶部工具栏 ==========
        Rectangle {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44
            color: Qt.rgba(0, 0, 0, 0.5)
            z: 100

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // 文件名
                Text {
                    text: currentFile ? currentFile.fileName : ""
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                // 文件计数
                Text {
                    text: mediaFiles.length > 0 ? "%1 / %2".arg(currentIndex + 1).arg(mediaFiles.length) : ""
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 12
                }

                // 原图对比按钮（消息模式）
                Rectangle {
                    visible: root.messageMode
                    width: compareRow.implicitWidth + 16
                    height: 30
                    radius: 6
                    color: showOriginal ? "#1E56D0" : Qt.rgba(1, 1, 1, 0.12)
                    border.width: showOriginal ? 0 : 1
                    border.color: Qt.rgba(1, 1, 1, 0.2)

                    Row {
                        id: compareRow
                        anchors.centerIn: parent
                        spacing: 6

                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("eye")
                            iconSize: 14
                            color: "#FFFFFF"
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: showOriginal ? qsTr("查看处理后") : qsTr("查看原图")
                            color: "#FFFFFF"
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: showOriginal = !showOriginal
                    }
                }

                // 全屏按钮
                IconButton {
                    iconName: root.visibility === Window.FullScreen ? "minimize-2" : "maximize"
                    iconSize: 16; btnSize: 32
                    iconColor: "#FFFFFF"
                    iconHoverColor: "#A0C4FF"
                    onClicked: _toggleFullscreen()
                    tooltip: root.visibility === Window.FullScreen ? qsTr("退出全屏") : qsTr("全屏")
                }

                // 关闭按钮
                IconButton {
                    iconName: "x"
                    iconSize: 16; btnSize: 32
                    iconColor: "#FFFFFF"
                    iconHoverColor: "#FF6B6B"
                    onClicked: root.close()
                    tooltip: qsTr("关闭")
                }
            }
        }

        // ========== 内容区域 ==========
        Item {
            id: contentArea
            anchors.top: topBar.bottom
            anchors.bottom: isVideo ? videoControls.top : bottomBar.top
            anchors.left: parent.left
            anchors.right: parent.right

            // 图片显示
            Image {
                id: imageView
                anchors.fill: parent
                anchors.margins: 20
                visible: !isVideo && currentSource !== ""
                source: {
                    if (isVideo || !currentSource) return ""
                    var src = currentSource
                    if (src.startsWith("file:///") || src.startsWith("qrc:/")) return src
                    return "file:///" + src
                }
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true
            }

            // 视频显示
            Item {
                id: videoContainer
                anchors.fill: parent
                anchors.margins: 20
                visible: isVideo

                MediaPlayer {
                    id: mediaPlayer
                    source: {
                        if (!isVideo || !currentSource) return ""
                        var src = currentSource
                        if (src.startsWith("file:///") || src.startsWith("qrc:/")) return src
                        return "file:///" + src
                    }
                    videoOutput: videoOutput
                    audioOutput: audioOutput

                    function togglePlay() {
                        if (playbackState === MediaPlayer.PlayingState)
                            pause()
                        else
                            play()
                    }

                    onPositionChanged: {
                        if (!videoProgressSlider.pressed) {
                            videoProgressSlider.value = position
                        }
                    }
                }

                AudioOutput {
                    id: audioOutput
                    volume: volumeSlider.value
                }

                VideoOutput {
                    id: videoOutput
                    anchors.fill: parent
                }

                // 视频中央播放/暂停覆盖
                Rectangle {
                    anchors.centerIn: parent
                    width: 64; height: 64; radius: 32
                    color: Qt.rgba(0, 0, 0, 0.5)
                    visible: isVideo && mediaPlayer.playbackState !== MediaPlayer.PlayingState
                    opacity: videoCenterMouse.containsMouse ? 1.0 : 0.7

                    ColoredIcon {
                        anchors.centerIn: parent
                        source: Theme.icon("play")
                        iconSize: 28
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        id: videoCenterMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mediaPlayer.togglePlay()
                    }

                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }

            // 占位
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: !currentFile

                ColoredIcon {
                    Layout.alignment: Qt.AlignHCenter
                    source: Theme.icon("image")
                    iconSize: 48
                    color: Qt.rgba(1, 1, 1, 0.3)
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("无媒体文件")
                    color: Qt.rgba(1, 1, 1, 0.4)
                    font.pixelSize: 14
                }
            }
        }

        // ========== 左右导航按钮 ==========
        // 左侧上一张
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: contentArea.verticalCenter
            width: 40; height: 40; radius: 20
            color: prevMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.08)
            visible: currentIndex > 0
            z: 50

            ColoredIcon {
                anchors.centerIn: parent
                source: Theme.icon("chevron-left")
                iconSize: 20
                color: "#FFFFFF"
            }

            MouseArea {
                id: prevMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: _prevFile()
            }

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // 右侧下一张
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: contentArea.verticalCenter
            width: 40; height: 40; radius: 20
            color: nextMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.08)
            visible: currentIndex < mediaFiles.length - 1
            z: 50

            ColoredIcon {
                anchors.centerIn: parent
                source: Theme.icon("chevron-right")
                iconSize: 20
                color: "#FFFFFF"
            }

            MouseArea {
                id: nextMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: _nextFile()
            }

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // ========== 视频控制栏 ==========
        Rectangle {
            id: videoControls
            anchors.bottom: bottomBar.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: isVideo ? 90 : 0
            visible: isVideo
            color: Qt.rgba(0, 0, 0, 0.6)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                // 进度条
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: _formatTime(mediaPlayer.position)
                        color: Qt.rgba(1, 1, 1, 0.8)
                        font.pixelSize: 11
                        font.family: "Consolas"
                    }

                    Slider {
                        id: videoProgressSlider
                        Layout.fillWidth: true
                        from: 0
                        to: mediaPlayer.duration > 0 ? mediaPlayer.duration : 1
                        value: mediaPlayer.position

                        onMoved: {
                            mediaPlayer.position = value
                        }

                        background: Rectangle {
                            x: videoProgressSlider.leftPadding
                            y: videoProgressSlider.topPadding + videoProgressSlider.availableHeight / 2 - height / 2
                            width: videoProgressSlider.availableWidth
                            height: 4; radius: 2
                            color: Qt.rgba(1, 1, 1, 0.2)

                            Rectangle {
                                width: videoProgressSlider.visualPosition * parent.width
                                height: parent.height; radius: 2
                                color: "#1E56D0"
                            }
                        }

                        handle: Rectangle {
                            x: videoProgressSlider.leftPadding + videoProgressSlider.visualPosition * (videoProgressSlider.availableWidth - width)
                            y: videoProgressSlider.topPadding + videoProgressSlider.availableHeight / 2 - height / 2
                            width: 14; height: 14; radius: 7
                            color: videoProgressSlider.pressed ? "#5B8DEF" : "#FFFFFF"
                            border.width: 2; border.color: "#1E56D0"
                            visible: videoProgressSlider.hovered || videoProgressSlider.pressed
                        }
                    }

                    Text {
                        text: _formatTime(mediaPlayer.duration)
                        color: Qt.rgba(1, 1, 1, 0.8)
                        font.pixelSize: 11
                        font.family: "Consolas"
                    }
                }

                // 控制按钮行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // 后退
                    IconButton {
                        iconName: "skip-back"
                        iconSize: 16; btnSize: 32
                        iconColor: "#FFFFFF"; iconHoverColor: "#A0C4FF"
                        tooltip: qsTr("快退 10 秒")
                        onClicked: mediaPlayer.position = Math.max(0, mediaPlayer.position - 10000)
                    }

                    // 播放/暂停
                    Rectangle {
                        width: 36; height: 36; radius: 18
                        color: playPauseMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1)

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: mediaPlayer.playbackState === MediaPlayer.PlayingState
                                    ? Theme.icon("pause") : Theme.icon("play")
                            iconSize: 18
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: playPauseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: mediaPlayer.togglePlay()
                        }

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    // 快进
                    IconButton {
                        iconName: "skip-forward"
                        iconSize: 16; btnSize: 32
                        iconColor: "#FFFFFF"; iconHoverColor: "#A0C4FF"
                        tooltip: qsTr("快进 10 秒")
                        onClicked: mediaPlayer.position = Math.min(mediaPlayer.duration, mediaPlayer.position + 10000)
                    }

                    // 分隔
                    Item { width: 8 }

                    // 倍速选择
                    Rectangle {
                        width: speedText.implicitWidth + 16
                        height: 28; radius: 6
                        color: speedMouse.containsMouse ? Qt.rgba(1,1,1,0.15) : Qt.rgba(1,1,1,0.08)
                        border.width: 1; border.color: Qt.rgba(1,1,1,0.15)

                        Text {
                            id: speedText
                            anchors.centerIn: parent
                            text: mediaPlayer.playbackRate + "x"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        MouseArea {
                            id: speedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: speedMenu.open()
                        }

                        Menu {
                            id: speedMenu
                            y: -implicitHeight - 4

                            background: Rectangle {
                                color: "#1A2040"
                                border.width: 1; border.color: Qt.rgba(1,1,1,0.15)
                                radius: 6
                            }

                            Repeater {
                                model: [0.5, 1.0, 1.5, 2.0, 3.0]
                                MenuItem {
                                    required property real modelData
                                    text: modelData + "x"
                                    onTriggered: mediaPlayer.playbackRate = modelData

                                    background: Rectangle {
                                        color: parent.highlighted ? Qt.rgba(1,1,1,0.1) : "transparent"
                                        radius: 4
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: mediaPlayer.playbackRate === modelData ? "#5B8DEF" : "#FFFFFF"
                                        font.pixelSize: 13
                                        font.weight: mediaPlayer.playbackRate === modelData ? Font.Bold : Font.Normal
                                        leftPadding: 8
                                    }
                                }
                            }
                        }

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    Item { Layout.fillWidth: true }

                    // 音量控制
                    Row {
                        spacing: 6

                        IconButton {
                            iconName: audioOutput.volume > 0 ? "volume-2" : "volume-x"
                            iconSize: 16; btnSize: 28
                            iconColor: "#FFFFFF"; iconHoverColor: "#A0C4FF"
                            tooltip: qsTr("静音")
                            onClicked: audioOutput.volume = audioOutput.volume > 0 ? 0 : 0.7
                        }

                        Slider {
                            id: volumeSlider
                            width: 80
                            from: 0; to: 1
                            value: 0.7

                            background: Rectangle {
                                x: volumeSlider.leftPadding
                                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                                width: volumeSlider.availableWidth
                                height: 3; radius: 1.5
                                color: Qt.rgba(1, 1, 1, 0.2)

                                Rectangle {
                                    width: volumeSlider.visualPosition * parent.width
                                    height: parent.height; radius: 1.5
                                    color: "#1E56D0"
                                }
                            }

                            handle: Rectangle {
                                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                                width: 12; height: 12; radius: 6
                                color: "#FFFFFF"
                                visible: volumeSlider.hovered || volumeSlider.pressed
                            }
                        }
                    }
                }
            }
        }

        // ========== 底部缩略图导航栏 ==========
        Rectangle {
            id: bottomBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: mediaFiles.length > 1 ? 60 : 0
            visible: mediaFiles.length > 1
            color: Qt.rgba(0, 0, 0, 0.5)

            ListView {
                id: bottomThumbList
                anchors.fill: parent
                anchors.margins: 6
                orientation: ListView.Horizontal
                spacing: 6
                clip: true
                model: mediaFiles.length
                currentIndex: root.currentIndex

                delegate: Rectangle {
                    required property int index
                    width: 48; height: 48
                    radius: 6
                    color: index === root.currentIndex ? "transparent" : Qt.rgba(1,1,1,0.05)
                    border.width: index === root.currentIndex ? 2 : 0
                    border.color: "#1E56D0"
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: {
                            var f = root.mediaFiles[index]
                            if (!f) return ""
                            if (f.thumbnail && f.thumbnail !== "") return f.thumbnail
                            if (f.filePath) return "image://thumbnail/" + f.filePath
                            return ""
                        }
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        sourceSize: Qt.size(96, 96)
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            _stopCurrentMedia()
                            root.currentIndex = index
                        }
                    }
                }

                // 滚轮水平滚动
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    onWheel: function(wheel) {
                        var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                        bottomThumbList.contentX = Math.max(0,
                            Math.min(bottomThumbList.contentX - delta,
                                     bottomThumbList.contentWidth - bottomThumbList.width))
                    }
                }
            }
        }
    }

    // ========== 工具方法 ==========
    function _formatTime(ms) {
        var totalSec = Math.floor(ms / 1000)
        var h = Math.floor(totalSec / 3600)
        var m = Math.floor((totalSec % 3600) / 60)
        var s = totalSec % 60
        if (h > 0)
            return "%1:%2:%3".arg(h).arg(String(m).padStart(2, '0')).arg(String(s).padStart(2, '0'))
        return "%1:%2".arg(String(m).padStart(2, '0')).arg(String(s).padStart(2, '0'))
    }

    // 关闭时停止播放
    onClosing: {
        if (isVideo) mediaPlayer.stop()
    }
}
