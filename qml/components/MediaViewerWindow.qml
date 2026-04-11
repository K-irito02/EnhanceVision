import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtMultimedia
import EnhanceVision.Controllers
import "../styles"
import "mediaViewer"
import "mediaViewer/MediaViewerHelpers.js" as MediaViewerHelpers

Window {
    id: root

    property var mediaFiles: []
    property int currentIndex: 0
    property string selectedFileId: ""
    property string selectedFilePath: ""
    property bool messageMode: false
    property bool showOriginal: false
    property string messageId: ""
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

    property var mediaPlayer: vidPlayer
    property real _savedX: 0
    property real _savedY: 0
    property real _savedW: 0
    property real _savedH: 0
    property bool _userDraggedPosition: false
    property real sharedVolume: SettingsController.volume / 100
    property real _volumeBeforeMute: 0.5

    width: 900
    height: 650
    minimumWidth: 480
    minimumHeight: 360
    title: currentFile ? currentFile.fileName : qsTr("媒体查看器")
    color: Theme.colors.background
    flags: Qt.Window | Qt.FramelessWindowHint

    onShowOriginalChanged: {
        if (isVideo && mediaPlayer) {
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
        mediaPlayer: root.mediaPlayer
        isVideo: root.isVideo
        currentSource: root.currentSource
        currentFile: root.currentFile
    }

    SubWindowHelper {
        id: windowHelper
        Component.onCompleted: {
            windowHelper.setWindow(root)
            windowHelper.setMinWidth(480)
            windowHelper.setMinHeight(360)
            windowHelper.setTitleBarHeight(44)
        }
        onDraggingChanged: {
            if (!isDragging) {
                root._userDraggedPosition = true
            }
        }
    }

    function computeIdealWindowSize(mediaW, mediaH, scaleFactor) {
        return MediaViewerHelpers.computeIdealWindowSize(mediaW, mediaH, scaleFactor, root.isVideo, true)
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

    function updateExcludeRegions() {
        windowHelper.clearExcludeRegions()
        if (shell.buttonRowItem && shell.buttonRowItem.width > 0) {
            var buttonRowPos = shell.buttonRowItem.mapToItem(shell.titleBarItem, 0, 0)
            windowHelper.addExcludeRegion(buttonRowPos.x, buttonRowPos.y, shell.buttonRowItem.width, shell.buttonRowItem.height)
        }
        if (shell.compareButtonItem && shell.compareButtonItem.visible && shell.compareButtonItem.width > 0) {
            var comparePos = shell.compareButtonItem.mapToItem(shell.titleBarItem, 0, 0)
            windowHelper.addExcludeRegion(comparePos.x, comparePos.y, shell.compareButtonItem.width, shell.compareButtonItem.height)
        }
    }

    function _toggleFullscreen() {
        if (root.visibility === Window.FullScreen) {
            root.showNormal()
            root.x = _savedX
            root.y = _savedY
            root.width = _savedW
            root.height = _savedH
        } else {
            _savedX = root.x
            _savedY = root.y
            _savedW = root.width
            _savedH = root.height
            root.showFullScreen()
        }
    }

    function _prevFile() {
        if (currentIndex > 0) {
            if (isVideo && mediaPlayer) {
                mediaPlayer.stop()
            }
            currentIndex--
            _syncSelectedFile()
        }
    }

    function _nextFile() {
        if (currentIndex < mediaFiles.length - 1) {
            if (isVideo && mediaPlayer) {
                mediaPlayer.stop()
            }
            currentIndex++
            _syncSelectedFile()
        }
    }

    function openAt(index) {
        if (mediaFiles.length === 0) {
            return
        }

        currentIndex = Math.max(0, Math.min(index, mediaFiles.length - 1))
        _syncSelectedFile()
        showOriginal = false

        if (root.visibility !== Window.FullScreen) {
            var file = currentFile
            if (file && file.resolution && file.resolution.width > 0 && file.resolution.height > 0) {
                var scaleFactor = root.messageMode ? 1 : Math.max(1, root.aiScaleFactor)
                var ideal = computeIdealWindowSize(file.resolution.width, file.resolution.height, scaleFactor)
                root.width = ideal.width
                root.height = ideal.height
            } else {
                root.width = 900
                root.height = 650
            }
            if (!_userDraggedPosition) {
                root.x = Math.floor((Screen.desktopAvailableWidth - root.width) / 2)
                root.y = Math.floor((Screen.desktopAvailableHeight - root.height) / 2)
            }
        }

        root.show()
        root.raise()
        root.requestActivate()
    }

    onWidthChanged: Qt.callLater(updateExcludeRegions)
    onHeightChanged: Qt.callLater(updateExcludeRegions)
    onVisibleChanged: if (visible) Qt.callLater(updateExcludeRegions)
    onClosing: {
        if (isVideo && mediaPlayer) {
            mediaPlayer.stop()
        }
        playbackController._resetState()
    }

    MediaViewerShell {
        id: shell
        anchors.fill: parent
        viewer: root
        videoPlayer: vidPlayer
        playbackController: playbackController
        showFullscreenButton: true
        fullscreenActive: root.visibility === Window.FullScreen
        showThumbnailBar: true
        onFullscreenRequested: root._toggleFullscreen()
        onCloseRequested: root.close()
    }

    MediaPlayer {
        id: vidPlayer
        videoOutput: shell.videoOutput
        audioOutput: shell.audioOutput

        function togglePlay() {
            if (playbackState === MediaPlayer.PlayingState) {
                pause()
            } else {
                play()
            }
        }

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
            }
            playbackController._resetState()
        }
    }
}
