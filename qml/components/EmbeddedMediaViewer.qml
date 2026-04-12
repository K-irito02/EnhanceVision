import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtMultimedia
import EnhanceVision.Controllers
import EnhanceVision.Utils
import "../styles"
import "../controls"
import "../utils"
import "mediaViewer"
import "mediaViewer/MediaViewerHelpers.js" as MediaViewerHelpers

Item {
    id: root

    property var mediaFiles: []
    property int currentIndex: 0
    property string selectedFileId: ""
    property string selectedFilePath: ""
    property bool messageMode: false
    property bool showOriginal: false
    property string messageId: ""
    property string viewerId: "viewer-" + Date.now()
    property bool shaderEnabled: false
    property real shaderBrightness: 0.0
    property real shaderContrast: 1.0
    property real shaderSaturation: 1.0
    property real shaderHue: 0.0
    property real shaderSharpness: 0.0
    property real shaderBlur: 0.0
    property real shaderDenoise: 0.0
    property real shaderExposure: 0.0
    property real shaderGamma: 1.0
    property real shaderTemperature: 0.0
    property real shaderTint: 0.0
    property real shaderVignette: 0.0
    property real shaderHighlights: 0.0
    property real shaderShadows: 0.0
    property Item containerItem: null
    property string displayMode: "embedded"
    property bool isOpen: false
    property bool isMinimized: false
    property int aiScaleFactor: 1

    readonly property var currentFile: mediaFiles.length > 0 && currentIndex >= 0 && currentIndex < mediaFiles.length ? mediaFiles[currentIndex] : null
    readonly property bool isVideo: currentFile ? currentFile.mediaType === 1 : false
    readonly property bool _shouldApplyShader: shaderEnabled && !messageMode && !showOriginal
    readonly property bool _hasDistinctResult: MediaViewerHelpers.hasDistinctResult(currentFile)
    readonly property bool _hasShaderOrOriginal: MediaViewerHelpers.shouldShowCompare(shaderEnabled, messageMode, currentFile)
    readonly property string currentSource: MediaViewerHelpers.resolveCurrentSource(currentFile, messageMode, showOriginal)
    readonly property string _resultSource: MediaViewerHelpers.resultSource(currentFile)
    readonly property string _originalSource: MediaViewerHelpers.originalSource(currentFile)

    signal fileRemoved(string messageId, int fileIndex)
    signal minimizeRequested(string viewerId, string title, string thumbnail)
    signal closed()

    property var _mediaPlayer: vidPlayer
    property bool _isPlaying: false
    property real _savedW: 800
    property real _savedH: 600
    property bool _userDraggedPosition: false
    property real sharedVolume: SettingsController.volume / 100
    property real _volumeBeforeMute: 0.5

    onShowOriginalChanged: {
        if (isVideo && _mediaPlayer) {
            playbackController.prepareSourceResultSwitch()
        }
    }

    onCurrentFileChanged: {
        if (currentFile) {
            selectedFileId = currentFile.id || ""
            selectedFilePath = currentFile.filePath || ""
        }
    }

    onCurrentIndexChanged: {
        showOriginal = false
        playbackController._resetState()
    }

    VideoPlaybackController {
        id: playbackController
        mediaPlayer: root._mediaPlayer
        isVideo: root.isVideo
        currentSource: root.currentSource
        currentFile: root.currentFile
    }

    function computeIdealWindowSize(mediaW, mediaH, scaleFactor) {
        return MediaViewerHelpers.computeIdealWindowSize(mediaW, mediaH, scaleFactor, root.isVideo, root.messageMode || root.displayMode === "detached")
    }

    function _normalizeThumbnailKey(source) {
        return MediaViewerHelpers.normalizeThumbnailKey(source)
    }

    function _baseThumbnailKey(file) {
        return MediaViewerHelpers.baseThumbnailKey(file)
    }

    function _preferredThumbnailKey(file) {
        return MediaViewerHelpers.preferredThumbnailKey(file)
    }

    function _getSource(source) {
        return MediaViewerHelpers.resolveSource(source)
    }

    function _formatTime(ms) {
        return MediaViewerHelpers.formatTime(ms)
    }

    function _findMainPage() {
        var p = parent
        while (p) {
            if (p.globalZIndexCounter !== undefined) {
                return p
            }
            p = p.parent
        }
        return null
    }

    function _isOverContainer(globalX, globalY) {
        if (!containerItem) {
            return false
        }
        var pos = containerItem.mapToGlobal(0, 0)
        return globalX >= pos.x && globalX <= pos.x + containerItem.width && globalY >= pos.y && globalY <= pos.y + containerItem.height
    }

    function _getMinimizedDockHeight() {
        if (!containerItem) {
            return 0
        }
        for (var i = 0; i < containerItem.children.length; ++i) {
            var child = containerItem.children[i]
            if (child.objectName === "minimizedDock" || child.toString().indexOf("MinimizedWindowDock") >= 0) {
                return child.height
            }
        }
        return 0
    }

    function _syncSelectedFile() {
        if (currentFile) {
            selectedFileId = currentFile.id || ""
            selectedFilePath = currentFile.filePath || ""
        }
    }

    function syncMediaFiles(files, preferredFileId, preferredFilePath) {
        var nextFiles = files || []
        if (nextFiles.length === 0) {
            close()
            return
        }

        var nextIndex = MediaViewerHelpers.resolveNextIndex(mediaFiles, nextFiles, currentIndex,
                                                            preferredFileId, preferredFilePath,
                                                            selectedFileId, selectedFilePath)

        if (MediaViewerHelpers.isAppendOnlyUpdate(mediaFiles, nextFiles)) {
            var mergedFiles = mediaFiles.slice(0)
            for (var appendIndex = mediaFiles.length; appendIndex < nextFiles.length; ++appendIndex) {
                mergedFiles.push(nextFiles[appendIndex])
            }
            mediaFiles = mergedFiles
        } else {
            mediaFiles = nextFiles
        }

        currentIndex = Math.max(0, Math.min(nextIndex, mediaFiles.length - 1))
        _syncSelectedFile()
    }

    function openAt(index) {
        if (mediaFiles.length === 0) {
            return
        }

        currentIndex = Math.max(0, Math.min(index, mediaFiles.length - 1))
        _syncSelectedFile()
        showOriginal = false

        if (isMinimized) {
            isMinimized = false
            closed()
        }

        isOpen = true

        var file = currentFile
        if (file && file.resolution && file.resolution.width > 0 && file.resolution.height > 0) {
            var scaleFactor = messageMode ? 1 : Math.max(1, aiScaleFactor)
            var ideal = computeIdealWindowSize(file.resolution.width, file.resolution.height, scaleFactor)
            _savedW = ideal.width
            _savedH = ideal.height
        } else {
            _savedW = 800
            _savedH = 600
        }

        if (displayMode === "embedded") {
            var mainPage = _findMainPage()
            if (mainPage && mainPage.globalZIndexCounter !== undefined) {
                mainPage.globalZIndexCounter++
                root.z = mainPage.globalZIndexCounter
            } else {
                root.z = 1100 + Date.now() % 100
            }
            embeddedOverlay.visible = true
            embeddedOverlay.forceActiveFocus()
        } else {
            if (detachedWindow.visibility !== Window.FullScreen) {
                detachedWindow.width = _savedW
                detachedWindow.height = _savedH
                if (!_userDraggedPosition) {
                    detachedWindow.x = Math.floor((Screen.desktopAvailableWidth - _savedW) / 2)
                    detachedWindow.y = Math.floor((Screen.desktopAvailableHeight - _savedH) / 2)
                }
            }
            detachedWindow.show()
            detachedWindow.raise()
            detachedWindow.requestActivate()
        }
    }

    function close() {
        isOpen = false
        isMinimized = false
        embeddedOverlay.visible = false
        dragPreviewWindow.hide()
        detachedWindow.hide()
        root.z = messageMode ? 1000 : 1001
        playbackController._resetState()
        if (isVideo && _mediaPlayer) {
            _mediaPlayer.stop()
            _mediaPlayer.source = ""
        }
        mediaFiles = []
        currentIndex = -1
        selectedFileId = ""
        selectedFilePath = ""
        closed()
    }

    function minimize() {
        isMinimized = true
        if (displayMode === "embedded") {
            embeddedOverlay.visible = false
        } else {
            detachedWindow.hide()
        }
        minimizeRequested(viewerId, currentFile ? currentFile.fileName : "", currentFile ? (currentFile.thumbnail || "") : "")
    }

    function restore() {
        isMinimized = false
        if (displayMode === "embedded") {
            var mainPage = _findMainPage()
            if (mainPage && mainPage.globalZIndexCounter !== undefined) {
                mainPage.globalZIndexCounter++
                root.z = mainPage.globalZIndexCounter
            } else {
                root.z = 1100 + Date.now() % 100
            }
            embeddedOverlay.visible = true
            embeddedOverlay.forceActiveFocus()
        } else {
            detachedWindow.showNormal()
            detachedWindow.raise()
            detachedWindow.requestActivate()
        }
    }

    function switchToDetached(globalX, globalY) {
        if (displayMode === "detached") {
            return
        }

        displayMode = "detached"
        embeddedOverlay.visible = false

        if (_savedW === 800 && _savedH === 600 && currentFile && currentFile.resolution && currentFile.resolution.width > 0) {
            var scaleFactor = messageMode ? 1 : Math.max(1, aiScaleFactor)
            var ideal = computeIdealWindowSize(currentFile.resolution.width, currentFile.resolution.height, scaleFactor)
            _savedW = ideal.width
            _savedH = ideal.height
        }

        detachedWindow.width = _savedW
        detachedWindow.height = _savedH
        if (globalX !== undefined && globalY !== undefined) {
            detachedWindow.x = globalX
            detachedWindow.y = globalY
            _userDraggedPosition = true
        } else {
            detachedWindow.x = Math.floor((Screen.desktopAvailableWidth - _savedW) / 2)
            detachedWindow.y = Math.floor((Screen.desktopAvailableHeight - _savedH) / 2)
        }
        detachedWindow.show()
        detachedWindow.raise()
        detachedWindow.requestActivate()
    }

    function switchToEmbedded() {
        if (displayMode === "embedded") {
            return
        }

        _savedW = detachedWindow.width
        _savedH = detachedWindow.height
        displayMode = "embedded"
        detachedWindow.hide()

        var mainPage = _findMainPage()
        if (mainPage && mainPage.globalZIndexCounter !== undefined) {
            mainPage.globalZIndexCounter++
            root.z = mainPage.globalZIndexCounter
        }
        embeddedOverlay.visible = true
        embeddedOverlay.forceActiveFocus()
    }

    function _prevFile() {
        if (currentIndex > 0) {
            currentIndex--
            _syncSelectedFile()
        }
    }

    function _nextFile() {
        if (currentIndex < mediaFiles.length - 1) {
            currentIndex++
            _syncSelectedFile()
        }
    }

    function _toggleDetachedFullscreen() {
        if (detachedWindow.visibility === Window.FullScreen) {
            detachedWindow.showNormal()
            detachedWindow.x = detachedWindow._savedX
            detachedWindow.y = detachedWindow._savedY
            detachedWindow.width = detachedWindow._savedW
            detachedWindow.height = detachedWindow._savedH
        } else {
            winHelper.saveNormalGeometry()
            detachedWindow._savedX = detachedWindow.x
            detachedWindow._savedY = detachedWindow.y
            detachedWindow._savedW = detachedWindow.width
            detachedWindow._savedH = detachedWindow.height
            detachedWindow.showFullScreen()
        }
    }

    Window {
        id: dragPreviewWindow
        width: root._savedW
        height: root._savedH
        color: "transparent"
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        visible: false

        property bool _snapHint: false

        Rectangle {
            anchors.fill: parent
            color: Theme.colors.background
            opacity: 0.85
            radius: 8
            border.width: 2
            border.color: dragPreviewWindow._snapHint ? Theme.colors.primaryLight : Theme.colors.primary

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 44
                color: Theme.colors.titleBar
                radius: 8

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.colors.titleBarBorder
                }

                Text {
                    anchors.centerIn: parent
                    text: root.currentFile ? root.currentFile.fileName : qsTr("媒体查看器")
                    color: Theme.colors.foreground
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                }
            }

            Text {
                anchors.centerIn: parent
                text: dragPreviewWindow._snapHint ? qsTr("释放鼠标吸附到消息区域") : qsTr("释放鼠标以独立窗口打开")
                color: dragPreviewWindow._snapHint ? Theme.colors.primaryLight : Theme.colors.mutedForeground
                font.pixelSize: 14
                font.weight: Font.Medium
            }
        }
    }

    Rectangle {
        id: embeddedOverlay
        anchors.fill: parent
        anchors.bottomMargin: root._getMinimizedDockHeight()
        visible: false
        color: Theme.colors.background
        z: root.z

        MouseArea {
            anchors.fill: parent
            anchors.topMargin: embeddedShell.titleBarItem.height
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onClicked: function(mouse) { mouse.accepted = true }
            onDoubleClicked: function(mouse) { mouse.accepted = true }
            onPressAndHold: function(mouse) { mouse.accepted = true }
            onWheel: function(wheel) { wheel.accepted = true }
        }

        MediaViewerShell {
            id: embeddedShell
            anchors.fill: parent
            viewer: root
            videoPlayer: vidPlayer
            playbackController: playbackController
            showDetachButton: true
            showMinimizeButton: true
            showThumbnailBar: root.messageMode
            enableDragOut: true
            onDetachRequested: root.switchToDetached()
            onMinimizeRequested: root.minimize()
            onCloseRequested: root.close()
            onDragOutStarted: function(startX, startY) {
                dragPreviewWindow.x = startX
                dragPreviewWindow.y = startY
                dragPreviewWindow.width = root._savedW
                dragPreviewWindow.height = root._savedH
                dragPreviewWindow.show()
            }
            onDragOutPositionChanged: function(globalX, globalY) {
                dragPreviewWindow.x = globalX
                dragPreviewWindow.y = globalY
                var previewCenterX = dragPreviewWindow.x + dragPreviewWindow.width / 2
                var previewCenterY = dragPreviewWindow.y + 22
                dragPreviewWindow._snapHint = root._isOverContainer(previewCenterX, previewCenterY)
            }
            onDragOutFinished: function(finalX, finalY, shouldSnap) {
                shouldSnap = dragPreviewWindow._snapHint
                dragPreviewWindow.hide()

                if (shouldSnap) {
                    root.switchToEmbedded()
                } else {
                    root.switchToDetached(finalX, finalY)
                    Qt.callLater(function() {
                        winHelper.startSystemMove()
                    })
                }
            }
        }
    }

    Window {
        id: detachedWindow
        width: 800
        height: 600
        minimumWidth: 480
        minimumHeight: 360
        title: root.currentFile ? root.currentFile.fileName : qsTr("媒体查看器")
        color: Theme.colors.background
        flags: Qt.Window | Qt.FramelessWindowHint
        visible: false

        property bool _snapHint: false
        property real _savedX: 0
        property real _savedY: 0
        property real _savedW: 800
        property real _savedH: 600

        function updateExcludeRegions() {
            winHelper.clearExcludeRegions()
            if (detachedShell.buttonRowItem && detachedShell.buttonRowItem.width > 0) {
                var buttonRowPos = detachedShell.buttonRowItem.mapToItem(detachedShell.titleBarItem, 0, 0)
                winHelper.addExcludeRegion(buttonRowPos.x, buttonRowPos.y, detachedShell.buttonRowItem.width, detachedShell.buttonRowItem.height)
            }
            if (detachedShell.compareButtonItem && detachedShell.compareButtonItem.visible && detachedShell.compareButtonItem.width > 0) {
                var comparePos = detachedShell.compareButtonItem.mapToItem(detachedShell.titleBarItem, 0, 0)
                winHelper.addExcludeRegion(comparePos.x, comparePos.y, detachedShell.compareButtonItem.width, detachedShell.compareButtonItem.height)
            }
        }

        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)
        onVisibleChanged: if (visible) Qt.callLater(updateExcludeRegions)
        onClosing: function(close) {
            close.accepted = true
            root.close()
        }

        SubWindowHelper {
            id: winHelper
            Component.onCompleted: {
                winHelper.setWindow(detachedWindow)
                winHelper.setMinWidth(480)
                winHelper.setMinHeight(360)
                winHelper.setTitleBarHeight(44)
            }
        }

        MediaViewerShell {
            id: detachedShell
            anchors.fill: parent
            viewer: root
            videoPlayer: vidPlayer
            playbackController: playbackController
            showEmbedButton: true
            showMinimizeButton: true
            showFullscreenButton: true
            fullscreenActive: detachedWindow.visibility === Window.FullScreen
            showThumbnailBar: true
            onEmbedRequested: root.switchToEmbedded()
            onMinimizeRequested: root.minimize()
            onFullscreenRequested: root._toggleDetachedFullscreen()
            onCloseRequested: root.close()
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.colors.primary
            opacity: 0.3
            visible: detachedWindow._snapHint
            radius: 8

            Text {
                anchors.centerIn: parent
                text: qsTr("松开鼠标吸附到消息区域")
                color: Theme.colors.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
        }
    }

    MediaPlayer {
        id: vidPlayer
        videoOutput: root.displayMode === "embedded" ? embeddedShell.videoOutput : detachedShell.videoOutput
        audioOutput: root.displayMode === "embedded" ? embeddedShell.audioOutput : detachedShell.audioOutput

        function togglePlay() {
            if (playbackState === MediaPlayer.PlayingState) {
                pause()
            } else {
                play()
            }
        }

        onPlaybackStateChanged: root._isPlaying = playbackState === MediaPlayer.PlayingState
        onMediaStatusChanged: {
            playbackController.handleMediaStatusChanged(mediaStatus)
            if (mediaStatus === MediaPlayer.InvalidMedia) {
                playbackController.handleMediaError(error, errorString)
            }
        }
        onErrorChanged: {
            if (error !== MediaPlayer.NoError) {
                playbackController.handleMediaError(error, errorString)
            }
        }
    }

    Connections {
        target: detachedWindow
        function onXChanged() { detachedWindow._snapHint = root._isOverContainer(detachedWindow.x + detachedWindow.width / 2, detachedWindow.y + 22) }
        function onYChanged() { detachedWindow._snapHint = root._isOverContainer(detachedWindow.x + detachedWindow.width / 2, detachedWindow.y + 22) }
    }

    Connections {
        target: winHelper
        function onDraggingChanged() {
            if (!winHelper.isDragging && detachedWindow._snapHint) {
                root._userDraggedPosition = false
                root.switchToEmbedded()
            } else if (!winHelper.isDragging && !detachedWindow._snapHint) {
                root._userDraggedPosition = true
            }
        }
    }

    Connections {
        target: root
        function onCurrentSourceChanged() {
            if (root.isVideo && root.currentSource) {
                vidPlayer.source = root._getSource(root.currentSource)
                if (playbackController._switchMode !== 2) {
                    playbackController.handleFileOpen()
                }
            } else {
                vidPlayer.source = ""
                playbackController._resetState()
            }
        }

        function onIsVideoChanged() {
            if (!root.isVideo && vidPlayer.playbackState !== MediaPlayer.StoppedState) {
                vidPlayer.stop()
                root._isPlaying = false
            }
            playbackController._resetState()
        }
    }
}
