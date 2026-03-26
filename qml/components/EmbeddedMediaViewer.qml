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

Item {
    id: root
    
    property var mediaFiles: []
    property int currentIndex: 0
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
    
    readonly property var currentFile: mediaFiles.length > 0 && currentIndex >= 0 && currentIndex < mediaFiles.length ? mediaFiles[currentIndex] : null
    readonly property bool isVideo: currentFile ? (currentFile.mediaType === 1) : false
    readonly property bool _shouldApplyShader: shaderEnabled && !messageMode && !showOriginal
    readonly property bool _hasDistinctResult: currentFile && currentFile.resultPath && currentFile.originalPath && currentFile.resultPath !== currentFile.originalPath
    readonly property bool _hasShaderOrOriginal: (shaderEnabled && !messageMode) || (messageMode && _hasDistinctResult)
    readonly property string currentSource: currentFile ? (showOriginal && currentFile.originalPath ? currentFile.originalPath : currentFile.filePath) : ""
    // 预加载路径：始终准备好原图和结果图，用于无闪烁切换
    readonly property string _resultSource: currentFile ? (currentFile.resultPath && currentFile.resultPath !== "" ? currentFile.resultPath : currentFile.filePath) : ""
    readonly property string _originalSource: currentFile ? (currentFile.originalPath && currentFile.originalPath !== "" ? currentFile.originalPath : currentFile.filePath) : ""
    
    signal fileRemoved(string messageId, int fileIndex)
    signal minimizeRequested(string viewerId, string title, string thumbnail)
    signal closed()
    
    property var _mediaPlayer: null
    property bool _videoEnded: false
    property bool _isPlaying: false
    property real _savedW: 800
    property real _savedH: 600
    property bool _userDraggedPosition: false  // 用户是否手动拖动过独立窗口位置
    property real sharedVolume: SettingsController.volume / 100
    property real _volumeBeforeMute: 0.5

    // ========== 窗口尺寸智能调整 ==========
    // 根据媒体分辨率与 AI 放大倍数计算理想窗口尺寸
    // 规则：
    //   1. 内容尺寸 = 媒体原始尺寸 × AI 放大倍数
    //   2. 加上 UI 装饰（标题栏、控制栏、缩略图栏）得到目标窗口尺寸
    //   3. 等比缩放，不超过屏幕工作区 80%
    //   4. 最小保障 480×360（确保控制按钮可点击）
    function computeIdealWindowSize(mediaW, mediaH, scaleFactor) {
        var sf = Math.max(1, scaleFactor || 1)
        // 防御：媒体尺寸无效时使用合理默认值
        var cw = (mediaW > 0 ? mediaW : 1280) * sf
        var ch = (mediaH > 0 ? mediaH : 720)  * sf

        // UI 装饰高度：标题栏(44) + 视频控制栏(80) + 缩略图栏(60)
        var decorH = 44
        if (isVideo) decorH += 80
        if (mediaFiles.length > 1 && messageMode) decorH += 60
        var decorW = 16  // 左右 padding

        var targetW = cw + decorW
        var targetH = ch + decorH

        // 屏幕工作区 80% 上限
        var maxW = Math.max(480, Math.floor(Screen.desktopAvailableWidth  * 0.80))
        var maxH = Math.max(360, Math.floor(Screen.desktopAvailableHeight * 0.80))

        // 等比缩放，保持宽高比不失真
        var scaleToFit = Math.min(1.0, Math.min(maxW / targetW, maxH / targetH))
        targetW = Math.floor(targetW * scaleToFit)
        targetH = Math.floor(targetH * scaleToFit)

        // 最小保障
        targetW = Math.max(480, targetW)
        targetH = Math.max(360, targetH)

        return Qt.size(targetW, targetH)
    }
    
    // AI 放大倍数属性：由外部（如 AIParamsPanel）在推理参数更新时设置。
    // 用于窗口初始尺寸计算，避免超分结果图过小或过大。
    // 默认值 1 = 不放大（浏览模式或 Shader 模式），超分模式由调用方注入。
    property int aiScaleFactor: 1

    function openAt(index) {
        currentIndex = Math.max(0, Math.min(index, mediaFiles.length - 1))
        showOriginal = false

        // 如果窗口已最小化，则恢复窗口并移除最小化标签
        if (isMinimized) {
            isMinimized = false
            // 通知移除最小化标签
            closed()  // 触发closed信号，MainPage会移除最小化标签
        }

        isOpen = true

        // ── 智能调整独立窗口初始尺寸（仅在切换到该文件时重算） ─────
        // messageMode 下结果图已是 scaleFactor 倍，传 1 避免重复放大；
        // 预览模式传 aiScaleFactor 预估 AI 输出尺寸。
        var file = mediaFiles.length > 0 ? mediaFiles[currentIndex] : null
        if (file && file.resolution && file.resolution.width > 0) {
            var sf = root.messageMode ? 1 : Math.max(1, root.aiScaleFactor)
            var ideal = computeIdealWindowSize(file.resolution.width, file.resolution.height, sf)
            _savedW = ideal.width
            _savedH = ideal.height
        } else {
            _savedW = 800
            _savedH = 600
        }

        // 动态提升z-index，确保最后打开的查看器始终在最上层
        if (displayMode === "embedded") {
            // 使用父级MainPage的全局计数器
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
            // 独立窗口模式：应用智能尺寸
            if (detachedWindow.visibility !== Window.FullScreen) {
                detachedWindow.width  = _savedW
                detachedWindow.height = _savedH
                // 仅在用户未手动拖动过位置时才居中，否则保持拖动后的位置
                if (!_userDraggedPosition) {
                    detachedWindow.x = Math.floor((Screen.desktopAvailableWidth  - _savedW) / 2)
                    detachedWindow.y = Math.floor((Screen.desktopAvailableHeight - _savedH) / 2)
                }
            }
            detachedWindow.show()
            detachedWindow.raise()
        }
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
    
    function close() {
        isOpen = false
        isMinimized = false
        embeddedOverlay.visible = false
        detachedWindow.close()
        root.z = messageMode ? 1000 : 1001  // 重置为基础层级
        if (isVideo && _mediaPlayer) _mediaPlayer.stop()
        closed()
    }
    
    function minimize() {
        if (displayMode === "embedded") {
            isMinimized = true
            embeddedOverlay.visible = false
            minimizeRequested(viewerId, currentFile ? currentFile.fileName : "", currentFile ? (currentFile.thumbnail || "") : "")
        } else {
            detachedWindow.showMinimized()
        }
    }
    
    function restore() {
        isMinimized = false
        if (displayMode === "embedded") {
            // 使用父级MainPage的全局计数器
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
        }
    }
    
    function switchToDetached(gx, gy) {
        if (displayMode === "detached") return
        displayMode = "detached"
        embeddedOverlay.visible = false

        // 若 _savedW/_savedH 是默认值且有媒体分辨率，先用智能尺寸覆盖
        if (_savedW === 800 && _savedH === 600) {
            var file = mediaFiles.length > 0 ? mediaFiles[currentIndex] : null
            if (file && file.resolution && file.resolution.width > 0) {
                var sf = root.messageMode ? 1 : Math.max(1, root.aiScaleFactor)
                var ideal = computeIdealWindowSize(file.resolution.width, file.resolution.height, sf)
                _savedW = ideal.width
                _savedH = ideal.height
            }
        }

        detachedWindow.width  = _savedW
        detachedWindow.height = _savedH
        if (gx !== undefined && gy !== undefined) {
            detachedWindow.x = gx
            detachedWindow.y = gy
        } else {
            detachedWindow.x = Math.floor((Screen.desktopAvailableWidth  - _savedW) / 2)
            detachedWindow.y = Math.floor((Screen.desktopAvailableHeight - _savedH) / 2)
        }
        detachedWindow.show()
        detachedWindow.raise()
        detachedWindow.requestActivate()
    }
    
    function switchToEmbedded() {
        if (displayMode === "embedded") return
        _savedW = detachedWindow.width
        _savedH = detachedWindow.height
        displayMode = "embedded"
        detachedWindow.hide()
        // 吸附回嵌入式时也需提升 z-index，确保覆盖其他嵌入窗口
        var mainPage = _findMainPage()
        if (mainPage && mainPage.globalZIndexCounter !== undefined) {
            mainPage.globalZIndexCounter++
            root.z = mainPage.globalZIndexCounter
        }
        embeddedOverlay.visible = true
        embeddedOverlay.forceActiveFocus()
    }
    
    function _prevFile() { if (currentIndex > 0) currentIndex-- }
    function _nextFile() { if (currentIndex < mediaFiles.length - 1) currentIndex++ }
    function _getSource(s) { return !s ? "" : (s.startsWith("file:///") || s.startsWith("qrc:/") ? s : "file:///" + s) }
    function _formatTime(ms) { var s = Math.floor(ms/1000); return Math.floor(s/60).toString().padStart(2,'0') + ":" + (s%60).toString().padStart(2,'0') }
    
    function _isOverContainer(gx, gy) {
        if (!containerItem) return false
        var p = containerItem.mapToGlobal(0, 0)
        return gx >= p.x && gx <= p.x + containerItem.width && gy >= p.y && gy <= p.y + containerItem.height
    }
    
    function _getMinimizedDockHeight() {
        if (!containerItem) return 0
        // 查找 MinimizedWindowDock 组件
        for (var i = 0; i < containerItem.children.length; i++) {
            var child = containerItem.children[i]
            if (child.objectName === "minimizedDock" || child.toString().indexOf("MinimizedWindowDock") >= 0) {
                return child.height
            }
        }
        return 0
    }

    // === 拖拽预览窗口 ===
    Window {
        id: dragPreviewWindow
        width: _savedW
        height: _savedH
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
                
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 8
                    color: Theme.colors.titleBar
                    radius: 0
                }
                
                Text {
                    anchors.centerIn: parent
                    text: root.currentFile ? root.currentFile.fileName : qsTr("媒体查看器")
                    color: Theme.colors.foreground
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                    anchors.horizontalCenterOffset: 0
                }
            }
            
            Text {
                anchors.centerIn: parent
                text: dragPreviewWindow._snapHint ? qsTr("释放鼠标吸附到消息区域") : qsTr("释放鼠标以独立窗口打开")
                color: dragPreviewWindow._snapHint ? Theme.colors.primaryLight : Theme.colors.mutedForeground
                font.pixelSize: 14
                y: parent.height / 2 + 20
            }
        }
    }
    
    // === 内嵌模式覆盖层 ===
    Rectangle {
        id: embeddedOverlay
        anchors.fill: parent
        anchors.bottomMargin: _getMinimizedDockHeight()
        visible: false
        color: Theme.colors.background
        z: root.z
        
        Rectangle {
            id: titleBar
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
            height: 44
            color: Theme.colors.titleBar
            
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.colors.titleBarBorder }
            
            MouseArea {
                id: titleDragArea
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.rightMargin: btnRow.width + 8
                property real startX: 0
                property real startY: 0
                property bool isDraggingOut: false
                
                cursorShape: Qt.OpenHandCursor
                onPressed: function(m) {
                    startX = m.x
                    startY = m.y
                    cursorShape = Qt.ClosedHandCursor
                }
                onPositionChanged: function(m) {
                    if (!pressed) return
                    
                    var dx = m.x - startX
                    var dy = m.y - startY
                    var distance = Math.sqrt(dx * dx + dy * dy)
                    
                    if (distance > 10 && !isDraggingOut) {
                        isDraggingOut = true
                        var globalPos = mapToGlobal(m.x, m.y)
                        dragPreviewWindow.x = globalPos.x - startX
                        dragPreviewWindow.y = globalPos.y - startY
                        dragPreviewWindow.width = _savedW
                        dragPreviewWindow.height = _savedH
                        dragPreviewWindow.show()
                    }
                    
                    if (isDraggingOut) {
                        var globalPos = mapToGlobal(m.x, m.y)
                        dragPreviewWindow.x = globalPos.x - startX
                        dragPreviewWindow.y = globalPos.y - startY
                        
                        var previewCenterX = dragPreviewWindow.x + dragPreviewWindow.width / 2
                        var previewCenterY = dragPreviewWindow.y + 22
                        dragPreviewWindow._snapHint = root._isOverContainer(previewCenterX, previewCenterY)
                    }
                }
                onReleased: {
                    cursorShape = Qt.OpenHandCursor
                    
                    if (isDraggingOut) {
                        isDraggingOut = false
                        var finalX = dragPreviewWindow.x
                        var finalY = dragPreviewWindow.y
                        var shouldSnap = dragPreviewWindow._snapHint
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
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: 12
                z: 10
                
                Text { text: root.currentFile ? root.currentFile.fileName : qsTr("媒体查看器"); color: Theme.colors.foreground; font.pixelSize: 14; font.weight: Font.Medium; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter }
                Text { text: root.mediaFiles.length > 0 ? (root.currentIndex+1) + "/" + root.mediaFiles.length : ""; color: Theme.colors.mutedForeground; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                
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
                
                Row {
                    id: btnRow; spacing: 2
                    Layout.alignment: Qt.AlignVCenter
                    IconButton { iconName: "external-link"; iconSize: 16; btnSize: 32; tooltip: qsTr("独立窗口"); onClicked: root.switchToDetached() }
                    IconButton { iconName: "minus"; iconSize: 16; btnSize: 32; tooltip: qsTr("最小化"); onClicked: root.minimize() }
                    IconButton { iconName: "x"; iconSize: 16; btnSize: 32; danger: true; tooltip: qsTr("关闭"); onClicked: root.close() }
                }
            }
        }
        
        MediaContentArea {
            id: contentArea
            anchors { top: titleBar.bottom; bottom: videoCtrl.visible ? videoCtrl.top : (thumbBar.visible ? thumbBar.top : parent.bottom); left: parent.left; right: parent.right }
            viewer: root
            videoPlayer: vidPlayer
        }
        
        VideoControlBar {
            id: videoCtrl
            anchors { bottom: thumbBar.visible ? thumbBar.top : parent.bottom; left: parent.left; right: parent.right }
            visible: root.isVideo
            viewer: root
            videoPlayer: vidPlayer
            audioOutput: contentArea.audioOutput
        }
        
        ThumbnailBar {
            id: thumbBar
            anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
            visible: root.mediaFiles.length > 1 && root.messageMode
            viewer: root
        }
        
        MediaPlayer {
            id: vidPlayer
            videoOutput: displayMode === "embedded" ? contentArea.videoOutput : detContentArea.videoOutput
            audioOutput: displayMode === "embedded" ? contentArea.audioOutput : detContentArea.audioOutput
            function togglePlay() { playbackState === MediaPlayer.PlayingState ? pause() : play() }
            
            onPlaybackStateChanged: {
                if (playbackState === MediaPlayer.StoppedState) {
                    root._isPlaying = false
                    if (position > 0 && duration > 0 && position >= duration - 500) {
                        root._videoEnded = true
                    }
                } else if (playbackState === MediaPlayer.PlayingState) {
                    root._isPlaying = true
                    root._videoEnded = false
                } else if (playbackState === MediaPlayer.PausedState) {
                    root._isPlaying = false
                }
            }
            
            Component.onCompleted: root._mediaPlayer = this
        }
        
        Keys.onPressed: function(e) {
            if (e.key === Qt.Key_Escape) root.close()
            else if (e.key === Qt.Key_Left) root._prevFile()
            else if (e.key === Qt.Key_Right) root._nextFile()
            else if (e.key === Qt.Key_Space && root.isVideo) vidPlayer.togglePlay()
        }
    }
    
    // === 独立窗口模式 ===
    Window {
        id: detachedWindow
        width: 800; height: 600; minimumWidth: 480; minimumHeight: 360
        title: root.currentFile ? root.currentFile.fileName : qsTr("媒体查看器")
        color: Theme.colors.background
        flags: Qt.Window | Qt.FramelessWindowHint
        visible: false
        
        SubWindowHelper { id: winHelper; Component.onCompleted: { winHelper.setWindow(detachedWindow); winHelper.setMinWidth(480); winHelper.setMinHeight(360); winHelper.setTitleBarHeight(44) } }
        
        property bool _snapHint: false
        property real _savedX: 0
        property real _savedY: 0
        property real _savedW: 800
        property real _savedH: 600
        
        function updateExcludeRegions() {
            winHelper.clearExcludeRegions()
            if (detBtnRow && detBtnRow.width > 0) {
                var globalPos = detBtnRow.mapToItem(detTitle, 0, 0)
                winHelper.addExcludeRegion(globalPos.x, globalPos.y, detBtnRow.width, detBtnRow.height)
            }
            if (detCompareButton && detCompareButton.visible && detCompareButton.width > 0) {
                var globalPos2 = detCompareButton.mapToItem(detTitle, 0, 0)
                winHelper.addExcludeRegion(globalPos2.x, globalPos2.y, detCompareButton.width, detCompareButton.height)
            }
        }
        
        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)
        onVisibleChanged: if (visible) Qt.callLater(updateExcludeRegions)
        
        Rectangle {
            anchors.fill: parent
            color: Theme.colors.background
            
            Rectangle {
                id: detTitle
                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                height: 44
                color: Theme.colors.titleBar
                
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.colors.titleBarBorder }
                
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16; anchors.rightMargin: 8
                    spacing: 12
                    z: 10
                    
                    Text { text: root.currentFile ? root.currentFile.fileName : ""; color: Theme.colors.foreground; font.pixelSize: 14; font.weight: Font.Medium; elide: Text.ElideMiddle; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter }
                    Text { text: root.mediaFiles.length > 0 ? (root.currentIndex+1) + "/" + root.mediaFiles.length : ""; color: Theme.colors.mutedForeground; font.pixelSize: 12; Layout.alignment: Qt.AlignVCenter }
                    
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
                    
                    Row {
                        id: detBtnRow
                        spacing: 2
                        Layout.alignment: Qt.AlignVCenter
                        IconButton { iconName: "minimize-2"; iconSize: 16; btnSize: 32; tooltip: qsTr("嵌入"); onClicked: root.switchToEmbedded() }
                        IconButton { iconName: "minus"; iconSize: 16; btnSize: 32; tooltip: qsTr("最小化"); onClicked: root.minimize() }
                        IconButton { 
                            iconName: detachedWindow.visibility === Window.FullScreen ? "minimize-2" : "maximize"
                            iconSize: 16
                            btnSize: 32
                            tooltip: detachedWindow.visibility === Window.FullScreen ? qsTr("退出全屏") : qsTr("全屏")
                            onClicked: { 
                                if (detachedWindow.visibility === Window.FullScreen) {
                                    detachedWindow.showNormal()
                                    detachedWindow.x = detachedWindow._savedX
                                    detachedWindow.y = detachedWindow._savedY
                                    detachedWindow.width = detachedWindow._savedW
                                    detachedWindow.height = detachedWindow._savedH
                                } else {
                                    // 保存当前窗口几何信息到 winHelper
                                    winHelper.saveNormalGeometry()
                                    // 同时保存到 detachedWindow 属性
                                    detachedWindow._savedX = detachedWindow.x
                                    detachedWindow._savedY = detachedWindow.y
                                    detachedWindow._savedW = detachedWindow.width
                                    detachedWindow._savedH = detachedWindow.height
                                    detachedWindow.showFullScreen()
                                }
                            }
                        }
                        IconButton { iconName: "x"; iconSize: 16; btnSize: 32; danger: true; tooltip: qsTr("关闭"); onClicked: root.close() }
                    }
                }
            }
            
            MediaContentArea {
                id: detContentArea
                anchors.top: detTitle.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: detVidCtrl.visible ? detVidCtrl.top : (detThumb.visible ? detThumb.top : parent.bottom)
                viewer: root
                videoPlayer: vidPlayer
            }
            
            VideoControlBar {
                id: detVidCtrl
                anchors.bottom: detThumb.visible ? detThumb.top : parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                visible: root.isVideo
                viewer: root
                videoPlayer: vidPlayer
                audioOutput: detContentArea.audioOutput
            }
            
            ThumbnailBar {
                id: detThumb
                anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                visible: root.mediaFiles.length > 1
                viewer: root
            }
            
            Rectangle {
                anchors.fill: parent
                color: Theme.colors.primary
                opacity: 0.3
                visible: detachedWindow._snapHint
                radius: 8
                
                Text { anchors.centerIn: parent; text: qsTr("松开鼠标吸附到消息区域"); color: Theme.colors.foreground; font { pixelSize: 16; weight: Font.Medium } }
            }
        }
        
        Shortcut { sequence: "Escape"; onActivated: { if (detachedWindow.visibility === Window.FullScreen) { detachedWindow.showNormal(); detachedWindow.x = detachedWindow._savedX; detachedWindow.y = detachedWindow._savedY; detachedWindow.width = detachedWindow._savedW; detachedWindow.height = detachedWindow._savedH } else root.close() } }
        Shortcut { sequence: "Left"; onActivated: root._prevFile() }
        Shortcut { sequence: "Right"; onActivated: root._nextFile() }
        Shortcut { sequence: "Space"; onActivated: if (root.isVideo) vidPlayer.togglePlay() }
        Shortcut { sequence: "F"; onActivated: { if (detachedWindow.visibility === Window.FullScreen) { detachedWindow.showNormal(); detachedWindow.x = detachedWindow._savedX; detachedWindow.y = detachedWindow._savedY; detachedWindow.width = detachedWindow._savedW; detachedWindow.height = detachedWindow._savedH } else { winHelper.saveNormalGeometry(); detachedWindow._savedX = detachedWindow.x; detachedWindow._savedY = detachedWindow.y; detachedWindow._savedW = detachedWindow.width; detachedWindow._savedH = detachedWindow.height; detachedWindow.showFullScreen() } } }
    }
    
    Connections {
        target: detachedWindow
        function onXChanged() { detachedWindow._snapHint = root._isOverContainer(detachedWindow.x + detachedWindow.width/2, detachedWindow.y + 22) }
        function onYChanged() { detachedWindow._snapHint = root._isOverContainer(detachedWindow.x + detachedWindow.width/2, detachedWindow.y + 22) }
    }
    
    Connections {
        target: winHelper
        function onDraggingChanged() {
            if (!winHelper.isDragging && detachedWindow._snapHint) {
                root._userDraggedPosition = false  // 吸附回嵌入式，重置拖动标记
                root.switchToEmbedded()
            } else if (!winHelper.isDragging && !detachedWindow._snapHint) {
                // 用户拖动独立窗口到新位置（非吸附），标记位置已手动调整
                root._userDraggedPosition = true
            }
        }
    }
    
    Connections {
        target: root
        function onCurrentSourceChanged() {
            if (root.isVideo && root.currentSource) {
                // 先停止当前播放，避免状态混乱
                if (vidPlayer.playbackState === MediaPlayer.PlayingState) {
                    vidPlayer.stop()
                }
                vidPlayer.source = root._getSource(root.currentSource)
            }
        }
        function onIsVideoChanged() {
            // 当从视频切换到图片时，停止视频播放
            if (!root.isVideo && vidPlayer.playbackState !== MediaPlayer.StoppedState) {
                vidPlayer.stop()
                root._videoEnded = false
                root._isPlaying = false
            }
        }
    }
    
    
    component MediaContentArea: Item {
        property var viewer
        property var videoPlayer
        property alias videoOutput: vo
        property alias audioOutput: ao
        clip: true  // 防止超分后大图溢出容器边界

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            z: -1
        }
        
        // ========== 导航按钮状态管理 ==========
        property bool navButtonsVisible: false
        property bool prevBtnHovered: false
        property bool nextBtnHovered: false
        
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
        
        // 隐藏的原始图像，仅作为 FullShaderEffect 的 source（尺寸受父容器约束）
        Image {
            id: imgSrc
            anchors.fill: parent
            visible: false
            source: !viewer.isVideo ? viewer._getSource(viewer.currentSource) : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true; smooth: true; mipmap: true
        }
        // Shader 效果层：fill parent（而非 imgSrc），确保显示区域与容器严格一致
        FullShaderEffect {
            anchors.fill: parent
            source: imgSrc
            visible: !viewer.isVideo && viewer.currentSource && viewer.shaderEnabled && !viewer.messageMode && !viewer.showOriginal
            brightness: viewer.shaderBrightness; contrast: viewer.shaderContrast
            saturation: viewer.shaderSaturation; hue: viewer.shaderHue
            sharpness: viewer.shaderSharpness; blurAmount: viewer.shaderBlur
            denoise: viewer.shaderDenoise; exposure: viewer.shaderExposure
            gamma: viewer.shaderGamma; temperature: viewer.shaderTemperature
            tint: viewer.shaderTint; vignette: viewer.shaderVignette
            highlights: viewer.shaderHighlights; shadows: viewer.shaderShadows
        }
        // ── 无闪烁源件/结果对比：双图预加载 + opacity 切换 ──────────
        // 结果图层（默认显示）
        Image {
            id: resultImageLayer
            anchors.fill: parent
            visible: !viewer.isVideo && viewer.messageMode && viewer._hasDistinctResult
            opacity: viewer.showOriginal ? 0.0 : 1.0
            source: !viewer.isVideo && viewer.messageMode ? viewer._getSource(viewer._resultSource) : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true; smooth: true; mipmap: true
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
        }
        // 原图层（showOriginal 时显示）
        Image {
            id: originalImageLayer
            anchors.fill: parent
            visible: !viewer.isVideo && viewer.messageMode && viewer._hasDistinctResult
            opacity: viewer.showOriginal ? 1.0 : 0.0
            source: !viewer.isVideo && viewer.messageMode && viewer._hasDistinctResult ? viewer._getSource(viewer._originalSource) : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true; smooth: true; mipmap: true
            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
        }
        // 普通图像层（非消息模式 或 无对比结果时显示）
        Image {
            anchors.fill: parent
            visible: !viewer.isVideo && viewer.currentSource && !viewer.messageMode && (viewer.showOriginal || !viewer.shaderEnabled)
            source: !viewer.isVideo ? viewer._getSource(viewer.currentSource) : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true; smooth: true; mipmap: true
        }
        // 消息模式下无对比结果时的回退显示
        Image {
            anchors.fill: parent
            visible: !viewer.isVideo && viewer.messageMode && !viewer._hasDistinctResult && viewer.currentSource
            source: !viewer.isVideo ? viewer._getSource(viewer.currentSource) : ""
            fillMode: Image.PreserveAspectFit
            asynchronous: true; smooth: true; mipmap: true
        }
        
        // 图片区域的鼠标检测
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: true
            acceptedButtons: Qt.NoButton
            z: 10
            
            onPositionChanged: {
                showNavButtonsAndResetTimer()
            }
            onEntered: {
                showNavButtonsAndResetTimer()
            }
            onExited: {
                startAutoHideIfNeeded()
            }
        }
        
        Item {
            anchors.fill: parent; visible: viewer.isVideo
            
            // 仅在预览模式应用视频Shader；消息模式下展示导出结果/原图，不再二次套用
            property bool _applyVideoShader: viewer.shaderEnabled && !viewer.showOriginal && !viewer.messageMode
            
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
                    
                    // 消息模式下，优先使用处理后的缩略图
                    if (viewer.messageMode && viewer.currentFile && viewer.currentFile.status === 2) {
                        if (viewer.currentFile.processedThumbnailId && viewer.currentFile.processedThumbnailId !== "") {
                            return "image://thumbnail/" + viewer.currentFile.processedThumbnailId
                        }
                    }
                    
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
            
            // VideoOutput接收视频流
            VideoOutput { 
                id: vo
                anchors.fill: parent
                z: 0
                // 当应用Shader时隐藏原始输出，由ShaderEffectSource捕获
                visible: !parent._applyVideoShader
            }
            
            // ShaderEffectSource捕获VideoOutput内容
            ShaderEffectSource {
                id: videoShaderSource
                sourceItem: vo
                live: true
                hideSource: parent._applyVideoShader
                visible: false
            }
            
            // 应用Shader效果的视频显示
            ShaderEffect {
                id: videoShaderEffect
                anchors.fill: parent
                visible: parent._applyVideoShader
                z: 0
                
                property var source: videoShaderSource
                property real brightness: viewer.shaderBrightness
                property real contrast: viewer.shaderContrast
                property real saturation: viewer.shaderSaturation
                property real hue: viewer.shaderHue
                property real sharpness: viewer.shaderSharpness
                property real blurAmount: viewer.shaderBlur
                property real denoise: viewer.shaderDenoise
                property real exposure: viewer.shaderExposure
                property real gamma: viewer.shaderGamma
                property real temperature: viewer.shaderTemperature
                property real tint: viewer.shaderTint
                property real vignette: viewer.shaderVignette
                property real highlights: viewer.shaderHighlights
                property real shadows: viewer.shaderShadows
                property size imgSize: Qt.size(vo.sourceRect.width > 0 ? vo.sourceRect.width : vo.width,
                                               vo.sourceRect.height > 0 ? vo.sourceRect.height : vo.height)
                
                vertexShader: "qrc:///resources/shaders/fullshader.vert.qsb"
                fragmentShader: "qrc:///resources/shaders/fullshader.frag.qsb"
                
                supportsAtlasTextures: true
            }
            
            AudioOutput { id: ao; volume: root.sharedVolume }
            Rectangle {
                anchors.centerIn: parent; width: 72; height: 72; radius: 36
                color: Theme.isDark ? Qt.rgba(0,0,0,0.6) : Qt.rgba(1,1,1,0.9)
                visible: viewer.isVideo && videoPlayer && videoPlayer.playbackState !== MediaPlayer.PlayingState
                z: 10
                ColoredIcon { anchors.centerIn: parent; source: Theme.icon("play"); iconSize: 32; color: Theme.colors.primary }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: if (videoPlayer) videoPlayer.togglePlay() }
            }
            
            MouseArea {
                id: videoClickArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                acceptedButtons: Qt.LeftButton
                z: 2
                
                onPositionChanged: {
                    showNavButtonsAndResetTimer()
                }
                onContainsMouseChanged: {
                    if (containsMouse) {
                        showNavButtonsAndResetTimer()
                    } else {
                        startAutoHideIfNeeded()
                    }
                }
                
                onClicked: function(mouse) {
                    if (videoPlayer) videoPlayer.togglePlay()
                    mouse.accepted = false
                }
            }
        }
        
        Rectangle { 
            id: prevBtn
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            radius: 22
            visible: viewer.currentIndex > 0
            opacity: navButtonsVisible ? 1.0 : 0.0
            z: 50
            
            property bool isHovered: pma.containsMouse
            property bool isPressed: pma.pressed
            
            // 玻璃拟态背景：高对比度半透明灰色 + 边框
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
                color: "#FFFFFF"
            }
            
            MouseArea { 
                id: pma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: viewer._prevFile()
                onEntered: {
                    prevBtnHovered = true
                    stopAutoHide()
                }
                onExited: {
                    prevBtnHovered = false
                    startAutoHideIfNeeded()
                }
            }
        }
        
        Rectangle { 
            id: nextBtn
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            radius: 22
            visible: viewer.currentIndex < viewer.mediaFiles.length - 1
            opacity: navButtonsVisible ? 1.0 : 0.0
            z: 50
            
            property bool isHovered: nma.containsMouse
            property bool isPressed: nma.pressed
            
            // 玻璃拟态背景
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
                color: "#FFFFFF"
            }
            
            MouseArea { 
                id: nma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: viewer._nextFile()
                onEntered: {
                    nextBtnHovered = true
                    stopAutoHide()
                }
                onExited: {
                    nextBtnHovered = false
                    startAutoHideIfNeeded()
                }
            }
        }
        
        ColumnLayout { anchors.centerIn: parent; spacing: 12; visible: !viewer.currentFile; ColoredIcon { Layout.alignment: Qt.AlignHCenter; source: Theme.icon("image"); iconSize: 64; color: Theme.colors.mutedForeground } Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("无媒体文件"); color: Theme.colors.mutedForeground; font.pixelSize: 16 } }
    }
    
    component VideoControlBar: Rectangle {
        property var viewer
        property var videoPlayer
        property var audioOutput
        height: 80
        color: Theme.colors.card
        Rectangle { anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 1; color: Theme.colors.border }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 12
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Text { text: viewer._formatTime(videoPlayer ? videoPlayer.position : 0); color: Theme.colors.mutedForeground; font.pixelSize: 12 }
                Slider { Layout.fillWidth: true; from: 0; to: videoPlayer && videoPlayer.duration > 0 ? videoPlayer.duration : 1; value: videoPlayer ? videoPlayer.position : 0; onMoved: if (videoPlayer) videoPlayer.position = value }
                Text { text: viewer._formatTime(videoPlayer ? videoPlayer.duration : 0); color: Theme.colors.mutedForeground; font.pixelSize: 12 }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                IconButton { iconName: "skip-back"; iconSize: 16; btnSize: 32; tooltip: qsTr("-10s"); onClicked: if (videoPlayer) videoPlayer.position = Math.max(0, videoPlayer.position - 10000) }
                IconButton { iconName: videoPlayer && videoPlayer.playbackState === MediaPlayer.PlayingState ? "pause" : "play"; iconSize: 16; btnSize: 32; tooltip: qsTr("播放/暂停"); onClicked: if (videoPlayer) videoPlayer.togglePlay() }
                IconButton { iconName: "skip-forward"; iconSize: 16; btnSize: 32; tooltip: qsTr("+10s"); onClicked: if (videoPlayer) videoPlayer.position = Math.min(videoPlayer.duration, videoPlayer.position + 10000) }
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
                                    // 0.5x -> 浅蓝, 3.0x -> 深蓝
                                    var colors = ["#7CB9E8", "#5B9BD5", "#3A7FC2", "#1E5FAF", "#0D4A9C", "#002FA7"]
                                    return colors[index]
                                }
                                
                                property bool isSelected: {
                                    var currentRate = videoPlayer ? videoPlayer.playbackRate : 1.0
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
                                    onClicked: if (videoPlayer) videoPlayer.playbackRate = speedBtnRect.modelData
                                }

                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                        }
                }
                Item { Layout.fillWidth: true }
                IconButton { 
                    iconName: audioOutput && audioOutput.volume > 0 ? "volume-2" : "volume-x"
                    iconSize: 16; btnSize: 28
                    iconColor: Theme.colors.mediaControlIcon
                    tooltip: qsTr("静音")
                    onClicked: {
                        if (root.sharedVolume > 0) {
                            root._volumeBeforeMute = root.sharedVolume
                            root.sharedVolume = 0
                            SettingsController.volume = 0
                        } else {
                            root.sharedVolume = root._volumeBeforeMute
                            SettingsController.volume = Math.round(root._volumeBeforeMute * 100)
                        }
                    }
                }
                Slider { 
                    implicitWidth: 80
                    from: 0
                    to: 1
                    value: root.sharedVolume
                    onMoved: {
                        root.sharedVolume = value
                        SettingsController.volume = Math.round(value * 100)
                    }
                }
            }
        }
    }
    
    component ThumbnailBar: Rectangle {
        property var viewer
        height: 60
        color: Theme.colors.mediaControlBg
        
        Rectangle { anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; height: 1; color: Theme.colors.mediaControlBorder }
        
        ListView {
            id: thumbListView
            anchors.fill: parent
            anchors.margins: 6
            orientation: ListView.Horizontal
            spacing: 6
            clip: true
            model: viewer.mediaFiles.length
            currentIndex: viewer.currentIndex
            
            delegate: Item {
                required property int index
                width: 48; height: 48
                property bool showDeleteBtn: thumbItemMouse.containsMouse || thumbDeleteMouse.containsMouse
                
                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: "transparent"
                    border.width: 2
                    border.color: index === viewer.currentIndex ? Theme.colors.primary : "transparent"
                    z: 5
                }
                
                Rectangle {
                    id: thumbRect
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: 6
                    color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                    clip: true
                    
                    Image {
                        id: thumbImg
                        anchors.fill: parent
                        source: {
                            var f = viewer.mediaFiles[index]
                            if (!f) return ""
                            
                            var result = ""
                            
                            // 消息模式下，优先使用处理后的缩略图
                            if (viewer.messageMode && f.status === 2) {
                                if (f.processedThumbnailId && f.processedThumbnailId !== "") {
                                    result = "image://thumbnail/" + f.processedThumbnailId
                                } else if (f.resultPath && f.resultPath !== "") {
                                    result = "image://thumbnail/" + f.resultPath
                                } else if (f.thumbnail && f.thumbnail !== "") {
                                    result = f.thumbnail
                                } else if (f.filePath) {
                                    result = "image://thumbnail/" + f.filePath
                                }
                            } else {
                                if (f.thumbnail && f.thumbnail !== "") {
                                    result = f.thumbnail
                                } else if (f.filePath) {
                                    result = "image://thumbnail/" + f.filePath
                                }
                            }
                            
                            return result
                        }
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        cache: false
                        sourceSize: Qt.size(96, 96)
                        visible: status === Image.Ready
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskThresholdMin: 0.5
                            maskSpreadAtMin: 1.0
                            maskSource: thumbMask
                        }
                    }
                    
                    Item {
                        id: thumbMask
                        visible: false
                        layer.enabled: true
                        width: thumbRect.width
                        height: thumbRect.height
                        Rectangle {
                            anchors.fill: parent
                            radius: 6
                            color: "white"
                        }
                    }
                    
                    ColoredIcon {
                        anchors.centerIn: parent
                        source: {
                            var f = viewer.mediaFiles[index]
                            if (!f) return Theme.icon("image")
                            return f.mediaType === 1 ? Theme.icon("video") : Theme.icon("image")
                        }
                        iconSize: 16
                        color: Theme.colors.mutedForeground
                        visible: thumbImg.status !== Image.Ready
                    }
                    
                    Rectangle {
                        visible: {
                            var f = viewer.mediaFiles[index]
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
                            color: "#FFFFFF"
                        }
                    }
                }
                
                MouseArea {
                    id: thumbItemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: viewer.currentIndex = index
                }
                
                Rectangle {
                    id: thumbDeleteBtn
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 2
                    width: 14; height: 14
                    radius: 7
                    color: thumbDeleteMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
                    visible: showDeleteBtn
                    z: 10
                    
                    Text {
                        anchors.centerIn: parent
                        text: "\u00D7"
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                    
                    MouseArea {
                        id: thumbDeleteMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: viewer.fileRemoved(viewer.messageId, index)
                    }
                }
            }
            
            MouseArea {
                anchors.fill: parent
                z: -1
                onWheel: function(wheel) {
                    var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                    thumbListView.contentX = Math.max(0, Math.min(thumbListView.contentX - delta, thumbListView.contentWidth - thumbListView.width))
                }
            }
        }
    }
}
