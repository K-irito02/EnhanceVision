import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtMultimedia
import EnhanceVision.Controllers
import "../../styles"
import "../../controls"
import ".."

Item {
    id: root

    property var viewer: null
    property var videoPlayer: null
    property var playbackController: null
    property alias videoOutput: videoOutput
    property alias audioOutput: audioOutput

    clip: true

    Rectangle {
        anchors.fill: parent
        color: Theme.colors.overlay
        z: -1
    }

    property bool _navButtonsVisible: false
    property bool _prevBtnHovered: false
    property bool _nextBtnHovered: false

    function _showNavButtonsAndResetTimer() {
        _navButtonsVisible = true
        autoHideTimer.restart()
    }

    function _stopAutoHide() {
        autoHideTimer.stop()
    }

    function _startAutoHideIfNeeded() {
        if (!_prevBtnHovered && !_nextBtnHovered) {
            autoHideTimer.restart()
        }
    }

    Timer {
        id: autoHideTimer
        interval: 2000
        repeat: false
        onTriggered: {
            if (!root._prevBtnHovered && !root._nextBtnHovered) {
                root._navButtonsVisible = false
            }
        }
    }

    Image {
        id: imageShaderSource
        anchors.fill: parent
        visible: false
        source: root.viewer && !root.viewer.isVideo ? root.viewer._getSource(root.viewer.currentSource) : ""
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
    }

    FullShaderEffect {
        anchors.fill: parent
        source: imageShaderSource
        visible: root.viewer && !root.viewer.isVideo && root.viewer.currentSource !== "" && root.viewer.shaderEnabled && !root.viewer.messageMode && !root.viewer.showOriginal
        brightness: root.viewer ? root.viewer.shaderBrightness : 0.0
        contrast: root.viewer ? root.viewer.shaderContrast : 1.0
        saturation: root.viewer ? root.viewer.shaderSaturation : 1.0
        hue: root.viewer ? root.viewer.shaderHue : 0.0
        sharpness: root.viewer ? root.viewer.shaderSharpness : 0.0
        blurAmount: root.viewer ? root.viewer.shaderBlur : 0.0
        denoise: root.viewer ? root.viewer.shaderDenoise : 0.0
        exposure: root.viewer ? root.viewer.shaderExposure : 0.0
        gamma: root.viewer ? root.viewer.shaderGamma : 1.0
        temperature: root.viewer ? root.viewer.shaderTemperature : 0.0
        tint: root.viewer ? root.viewer.shaderTint : 0.0
        vignette: root.viewer ? root.viewer.shaderVignette : 0.0
        highlights: root.viewer ? root.viewer.shaderHighlights : 0.0
        shadows: root.viewer ? root.viewer.shaderShadows : 0.0
    }

    Loader {
        active: root.viewer && !root.viewer.isVideo && root.viewer.messageMode && root.viewer._hasDistinctResult
        anchors.fill: parent
        sourceComponent: Item {
            Image {
                anchors.fill: parent
                opacity: root.viewer.showOriginal ? 0.0 : 1.0
                source: root.viewer._getSource(root.viewer._resultSource)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true

                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
            }

            Image {
                anchors.fill: parent
                opacity: root.viewer.showOriginal ? 1.0 : 0.0
                source: root.viewer._getSource(root.viewer._originalSource)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true

                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
            }
        }
    }

    Image {
        anchors.fill: parent
        visible: root.viewer && !root.viewer.isVideo && !root.viewer.messageMode && root.viewer.currentSource !== "" && (root.viewer.showOriginal || !root.viewer.shaderEnabled)
        source: root.viewer && !root.viewer.isVideo && !root.viewer.messageMode && root.viewer.currentSource !== ""
                ? root.viewer._getSource(root.viewer.currentSource)
                : ""
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
    }

    Image {
        anchors.fill: parent
        visible: root.viewer && !root.viewer.isVideo && root.viewer.messageMode && !root.viewer._hasDistinctResult && root.viewer.currentSource !== ""
        source: root.viewer && !root.viewer.isVideo && root.viewer.messageMode && !root.viewer._hasDistinctResult && root.viewer.currentSource !== ""
                ? root.viewer._getSource(root.viewer.currentSource)
                : ""
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        enabled: root.viewer && !root.viewer.isVideo
        z: 10

        onPressed: function(mouse) { mouse.accepted = true }
        onReleased: function(mouse) { mouse.accepted = true }
        onClicked: function(mouse) { mouse.accepted = true }
        onDoubleClicked: function(mouse) { mouse.accepted = true }
        onPositionChanged: root._showNavButtonsAndResetTimer()
        onContainsMouseChanged: {
            if (containsMouse) {
                root._showNavButtonsAndResetTimer()
            } else {
                root._startAutoHideIfNeeded()
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: root.viewer && root.viewer.isVideo

        property bool _applyVideoShader: root.viewer && root.viewer.shaderEnabled && !root.viewer.showOriginal && !root.viewer.messageMode
        property bool _shouldShowPlayIcon: {
            if (!root.viewer || !root.viewer.isVideo || !root.videoPlayer) {
                return false
            }
            if (root.playbackController && root.playbackController.isLoading) {
                return false
            }
            if (root.videoPlayer.playbackState === MediaPlayer.PlayingState) {
                return false
            }
            if (SettingsController.videoAutoPlay && root.playbackController && root.playbackController._switchMode === 1) {
                return false
            }
            return true
        }

        ThumbnailStatusImage {
            anchors.fill: parent
            visible: root.viewer && root.viewer.isVideo && root.videoPlayer && root.videoPlayer.playbackState === MediaPlayer.StoppedState
            opacity: visible ? 1.0 : 0.0
            z: 1
            baseThumbnailId: root.viewer ? root.viewer._baseThumbnailKey(root.viewer.currentFile) : ""
            preferredThumbnailId: root.viewer ? root.viewer._preferredThumbnailKey(root.viewer.currentFile) : ""
            preferPreferredThumbnail: root.viewer && root.viewer.messageMode && root.viewer.currentFile && root.viewer.currentFile.status === 2 && preferredThumbnailId !== ""
            mediaType: 1
            requestedSourceSize: Qt.size(Math.max(256, Math.round(width)), Math.max(256, Math.round(height)))
            fillMode: Image.PreserveAspectFit
            backgroundColor: "transparent"
            showFailureBorder: true

            Behavior on opacity { NumberAnimation { duration: 200 } }
        }

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            z: 0
            visible: !parent._applyVideoShader
        }

        ShaderEffectSource {
            id: videoShaderSource
            sourceItem: videoOutput
            live: parent._applyVideoShader
            hideSource: parent._applyVideoShader
            visible: false
            textureSize: {
                var maxVideoSize = 1280
                var sourceWidth = videoOutput.sourceRect.width > 0 ? videoOutput.sourceRect.width : videoOutput.width
                var sourceHeight = videoOutput.sourceRect.height > 0 ? videoOutput.sourceRect.height : videoOutput.height
                if (sourceWidth <= 0 || sourceHeight <= 0) {
                    return Qt.size(512, 512)
                }
                var scale = Math.min(1.0, maxVideoSize / Math.max(sourceWidth, sourceHeight))
                return Qt.size(Math.floor(sourceWidth * scale), Math.floor(sourceHeight * scale))
            }
            recursive: false
            mipmap: false
            smooth: true
        }

        ShaderEffect {
            anchors.fill: parent
            visible: parent._applyVideoShader
            z: 0

            property var source: videoShaderSource
            property real brightness: root.viewer ? root.viewer.shaderBrightness : 0.0
            property real contrast: root.viewer ? root.viewer.shaderContrast : 1.0
            property real saturation: root.viewer ? root.viewer.shaderSaturation : 1.0
            property real hue: root.viewer ? root.viewer.shaderHue : 0.0
            property real sharpness: root.viewer ? root.viewer.shaderSharpness : 0.0
            property real blurAmount: root.viewer ? root.viewer.shaderBlur : 0.0
            property real denoise: root.viewer ? root.viewer.shaderDenoise : 0.0
            property real exposure: root.viewer ? root.viewer.shaderExposure : 0.0
            property real gamma: root.viewer ? root.viewer.shaderGamma : 1.0
            property real temperature: root.viewer ? root.viewer.shaderTemperature : 0.0
            property real tint: root.viewer ? root.viewer.shaderTint : 0.0
            property real vignette: root.viewer ? root.viewer.shaderVignette : 0.0
            property real highlights: root.viewer ? root.viewer.shaderHighlights : 0.0
            property real shadows: root.viewer ? root.viewer.shaderShadows : 0.0
            property size imgSize: Qt.size(videoOutput.sourceRect.width > 0 ? videoOutput.sourceRect.width : videoOutput.width,
                                           videoOutput.sourceRect.height > 0 ? videoOutput.sourceRect.height : videoOutput.height)

            vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
            fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
            supportsAtlasTextures: true
        }

        AudioOutput {
            id: audioOutput
            volume: root.viewer ? root.viewer.sharedVolume : 0.5
        }

        Rectangle {
            anchors.centerIn: parent
            width: 72
            height: 72
            radius: 36
            color: Theme.isDark ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(1, 1, 1, 0.9)
            visible: root.viewer && root.viewer.isVideo && root.videoPlayer && root.playbackController && root.playbackController.isLoading
            z: 11

            ColoredIcon {
                id: centerLoadingIcon
                anchors.centerIn: parent
                source: Theme.icon("loader")
                iconSize: 32
                color: Theme.colors.primary

                RotationAnimator {
                    target: centerLoadingIcon
                    running: root.playbackController && root.playbackController.isLoading
                    from: 0
                    to: 360
                    duration: 1000
                    loops: Animation.Infinite
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 72
            height: 72
            radius: 36
            color: Theme.isDark ? Qt.rgba(0, 0, 0, 0.6) : Qt.rgba(1, 1, 1, 0.9)
            visible: parent._shouldShowPlayIcon
            z: 10

            ColoredIcon {
                anchors.centerIn: parent
                source: Theme.icon("play")
                iconSize: 32
                color: Theme.colors.primary
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.videoPlayer) root.videoPlayer.togglePlay()
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            z: 2

            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onPositionChanged: root._showNavButtonsAndResetTimer()
            onContainsMouseChanged: {
                if (containsMouse) {
                    root._showNavButtonsAndResetTimer()
                } else {
                    root._startAutoHideIfNeeded()
                }
            }
            onClicked: function(mouse) {
                if (mouse.button === Qt.LeftButton && root.videoPlayer) {
                    root.videoPlayer.togglePlay()
                }
                mouse.accepted = true
            }
            onDoubleClicked: function(mouse) { mouse.accepted = true }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 44
        height: 44
        radius: 22
        visible: root.viewer && root.viewer.currentIndex > 0
        enabled: visible && root._navButtonsVisible
        opacity: root._navButtonsVisible ? 1.0 : 0.0
        z: 50

        property bool isHovered: prevMouse.containsMouse && enabled
        property bool isPressed: prevMouse.pressed && enabled

        color: {
            if (isPressed) return Qt.rgba(0.15, 0.15, 0.15, 0.75)
            if (isHovered) return Qt.rgba(0.2, 0.2, 0.2, 0.7)
            return Qt.rgba(0.25, 0.25, 0.25, 0.55)
        }
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, isHovered ? 0.25 : 0.15)
        scale: isPressed ? 0.9 : (isHovered ? 1.1 : 1.0)

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        ColoredIcon {
            anchors.centerIn: parent
            source: Theme.icon("chevron-left")
            iconSize: 22
            color: Theme.colors.textOnPrimary
        }

        MouseArea {
            id: prevMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: parent.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: if (root.viewer) root.viewer._prevFile()
            onEntered: {
                root._prevBtnHovered = true
                root._stopAutoHide()
            }
            onExited: {
                root._prevBtnHovered = false
                root._startAutoHideIfNeeded()
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 44
        height: 44
        radius: 22
        visible: root.viewer && root.viewer.currentIndex < root.viewer.mediaFiles.length - 1
        enabled: visible && root._navButtonsVisible
        opacity: root._navButtonsVisible ? 1.0 : 0.0
        z: 50

        property bool isHovered: nextMouse.containsMouse && enabled
        property bool isPressed: nextMouse.pressed && enabled

        color: {
            if (isPressed) return Qt.rgba(0.15, 0.15, 0.15, 0.75)
            if (isHovered) return Qt.rgba(0.2, 0.2, 0.2, 0.7)
            return Qt.rgba(0.25, 0.25, 0.25, 0.55)
        }
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, isHovered ? 0.25 : 0.15)
        scale: isPressed ? 0.9 : (isHovered ? 1.1 : 1.0)

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        ColoredIcon {
            anchors.centerIn: parent
            source: Theme.icon("chevron-right")
            iconSize: 22
            color: Theme.colors.textOnPrimary
        }

        MouseArea {
            id: nextMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: parent.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: if (root.viewer) root.viewer._nextFile()
            onEntered: {
                root._nextBtnHovered = true
                root._stopAutoHide()
            }
            onExited: {
                root._nextBtnHovered = false
                root._startAutoHideIfNeeded()
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        visible: !root.viewer || !root.viewer.currentFile

        ColoredIcon {
            Layout.alignment: Qt.AlignHCenter
            source: Theme.icon("image")
            iconSize: 64
            color: Theme.colors.mutedForeground
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("无媒体文件")
            color: Theme.colors.mutedForeground
            font.pixelSize: 16
        }
    }
}
