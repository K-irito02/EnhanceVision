import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects
import QtMultimedia
import EnhanceVision.Controllers
import "../styles"
import "../controls"
import EnhanceVision.Utils

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
 * - 自定义标题栏
 * - 删除文件功能（底部缩略图悬停显示删除按钮）
 */
Window {
    id: root

    property var mediaFiles: []
    property int currentIndex: 0
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

    property var currentFile: mediaFiles.length > 0 && currentIndex >= 0 && currentIndex < mediaFiles.length
                              ? mediaFiles[currentIndex] : null
    property bool isVideo: currentFile ? (currentFile.mediaType === 1) : false
    
    // 是否应用 shader 效果
    // - 预览区（messageMode=false）：应用 shader（实时预览效果）
    // - 消息区（messageMode=true）：不应用 shader（图片已处理或查看原图对比）
    property bool _shouldApplyShader: {
        if (!shaderEnabled) return false
        // 消息模式下永远不应用 shader：
        // - 默认显示已处理图片（shader 已烘焙）
        // - 查看原图时只显示原图（不应用 shader，用于对比）
        if (messageMode) return false
        return true
    }
    
    property string currentSource: {
        if (!currentFile) return ""
        
        if (messageMode) {
            if (showOriginal) {
                return currentFile.originalPath || currentFile.filePath || ""
            }
            if (currentFile.resultPath && currentFile.resultPath !== "") {
                return currentFile.resultPath
            }
            return currentFile.filePath || currentFile.originalPath || ""
        } else {
            return currentFile.filePath || currentFile.originalPath || ""
        }
    }
    // 预加载路径：始终准备好原图和结果图，用于无闪烁切换
    readonly property string _resultSource: currentFile ? (currentFile.resultPath && currentFile.resultPath !== "" ? currentFile.resultPath : (currentFile.filePath || "")) : ""
    readonly property string _originalSource: currentFile ? (currentFile.originalPath && currentFile.originalPath !== "" ? currentFile.originalPath : (currentFile.filePath || "")) : ""
    readonly property bool _hasDistinctResult: currentFile && currentFile.resultPath && currentFile.originalPath && currentFile.resultPath !== currentFile.originalPath
    function _getSource(s) { return !s ? "" : (s.startsWith("file:///") || s.startsWith("qrc:/") ? s : "file:///" + s) }

    property real _savedX: 0
    property real _savedY: 0
    property real _savedW: 0
    property real _savedH: 0
    property bool _wasMaximized: false
    property bool _userDraggedPosition: false  // 用户是否手动拖动过窗口位置
    property var mediaPlayer: null
    property real _volumeBeforeMute: 0.5

    // ========== 导航按钮状态管理 ==========
    property bool navButtonsVisible: false
    property bool prevBtnHovered: false
    property bool nextBtnHovered: false

    signal fileRemoved(string messageId, int fileIndex)

    // 2秒后自动隐藏定时器
    Timer {
        id: autoHideTimer
        interval: 2000
        repeat: false
        onTriggered: {
            // 只要鼠标不在按钮上就隐藏
            if (!prevBtnHovered && !nextBtnHovered) {
                navButtonsVisible = false
            }
        }
    }
    
    // 显示导航按钮并重置定时器
    function showNavButtonsAndResetTimer() {
        navButtonsVisible = true
        autoHideTimer.restart()
    }
    
    // 停止自动隐藏（当悬停在按钮上时）
    function stopAutoHide() {
        autoHideTimer.stop()
    }
    
    // 启动自动隐藏（当离开按钮时）
    function startAutoHideIfNeeded() {
        if (!prevBtnHovered && !nextBtnHovered) {
            autoHideTimer.restart()
        }
    }

    width: 900
    height: 650
    minimumWidth: 480
    minimumHeight: 360
    title: currentFile ? currentFile.fileName : qsTr("媒体查看器")
    color: Theme.colors.background
    flags: Qt.Window | Qt.FramelessWindowHint

    // ========== 窗口尺寸智能调整 ==========
    // 根据媒体文件分辨率和 AI 放大倍数智能设置初始窗口尺寸
    // 规则：
    //   1. 基于媒体原始分辨率 × AI 放大倍数得到「目标内容尺寸」
    //   2. 加上 UI 装饰（标题栏 44px、控制栏高度）得到「目标窗口尺寸」
    //   3. 限制在屏幕工作区的 80% 以内
    //   4. 不小于最小可用尺寸（480×360）
    function computeIdealWindowSize(mediaW, mediaH, scaleFactor) {
        var sf = Math.max(1, scaleFactor || 1)
        // 防御：媒体尺寸无效时使用合理默认值
        var cw = (mediaW > 0 ? mediaW : 1280) * sf
        var ch = (mediaH > 0 ? mediaH : 720)  * sf

        // UI 装饰高度：标题栏(44) + 视频控制栏(90) + 缩略图栏(60)
        var decorH = 44
        if (isVideo) decorH += 90
        if (mediaFiles.length > 1) decorH += 60
        var decorW = 16

        var targetW = cw + decorW
        var targetH = ch + decorH

        // 屏幕工作区 80% 上限
        var maxW = Math.max(480, Math.floor(Screen.desktopAvailableWidth  * 0.80))
        var maxH = Math.max(360, Math.floor(Screen.desktopAvailableHeight * 0.80))

        // 等比缩放，保持宽高比不失真
        var scaleToFit = Math.min(1.0, Math.min(maxW / targetW, maxH / targetH))
        targetW = Math.floor(targetW * scaleToFit)
        targetH = Math.floor(targetH * scaleToFit)

        // 最小保障（确保控制按钮可点击）
        targetW = Math.max(480, targetW)
        targetH = Math.max(360, targetH)

        return Qt.size(targetW, targetH)
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
            // 用户拖动结束后标记位置已手动调整
            if (!isDragging) {
                root._userDraggedPosition = true
            }
        }
    }

    function updateExcludeRegions() {
        windowHelper.clearExcludeRegions()
        var buttonsRow = titleBarButtonsRow
        if (buttonsRow && buttonsRow.width > 0) {
            var globalPos = buttonsRow.mapToItem(titleBar, 0, 0)
            windowHelper.addExcludeRegion(globalPos.x, globalPos.y, buttonsRow.width, buttonsRow.height)
        }
        var compareBtn = compareButton
        if (compareBtn && compareBtn.visible && compareBtn.width > 0) {
            var globalPos2 = compareBtn.mapToItem(titleBar, 0, 0)
            windowHelper.addExcludeRegion(globalPos2.x, globalPos2.y, compareBtn.width, compareBtn.height)
        }
    }

    onWidthChanged: Qt.callLater(updateExcludeRegions)
    onHeightChanged: Qt.callLater(updateExcludeRegions)
    onVisibleChanged: {
        if (visible) {
            Qt.callLater(updateExcludeRegions)
        }
    }

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

    property bool _videoEnded: false

    function _stopCurrentMedia() {
        if (isVideo && mediaPlayer) {
            mediaPlayer.stop()
        }
        showOriginal = false
        _videoEnded = false
    }

    // AI 放大倍数属性：由外部（如 AIParamsPanel）在推理参数更新时设置。
    // 用于窗口初始尺寸计算，避免超分结果图过小或过大。
    // 默认值 1 = 不放大（浏览模式或 Shader 模式），超分模式由调用方注入。
    property int aiScaleFactor: 1

    function openAt(index) {
        currentIndex = index
        showOriginal = false

        // ── 窗口尺寸智能调整 ───────────────────────────────────────
        // 仅在窗口非全屏状态下调整（全屏时保持全屏）
        if (root.visibility !== Window.FullScreen) {
            var file = mediaFiles.length > 0 && index >= 0 && index < mediaFiles.length
                       ? mediaFiles[index] : null
            if (file && file.resolution && file.resolution.width > 0 && file.resolution.height > 0) {
                // 使用 AI 放大倍数感知窗口尺寸：
                // - messageMode（查看结果）：结果图已是 scaleFactor 倍，此处传 1 避免重复放大
                // - 预览模式（实时查看）：传入 aiScaleFactor 预估输出尺寸
                var sf = root.messageMode ? 1 : Math.max(1, root.aiScaleFactor)
                var ideal = computeIdealWindowSize(file.resolution.width, file.resolution.height, sf)
                root.width  = ideal.width
                root.height = ideal.height
            } else {
                // 无法获取分辨率时使用合理默认值
                root.width  = 900
                root.height = 650
            }
            // 仅在用户未手动拖动过位置时才居中，否则保持拖动后的位置
            if (!_userDraggedPosition) {
                root.x = Math.floor((Screen.desktopAvailableWidth  - root.width)  / 2)
                root.y = Math.floor((Screen.desktopAvailableHeight - root.height) / 2)
            }
        }

        root.show()
        root.raise()
        root.requestActivate()
    }

    onCurrentIndexChanged: showOriginal = false

    Rectangle {
        anchors.fill: parent
        color: Theme.colors.background
        radius: 8

        Rectangle {
            id: titleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44
            color: Theme.colors.titleBar
            z: 100

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.colors.titleBarBorder
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: currentFile ? currentFile.fileName : ""
                    color: Theme.colors.foreground
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: mediaFiles.length > 0 ? "%1 / %2".arg(currentIndex + 1).arg(mediaFiles.length) : ""
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 12
                }

                Rectangle {
                    id: compareButton
                    visible: root.messageMode && root.currentFile && root.currentFile.resultPath && root.currentFile.originalPath && root.currentFile.resultPath !== root.currentFile.originalPath
                    width: compareRow.implicitWidth + 16
                    height: 28
                    radius: 6
                    color: showOriginal ? Theme.colors.primary : Theme.colors.card
                    border.width: showOriginal ? 0 : 1
                    border.color: Theme.colors.border

                    Row {
                        id: compareRow
                        anchors.centerIn: parent
                        spacing: 6

                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("eye")
                            iconSize: 14
                            color: Theme.colors.foreground
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: showOriginal ? qsTr("查看成果") : qsTr("查看源件")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: showOriginal = !showOriginal
                    }
                }

                Row {
                    id: titleBarButtonsRow
                    spacing: 2

                    IconButton {
                        iconName: root.visibility === Window.FullScreen ? "minimize-2" : "maximize"
                        iconSize: 16; btnSize: 32
                        onClicked: _toggleFullscreen()
                        tooltip: root.visibility === Window.FullScreen ? qsTr("退出全屏") : qsTr("全屏")
                    }

                    IconButton {
                        iconName: "x"
                        iconSize: 16; btnSize: 32
                        danger: true
                        onClicked: root.close()
                        tooltip: qsTr("关闭")
                    }
                }
            }
        }

        Item {
            id: contentArea
            anchors.top: titleBar.bottom
            anchors.bottom: isVideo ? videoControls.top : bottomBar.top
            anchors.left: parent.left
            anchors.right: parent.right
            clip: true  // 防止超分后大图溢出窗口边界

            Image {
                id: imageViewSource
                anchors.fill: parent
                anchors.margins: 8
                visible: false
                source: {
                    if (isVideo || !currentSource) return ""
                    var src = currentSource
                    if (src.startsWith("file:///") || src.startsWith("qrc:/") || src.startsWith("demo://")) return src
                    return "file:///" + src
                }
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true
            }

            // Shader 效果层：anchors.fill parent（而非 imageViewSource），
            // 避免因 imageViewSource 隐藏时尺寸计算异常导致的溢出
            FullShaderEffect {
                id: imageViewWithShader
                anchors.fill: parent
                anchors.margins: 8
                source: imageViewSource
                visible: !isVideo && currentSource !== "" && _shouldApplyShader
                
                brightness: shaderBrightness
                contrast: shaderContrast
                saturation: shaderSaturation
                hue: shaderHue
                sharpness: shaderSharpness
                blurAmount: shaderBlur
                denoise: shaderDenoise
                exposure: shaderExposure
                gamma: shaderGamma
                temperature: shaderTemperature
                tint: shaderTint
                vignette: shaderVignette
                highlights: shaderHighlights
                shadows: shaderShadows
                
                MouseArea {
                    id: shaderImageClickArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    acceptedButtons: Qt.LeftButton

                    onContainsMouseChanged: {
                        if (containsMouse) {
                            root.showNavButtonsAndResetTimer()
                        } else {
                            root.startAutoHideIfNeeded()
                        }
                    }
                    
                    onPositionChanged: {
                        root.showNavButtonsAndResetTimer()
                    }

                    onDoubleClicked: {
                        _toggleFullscreen()
                    }
                }
            }

            // ── 无闪烁源件/结果对比：双图预加载 + opacity 切换 ──────────
            // 结果图层（消息模式默认显示）
            Image {
                id: resultImageLayer
                anchors.fill: parent
                anchors.margins: 8
                visible: !isVideo && messageMode && _hasDistinctResult
                opacity: showOriginal ? 0.0 : 1.0
                source: !isVideo && messageMode ? _getSource(_resultSource) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true; smooth: true; mipmap: true
                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
            }
            // 原图层（showOriginal 时显示）
            Image {
                id: originalImageLayer
                anchors.fill: parent
                anchors.margins: 8
                visible: !isVideo && messageMode && _hasDistinctResult
                opacity: showOriginal ? 1.0 : 0.0
                source: !isVideo && messageMode && _hasDistinctResult ? _getSource(_originalSource) : ""
                fillMode: Image.PreserveAspectFit
                asynchronous: true; smooth: true; mipmap: true
                Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
            }
            // 普通图像层（非消息模式 或 无对比结果时）
            Image {
                id: imageView
                anchors.fill: parent
                anchors.margins: 8
                visible: !isVideo && currentSource !== "" && !_shouldApplyShader && !(messageMode && _hasDistinctResult)
                source: {
                    if (isVideo || !currentSource) return ""
                    var src = currentSource
                    if (src.startsWith("file:///") || src.startsWith("qrc:/") || src.startsWith("demo://")) return src
                    return "file:///" + src
                }
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                mipmap: true

                MouseArea {
                    id: imageClickArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    acceptedButtons: Qt.LeftButton

                    onContainsMouseChanged: {
                        if (containsMouse) {
                            root.showNavButtonsAndResetTimer()
                        } else {
                            root.startAutoHideIfNeeded()
                        }
                    }
                    
                    onPositionChanged: {
                        root.showNavButtonsAndResetTimer()
                    }

                    onDoubleClicked: {
                        _toggleFullscreen()
                    }
                }
            }

            Item {
                id: videoContainer
                anchors.fill: parent
                anchors.margins: 8
                visible: isVideo
                clip: true  // 防止视频帧溢出容器

                // 是否应用视频Shader效果（消息模式下也需要应用）
                property bool _applyVideoShader: shaderEnabled && !showOriginal

                Image {
                    id: videoPreviewFrame
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    mipmap: true
                    visible: isVideo && mediaPlayer && (mediaPlayer.playbackState === MediaPlayer.StoppedState || root._videoEnded)
                    opacity: visible ? 1.0 : 0.0
                    z: 1
                    
                    source: {
                        if (!isVideo || !currentSource || currentSource === "") return ""
                        
                        // 消息模式下，优先使用处理后的缩略图
                        if (messageMode && currentFile && currentFile.status === 2) {
                            if (currentFile.processedThumbnailId && currentFile.processedThumbnailId !== "") {
                                return "image://thumbnail/" + currentFile.processedThumbnailId
                            }
                        }
                        
                        var src = currentSource
                        if (src.startsWith("file:///")) {
                            src = src.substring(8)
                        }
                        return "image://thumbnail/" + src
                    }
                    
                    Rectangle {
                        anchors.fill: parent
                        color: Theme.colors.card
                        visible: parent.status === Image.Loading
                        
                        BusyIndicator {
                            anchors.centerIn: parent
                            running: parent.visible
                            implicitWidth: 48
                            implicitHeight: 48
                        }
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

                // VideoOutput接收视频流
                VideoOutput {
                    id: videoOutput
                    anchors.fill: parent
                    z: 0
                    // 当应用Shader时隐藏原始输出
                    visible: !videoContainer._applyVideoShader
                }
                
                // ShaderEffectSource捕获VideoOutput内容
                ShaderEffectSource {
                    id: videoShaderSource
                    sourceItem: videoOutput
                    sourceRect: Qt.rect(0, 0, 0, 0)
                    live: true
                    hideSource: videoContainer._applyVideoShader
                    visible: false
                }
                
                // 应用Shader效果的视频显示
                ShaderEffect {
                    id: videoShaderEffect
                    anchors.fill: parent
                    visible: videoContainer._applyVideoShader
                    z: 0
                    
                    property var source: videoShaderSource
                    property real brightness: shaderBrightness
                    property real contrast: shaderContrast
                    property real saturation: shaderSaturation
                    property real hue: shaderHue
                    property real sharpness: shaderSharpness
                    property real blurAmount: shaderBlur
                    property real denoise: shaderDenoise
                    property real exposure: shaderExposure
                    property real gamma: shaderGamma
                    property real temperature: shaderTemperature
                    property real tint: shaderTint
                    property real vignette: shaderVignette
                    property real highlights: shaderHighlights
                    property real shadows: shaderShadows
                    property size imgSize: Qt.size(videoOutput.sourceRect.width > 0 ? videoOutput.sourceRect.width : videoOutput.width,
                                                   videoOutput.sourceRect.height > 0 ? videoOutput.sourceRect.height : videoOutput.height)
                    
                    vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
                    fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
                    
                    supportsAtlasTextures: true
                }

                AudioOutput {
                    id: audioOutput
                    volume: volumeSlider.value
                }

                MediaPlayer {
                    id: videoPlayer
                    videoOutput: videoOutput
                    audioOutput: audioOutput
                    playbackRate: 1.0

                    function togglePlay() {
                        if (playbackState === MediaPlayer.PlayingState)
                            pause()
                        else
                            play()
                    }

                    onPositionChanged: function(position) {
                        if (!videoProgressSlider.pressed) {
                            videoProgressSlider.value = position
                        }
                    }

                    onPlaybackStateChanged: {
                        if (playbackState === MediaPlayer.StoppedState) {
                            if (position > 0 && duration > 0 && position >= duration - 500) {
                                root._videoEnded = true
                            }
                        } else if (playbackState === MediaPlayer.PlayingState) {
                            root._videoEnded = false
                        }
                    }

                    Component.onCompleted: root.mediaPlayer = this
                }

                Connections {
                    target: root
                    function onCurrentSourceChanged() {
                        if (isVideo && currentSource && currentSource !== "") {
                            var src = currentSource
                            if (!src.startsWith("file:///") && !src.startsWith("qrc:/")) {
                                src = "file:///" + src
                            }
                            videoPlayer.source = src
                        } else {
                            videoPlayer.source = ""
                        }
                    }
                    function onIsVideoChanged() {
                        if (!isVideo) {
                            videoPlayer.stop()
                            videoPlayer.source = ""
                        }
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 64; height: 64; radius: 32
                    color: Theme.isDark ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(255, 255, 255, 0.8)
                    visible: isVideo && mediaPlayer && mediaPlayer.playbackState !== MediaPlayer.PlayingState
                    opacity: videoCenterMouse.containsMouse ? 1.0 : 0.7
                    z: 10

                    ColoredIcon {
                        anchors.centerIn: parent
                        source: Theme.icon("play")
                        iconSize: 28
                        color: Theme.colors.mediaControlIcon
                    }

                    MouseArea {
                        id: videoCenterMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (mediaPlayer) mediaPlayer.togglePlay()
                        onContainsMouseChanged: {
                            if (containsMouse) {
                                root.hoverActive = true
                                hideButtonsTimer.stop()
                            }
                        }
                    }

                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }

                MouseArea {
                    id: videoClickArea
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    acceptedButtons: Qt.LeftButton
                    z: 3

                    onContainsMouseChanged: {
                        root._mouseInContentArea = containsMouse
                        if (containsMouse) {
                            root.hoverActive = true
                            hideButtonsTimer.stop()
                        } else if (!root._buttonsHovered) {
                            hideButtonsTimer.restart()
                        }
                    }
                    
                    onPositionChanged: {
                        root.hoverActive = true
                        hideButtonsTimer.restart()
                    }

                    onClicked: function(mouse) {
                        if (mediaPlayer) {
                            mediaPlayer.togglePlay()
                        }
                        mouse.accepted = false
                    }

                    onDoubleClicked: {
                        _toggleFullscreen()
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: !currentFile

                ColoredIcon {
                    Layout.alignment: Qt.AlignHCenter
                    source: Theme.icon("image")
                    iconSize: 48
                    color: Theme.colors.mutedForeground
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("无媒体文件")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 14
                }
            }
        }

        Rectangle {
            id: prevButton
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: contentArea.verticalCenter
            width: 44; height: 44; radius: 22
            visible: currentIndex > 0
            opacity: navButtonsVisible ? 1.0 : 0.0
            z: 50
            
            property bool isHovered: prevMouse.containsMouse
            property bool isPressed: prevMouse.pressed
            
            // 玻璃拟态背景
            color: {
                if (isPressed) return Qt.rgba(0.15, 0.15, 0.15, 0.75)
                if (isHovered) return Qt.rgba(0.2, 0.2, 0.2, 0.7)
                return Qt.rgba(0.25, 0.25, 0.25, 0.55)
            }
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, isHovered ? 0.25 : 0.15)
            
            scale: isPressed ? 0.9 : (isHovered ? 1.1 : 1.0)
            
            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                cursorShape: Qt.PointingHandCursor
                onClicked: _prevFile()
                onEntered: {
                    root.prevBtnHovered = true
                    root.stopAutoHide()
                }
                onExited: {
                    root.prevBtnHovered = false
                    root.startAutoHideIfNeeded()
                }
            }
        }

        Rectangle {
            id: nextButton
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: contentArea.verticalCenter
            width: 44; height: 44; radius: 22
            visible: currentIndex < mediaFiles.length - 1
            opacity: navButtonsVisible ? 1.0 : 0.0
            z: 50
            
            property bool isHovered: nextMouse.containsMouse
            property bool isPressed: nextMouse.pressed
            
            // 玻璃拟态背景
            color: {
                if (isPressed) return Qt.rgba(0.15, 0.15, 0.15, 0.75)
                if (isHovered) return Qt.rgba(0.2, 0.2, 0.2, 0.7)
                return Qt.rgba(0.25, 0.25, 0.25, 0.55)
            }
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, isHovered ? 0.25 : 0.15)
            
            scale: isPressed ? 0.9 : (isHovered ? 1.1 : 1.0)
            
            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
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
                cursorShape: Qt.PointingHandCursor
                onClicked: _nextFile()
                onEntered: {
                    root.nextBtnHovered = true
                    root.stopAutoHide()
                }
                onExited: {
                    root.nextBtnHovered = false
                    root.startAutoHideIfNeeded()
                }
            }
        }

        Rectangle {
            id: videoControls
            anchors.bottom: bottomBar.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: isVideo ? 90 : 0
            visible: isVideo
            color: Theme.colors.mediaControlBg

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.mediaControlBorder
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: _formatTime(mediaPlayer ? mediaPlayer.position : 0)
                        color: Theme.colors.mediaControlTextMuted
                        font.pixelSize: 11
                        font.family: "Consolas"
                    }

                    Slider {
                        id: videoProgressSlider
                        Layout.fillWidth: true
                        from: 0
                        to: mediaPlayer && mediaPlayer.duration > 0 ? mediaPlayer.duration : 1
                        value: mediaPlayer ? mediaPlayer.position : 0

                        onMoved: {
                            if (mediaPlayer) mediaPlayer.position = value
                        }

                        background: Rectangle {
                            x: videoProgressSlider.leftPadding
                            y: videoProgressSlider.topPadding + videoProgressSlider.availableHeight / 2 - height / 2
                            width: videoProgressSlider.availableWidth
                            height: 4; radius: 2
                            color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0, 0, 0, 0.15)

                            Rectangle {
                                width: videoProgressSlider.visualPosition * parent.width
                                height: parent.height; radius: 2
                                color: Theme.colors.primary
                            }
                        }

                        handle: Rectangle {
                            x: videoProgressSlider.leftPadding + videoProgressSlider.visualPosition * (videoProgressSlider.availableWidth - width)
                            y: videoProgressSlider.topPadding + videoProgressSlider.availableHeight / 2 - height / 2
                            width: 14; height: 14; radius: 7
                            color: videoProgressSlider.pressed ? Theme.colors.primaryLight : Theme.colors.card
                            border.width: 2; border.color: Theme.colors.primary
                            visible: videoProgressSlider.hovered || videoProgressSlider.pressed
                        }
                    }

                    Text {
                        text: _formatTime(mediaPlayer ? mediaPlayer.duration : 0)
                        color: Theme.colors.mediaControlTextMuted
                        font.pixelSize: 11
                        font.family: "Consolas"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    IconButton {
                        iconName: "skip-back"
                        iconSize: 16; btnSize: 32
                        iconColor: Theme.colors.mediaControlIcon
                        iconHoverColor: Theme.colors.mediaControlIconHover
                        tooltip: qsTr("快退 10 秒")
                        onClicked: if (mediaPlayer) mediaPlayer.position = Math.max(0, mediaPlayer.position - 10000)
                    }

                    Rectangle {
                        width: 36; height: 36; radius: 18
                        color: Theme.isDark 
                               ? (playPauseMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1))
                               : (playPauseMouse.containsMouse ? Qt.rgba(0,0,0,0.1) : Qt.rgba(0,0,0,0.05))

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: mediaPlayer && mediaPlayer.playbackState === MediaPlayer.PlayingState
                                    ? Theme.icon("pause") : Theme.icon("play")
                            iconSize: 18
                            color: Theme.colors.mediaControlIcon
                        }

                        MouseArea {
                            id: playPauseMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (mediaPlayer) mediaPlayer.togglePlay()
                        }

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    IconButton {
                        iconName: "skip-forward"
                        iconSize: 16; btnSize: 32
                        iconColor: Theme.colors.mediaControlIcon
                        iconHoverColor: Theme.colors.mediaControlIconHover
                        tooltip: qsTr("快进 10 秒")
                        onClicked: if (mediaPlayer) mediaPlayer.position = Math.min(mediaPlayer.duration, mediaPlayer.position + 10000)
                    }

                    Item { width: 8 }

                    Row {
                        spacing: 4

                        Repeater {
                            model: [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]
                            Rectangle {
                                id: speedBtnRect
                                required property real modelData
                                required property int index
                                width: speedBtnText.implicitWidth + 12
                                height: 26
                                radius: 5
                                
                                // 计算渐变颜色：从浅蓝到深蓝
                                function getSpeedColor() {
                                    var colors = ["#7CB9E8", "#5B9BD5", "#3A7FC2", "#1E5FAF", "#0D4A9C", "#002FA7"]
                                    return colors[index]
                                }
                                
                                property bool isSelected: {
                                    var currentRate = mediaPlayer ? mediaPlayer.playbackRate : 1.0
                                    return Math.abs(currentRate - modelData) < 0.01
                                }
                                
                                color: {
                                    if (isSelected) {
                                        return getSpeedColor()
                                    }
                                    if (speedBtnMouse.pressed) {
                                        return Theme.isDark ? Qt.rgba(1,1,1,0.18) : Qt.rgba(0,0,0,0.12)
                                    }
                                    return Theme.isDark 
                                           ? (speedBtnMouse.containsMouse ? Qt.rgba(1,1,1,0.12) : Qt.rgba(1,1,1,0.06))
                                           : (speedBtnMouse.containsMouse ? Qt.rgba(0,0,0,0.08) : Qt.rgba(0,0,0,0.04))
                                }
                                border.width: 1
                                border.color: isSelected ? "transparent" : Theme.colors.mediaControlBorder

                                Text {
                                    id: speedBtnText
                                    anchors.centerIn: parent
                                    text: speedBtnRect.modelData.toFixed(1) + "x"
                                    color: speedBtnRect.isSelected ? "#FFFFFF" : Theme.colors.mediaControlTextMuted
                                    font.pixelSize: 11
                                    font.weight: speedBtnRect.isSelected ? Font.Bold : Font.Normal
                                }

                                MouseArea {
                                    id: speedBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (mediaPlayer) mediaPlayer.playbackRate = speedBtnRect.modelData
                                }

                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Row {
                        spacing: 6

                        IconButton {
                            iconName: audioOutput.volume > 0 ? "volume-2" : "volume-x"
                            iconSize: 16; btnSize: 28
                            iconColor: Theme.colors.mediaControlIcon
                            iconHoverColor: Theme.colors.mediaControlIconHover
                            tooltip: qsTr("静音")
                            onClicked: {
                                if (audioOutput.volume > 0) {
                                    root._volumeBeforeMute = audioOutput.volume
                                    audioOutput.volume = 0
                                    SettingsController.volume = 0
                                } else {
                                    audioOutput.volume = root._volumeBeforeMute
                                    SettingsController.volume = Math.round(root._volumeBeforeMute * 100)
                                }
                            }
                        }

                        Slider {
                            id: volumeSlider
                            width: 80
                            from: 0; to: 1
                            value: SettingsController.volume / 100

                            onMoved: {
                                SettingsController.volume = Math.round(value * 100)
                            }

                            background: Rectangle {
                                x: volumeSlider.leftPadding
                                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                                width: volumeSlider.availableWidth
                                height: 3; radius: 1.5
                                color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0, 0, 0, 0.15)

                                Rectangle {
                                    width: volumeSlider.visualPosition * parent.width
                                    height: parent.height; radius: 1.5
                                    color: Theme.colors.primary
                                }
                            }

                            handle: Rectangle {
                                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                                width: 12; height: 12; radius: 6
                                color: Theme.colors.card
                                visible: volumeSlider.hovered || volumeSlider.pressed
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: bottomBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: mediaFiles.length > 1 ? 60 : 0
            visible: mediaFiles.length > 1
            color: Theme.colors.mediaControlBgLight

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.mediaControlBorder
            }

            ListView {
                id: bottomThumbList
                anchors.fill: parent
                anchors.margins: 6
                orientation: ListView.Horizontal
                spacing: 6
                clip: true
                model: mediaFiles.length
                currentIndex: root.currentIndex

                delegate: Item {
                    required property int index
                    width: 48; height: 48
                    property bool showDeleteBtn: bottomThumbMouse.containsMouse || bottomDeleteBtnMouse.containsMouse

                    Rectangle {
                        id: bottomHoverBorder
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.width: 2
                        border.color: index === root.currentIndex ? Theme.colors.primary : "transparent"
                        z: 5
                    }

                    Rectangle {
                        id: bottomThumbRect
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: 6
                        color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                        clip: true

                        Image {
                            id: bottomThumbImage
                            anchors.fill: parent
                            source: {
                                var f = root.mediaFiles[index]
                                if (!f) return ""
                                
                                if (root.messageMode && f.status === 2) {
                                    if (f.processedThumbnailId && f.processedThumbnailId !== "") {
                                        return "image://thumbnail/" + f.processedThumbnailId
                                    }
                                    if (f.resultPath && f.resultPath !== "") {
                                        return "image://thumbnail/" + f.resultPath
                                    }
                                }
                                
                                if (f.thumbnail && f.thumbnail !== "") return f.thumbnail
                                if (f.filePath) return "image://thumbnail/" + f.filePath
                                return ""
                            }
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            smooth: true
                            sourceSize: Qt.size(96, 96)
                            visible: status === Image.Ready
                            layer.enabled: true
                            layer.samples: 4
                            layer.effect: MultiEffect {
                                maskEnabled: true
                                maskThresholdMin: 0.5
                                maskSpreadAtMin: 1.0
                                maskSource: bottomThumbMask
                            }
                        }

                        Item {
                            id: bottomThumbMask
                            visible: false
                            layer.enabled: true
                            layer.samples: 4
                            width: bottomThumbRect.width
                            height: bottomThumbRect.height

                            Rectangle {
                                anchors.fill: parent
                                radius: 6
                                color: "white"
                                antialiasing: true
                            }
                        }

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: {
                                var f = root.mediaFiles[index]
                                if (!f) return Theme.icon("image")
                                return f.mediaType === 1 ? Theme.icon("video") : Theme.icon("image")
                            }
                            iconSize: 16
                            color: Theme.colors.mutedForeground
                            visible: bottomThumbImage.status !== Image.Ready
                        }

                        Rectangle {
                            visible: {
                                var f = root.mediaFiles[index]
                                return f && f.mediaType === 1
                            }
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.margins: 3
                            width: 16; height: 12
                            radius: 2
                            color: Theme.isDark ? Qt.rgba(0, 0, 0, 0.65) : Qt.rgba(0, 0, 0, 0.5)
                            ColoredIcon {
                                anchors.centerIn: parent
                                source: Theme.icon("play")
                                iconSize: 8
                                color: Theme.colors.textOnPrimary
                            }
                        }
                    }

                    MouseArea {
                        id: bottomThumbMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            _stopCurrentMedia()
                            root.currentIndex = index
                        }
                    }
                    
                    Rectangle {
                        id: bottomDeleteBtn
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 2
                        width: 14; height: 14
                        radius: 7
                        color: bottomDeleteBtnMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
                        visible: showDeleteBtn
                        z: 10
                        
                        opacity: visible ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 100 } }
                        
                        Text {
                            anchors.centerIn: parent
                            text: "\u00D7"
                            color: Theme.colors.textOnPrimary
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                        
                        MouseArea {
                            id: bottomDeleteBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.fileRemoved(root.messageId, index)
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }

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

    function _formatTime(ms) {
        var totalSec = Math.floor(ms / 1000)
        var h = Math.floor(totalSec / 3600)
        var m = Math.floor((totalSec % 3600) / 60)
        var s = totalSec % 60
        if (h > 0)
            return "%1:%2:%3".arg(h).arg(String(m).padStart(2, '0')).arg(String(s).padStart(2, '0'))
        return "%1:%2".arg(String(m).padStart(2, '0')).arg(String(s).padStart(2, '0'))
    }

    onClosing: {
        if (isVideo && mediaPlayer) mediaPlayer.stop()
    }
}
