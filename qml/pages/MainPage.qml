import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../styles"
import "../components"
import "../controls"

/**
 * @brief 主页面组件
 * 
 * 主应用页面，三层布局（从上到下）：
 * (3) 已发送消息展示区 - 占满剩余空间
 * (2) 上传多媒体文件预览区 - 水平缩略图条
 * (1) 底部功能按钮区 - 模式选择、上传、发送
 * 参考功能设计文档 0.2 和 UI 设计文档 08-ui-design.md
 */
Rectangle {
    id: root
    color: Theme.colors.background
    
    // ========== 属性定义 ==========
    property int processingMode: 0  // 0: Shader, 1: AI
    property bool hasFiles: typeof fileModel !== "undefined" ? fileModel.count > 0 : pendingFilesModel.count > 0
    property bool hasMessages: typeof messageModel !== "undefined" ? messageModel.count > 0 : true
    property string currentSessionId: typeof sessionController !== "undefined" ? sessionController.activeSessionId : ""
    // AI 放大倍数：由 App.qml 从 ControlPanel 透传，用于查看器窗口尺寸计算
    property int aiScaleFactor: 1
    
    // 全局z-index计数器，供所有查看器共享
    property int globalZIndexCounter: 1100
    
    // ========== 信号 ==========
    signal expandControlPanel()
    
    // ========== 会话切换监听 ==========
    Connections {
        target: typeof sessionController !== "undefined" ? sessionController : null
        function onActiveSessionChanged() {
            // 会话切换时，关闭消息查看器并清理其最小化标签
            if (messageEmbeddedViewer.isOpen) {
                messageEmbeddedViewer.close()
            }
            // 更新当前会话ID
            root.currentSessionId = sessionController.activeSessionId
        }
    }
    
    // ========== 拖放区域（全页面） ==========
    DropArea {
        id: pageDropArea
        anchors.fill: parent
        
        onDropped: function(drop) {
            if (drop.hasUrls) {
                if (typeof fileController !== "undefined")
                    fileController.addFiles(drop.urls)
                else
                    _addDemoFiles(drop.urls)
            }
        }
        
        // 拖拽高亮覆盖层
        Rectangle {
            anchors.fill: parent
            color: Theme.colors.primarySubtle
            opacity: pageDropArea.containsDrag ? 0.3 : 0
            z: 500
            visible: opacity > 0
            
            Behavior on opacity { NumberAnimation { duration: 150 } }
            
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: pageDropArea.containsDrag
                
                ColoredIcon {
                    Layout.alignment: Qt.AlignHCenter
                    source: Theme.icon("upload")
                    iconSize: 48
                    color: Theme.colors.primary
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("释放以添加文件")
                    color: Theme.colors.primary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }
        }
    }
    
    // ========== 主布局 ==========
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // ========== (3) 消息展示区 ==========
        Item {
            id: messageAreaContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 空状态欢迎界面
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 24
                visible: !root.hasMessages && !root.hasFiles
                
                // 欢迎图标 - 使用自定义 Logo
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/icons/app_icon.png"
                    sourceSize.width: 80
                    sourceSize.height: 80
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
                
                // 欢迎标题
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("开始新的处理任务")
                    color: Theme.colors.foreground
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
                
                // 欢迎描述
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("添加文件并选择处理模式来开始")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 14
                }
                
                // 添加文件按钮
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: addFileRow.implicitWidth + 32
                    height: 44
                    radius: Theme.radius.lg
                    color: Theme.colors.primary
                    
                    Row {
                        id: addFileRow
                        anchors.centerIn: parent
                        spacing: 8
                        
                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("upload")
                            iconSize: 18
                            color: Theme.colors.textOnPrimary
                        }
                        
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("添加文件")
                            color: Theme.colors.textOnPrimary
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileDialog.open()
                    }
                    
                    Behavior on scale { NumberAnimation { duration: 100 } }
                }
                
                // 快捷键提示
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Ctrl+O 添加文件，Ctrl+N 新会话，支持拖放文件")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 12
                    opacity: 0.7
                }
            }
            
            // 消息列表（有消息时显示）
            MessageList {
                id: messageListView
                anchors.fill: parent
                anchors.margins: 12
                anchors.bottomMargin: {
                    var baseMargin = 12
                    var dockHeight = pendingMinimizedDock.height
                    // 预览区域已在 ColumnLayout 中占据空间，这里只需要为停靠区域预留空间
                    // 当允许覆盖时，不预留额外空间
                    if (effectiveCanOverlay) {
                        return baseMargin
                    }
                    // 最新消息卡片完整显示时，只需要为停靠区域预留空间
                    return baseMargin + dockHeight
                }
                visible: root.hasMessages
                
                currentSessionId: root.currentSessionId
                hasFiles: root.hasFiles
                previewAreaHeight: pendingFilePreviewArea.height  // 传递预览区域高度
                
                // 传递容器和查看器引用
                viewerContainer: messageAreaContainer
                pendingViewer: pendingEmbeddedViewer
                messageViewer: messageEmbeddedViewer
                minimizedDock: pendingMinimizedDock
            }
            
            // ========== 内嵌式媒体查看器（待处理文件） ==========
            EmbeddedMediaViewer {
                id: pendingEmbeddedViewer
                anchors.fill: parent
                containerItem: messageAreaContainer
                messageMode: false
                viewerId: "pending-viewer"
                aiScaleFactor: root.aiScaleFactor
                
                shaderEnabled: root.processingMode === 0 && _getShaderParam("hasShaderModifications", false)
                shaderBrightness: _getShaderParam("shaderBrightness", 0.0)
                shaderContrast: _getShaderParam("shaderContrast", 1.0)
                shaderSaturation: _getShaderParam("shaderSaturation", 1.0)
                shaderHue: _getShaderParam("shaderHue", 0.0)
                shaderSharpness: _getShaderParam("shaderSharpness", 0.0)
                shaderBlur: _getShaderParam("shaderBlur", 0.0)
                shaderDenoise: _getShaderParam("shaderDenoise", 0.0)
                shaderExposure: _getShaderParam("shaderExposure", 0.0)
                shaderGamma: _getShaderParam("shaderGamma", 1.0)
                shaderTemperature: _getShaderParam("shaderTemperature", 0.0)
                shaderTint: _getShaderParam("shaderTint", 0.0)
                shaderVignette: _getShaderParam("shaderVignette", 0.0)
                shaderHighlights: _getShaderParam("shaderHighlights", 0.0)
                shaderShadows: _getShaderParam("shaderShadows", 0.0)
                
                onFileRemoved: function(msgId, fileIndex) {
                    if (typeof fileController !== "undefined") {
                        fileController.removeFileAt(fileIndex)
                    } else {
                        pendingFilesModel.remove(fileIndex)
                    }
                }
                
                onMinimizeRequested: function(viewerId, title, thumbnail) {
                    pendingMinimizedDock.addWindow(viewerId, title, thumbnail)
                }
                
                onClosed: {
                    pendingMinimizedDock.removeWindow(viewerId)
                }
            }
            
            // ========== 内嵌式媒体查看器（消息文件） ==========
            EmbeddedMediaViewer {
                id: messageEmbeddedViewer
                anchors.fill: parent
                containerItem: messageAreaContainer
                messageMode: true
                viewerId: "message-viewer"
                // messageMode 下结果图已是 scaleFactor 倍，传 1 避免重复放大
                aiScaleFactor: 1
                
                property string currentMessageId: ""
                
                onFileRemoved: function(msgId, fileIndex) {
                    if (typeof messageModel !== "undefined") {
                        messageModel.removeMediaFile(msgId, fileIndex)
                    }
                }
                
                onMinimizeRequested: function(viewerId, title, thumbnail) {
                    pendingMinimizedDock.addWindow(viewerId, title, thumbnail)
                }
                
                onClosed: {
                    pendingMinimizedDock.removeWindow(viewerId)
                }
            }
            
            // ========== 最小化窗口停靠区 ==========
            MinimizedWindowDock {
                id: pendingMinimizedDock
                objectName: "minimizedDock"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                z: messageListView.effectiveCanOverlay ? 100 : 5
                
                onRestoreWindow: function(viewerId) {
                    if (viewerId === "pending-viewer") {
                        pendingEmbeddedViewer.restore()
                    } else if (viewerId === "message-viewer") {
                        messageEmbeddedViewer.restore()
                    }
                    removeWindow(viewerId)
                }
                
                onCloseWindow: function(viewerId) {
                    if (viewerId === "pending-viewer") {
                        pendingEmbeddedViewer.close()
                    } else if (viewerId === "message-viewer") {
                        messageEmbeddedViewer.close()
                    }
                    removeWindow(viewerId)
                }
            }
        }
        
        // ========== (2) 底部待处理文件预览区 ==========
        Rectangle {
            id: pendingFilePreviewArea
            Layout.fillWidth: true
            Layout.preferredHeight: root.hasFiles ? 100 : 0
            visible: root.hasFiles
            color: Theme.colors.card
            z: messageListView.effectiveCanOverlay ? 100 : 5
            
            // 顶部分隔线
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.border
            }
            
            // 使用 MediaThumbnailStrip 展示待处理文件
            MediaThumbnailStrip {
                id: pendingStrip
                anchors.fill: parent
                anchors.margins: 10
                mediaModel: typeof fileModel !== "undefined" ? fileModel : pendingFilesModel
                thumbSize: 80
                thumbSpacing: 8
                expandable: false
                onlyCompleted: false
                messageMode: false
                
                onViewFile: function(index) {
                    _openPendingFileViewer(index)
                }
                onSaveFile: function(index) {
                }
                onDeleteFile: function(index) {
                    if (typeof fileController !== "undefined")
                        fileController.removeFileAt(index)
                    else
                        pendingFilesModel.remove(index)
                }
            }
        }
        
        // ========== (1) 底部操作栏 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.colors.card
            
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.border
            }
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12
                
                // ========== 模式选择 ==========
                RowLayout {
                    spacing: 4
                    
                    // Shader 模式按钮
                    Rectangle {
                        width: shaderRow.implicitWidth + 16
                        height: 36
                        radius: Theme.radius.md
                        color: root.processingMode === 0 ? Theme.colors.primary : "transparent"
                        border.width: root.processingMode === 0 ? 0 : 1
                        border.color: Theme.colors.border
                        
                        Row {
                            id: shaderRow
                            anchors.centerIn: parent
                            spacing: 6
                            
                            ColoredIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                source: Theme.icon("sliders")
                                iconSize: 14
                                color: root.processingMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Shader"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: root.processingMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.processingMode = 0
                                root.expandControlPanel()
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                    
                    // AI 模式按钮
                    Rectangle {
                        width: aiRow.implicitWidth + 16
                        height: 36
                        radius: Theme.radius.md
                        color: root.processingMode === 1 ? Theme.colors.primary : "transparent"
                        border.width: root.processingMode === 1 ? 0 : 1
                        border.color: Theme.colors.border
                        
                        Row {
                            id: aiRow
                            anchors.centerIn: parent
                            spacing: 6
                            
                            ColoredIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                source: Theme.icon("sparkles")
                                iconSize: 14
                                color: root.processingMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("AI推理")
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: root.processingMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.processingMode = 1
                                root.expandControlPanel()
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // ========== 右侧操作按钮 ==========
                RowLayout {
                    spacing: 8
                    
                    // 上传按钮
                    IconButton {
                        iconName: "upload"
                        iconSize: 18
                        btnSize: 40
                        tooltip: qsTr("添加文件")
                        onClicked: fileDialog.open()
                    }
                    
                    // 发送按钮
                    Rectangle {
                        width: 40
                        height: 40
                        radius: Theme.radius.md
                        color: root.hasFiles ? Theme.colors.primary : Theme.colors.muted
                        enabled: root.hasFiles
                        
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("send")
                            iconSize: 18
                            color: root.hasFiles ? "#FFFFFF" : Theme.colors.mutedForeground
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: root.hasFiles ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (root.hasFiles) {
                                    _sendForProcessing()
                                }
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }
            }
        }
    }
    
    // 待处理文件查看器已移至 messageAreaContainer 内的 EmbeddedMediaViewer
    
    // 监听 fileModel 变化，同步更新查看器窗口
    Connections {
        target: typeof fileModel !== "undefined" ? fileModel : null
        enabled: typeof fileModel !== "undefined"
        function onRowsInserted(parent, first, last) { _syncPendingViewerWindow() }
        function onRowsRemoved(parent, first, last) { _syncPendingViewerWindow() }
        function onDataChanged(topLeft, bottomRight, roles) { _syncPendingViewerWindow() }
        function onModelReset() { _syncPendingViewerWindow() }
    }
    
    // 监听 pendingFilesModel 变化（demo 模式）
    Connections {
        target: typeof fileModel === "undefined" ? pendingFilesModel : null
        enabled: typeof fileModel === "undefined"
        function onCountChanged() { _syncPendingViewerWindow() }
    }
    
    // ========== 临时待处理文件模型（无 C++ fileModel 时使用） ==========
    ListModel {
        id: pendingFilesModel
    }
    
    // ========== 文件选择对话框 ==========
    FileDialog {
        id: fileDialog
        title: qsTr("选择媒体文件")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("所有支持的文件 (*.jpg *.jpeg *.png *.bmp *.webp *.tiff *.tif *.mp4 *.avi *.mkv *.mov *.flv)"),
            qsTr("图片文件 (*.jpg *.jpeg *.png *.bmp *.webp *.tiff *.tif)"),
            qsTr("视频文件 (*.mp4 *.avi *.mkv *.mov *.flv)"),
            qsTr("所有文件 (*.*)")
        ]
        
        onAccepted: {
            if (typeof fileController !== "undefined") {
                fileController.addFiles(fileDialog.selectedFiles)
            } else {
                _addDemoFiles(fileDialog.selectedFiles)
            }
        }
    }
    
    // ========== 内部方法 ==========
    function _getShaderParam(paramName, defaultValue) {
        var p = root.parent
        while (p) {
            if (p.objectName === "AppRoot" || (p[paramName] !== undefined)) {
                return p[paramName] !== undefined ? p[paramName] : defaultValue
            }
            p = p.parent
        }
        return defaultValue
    }
    
    function _sendForProcessing() {
        if (typeof sessionController !== "undefined") {
            sessionController.ensureActiveSession()
        }
        if (typeof processingController !== "undefined") {
            var params = {}
            if (root.processingMode === 0) {
                params = {
                    brightness: _getShaderParam("shaderBrightness", 0.0),
                    contrast: _getShaderParam("shaderContrast", 1.0),
                    saturation: _getShaderParam("shaderSaturation", 1.0),
                    hue: _getShaderParam("shaderHue", 0.0),
                    sharpness: _getShaderParam("shaderSharpness", 0.0),
                    blur: _getShaderParam("shaderBlur", 0.0),
                    denoise: _getShaderParam("shaderDenoise", 0.0),
                    exposure: _getShaderParam("shaderExposure", 0.0),
                    gamma: _getShaderParam("shaderGamma", 1.0),
                    temperature: _getShaderParam("shaderTemperature", 0.0),
                    tint: _getShaderParam("shaderTint", 0.0),
                    vignette: _getShaderParam("shaderVignette", 0.0),
                    highlights: _getShaderParam("shaderHighlights", 0.0),
                    shadows: _getShaderParam("shaderShadows", 0.0)
                }
            } else {
                var selectedModelId = _getShaderParam("aiSelectedModelId", "")
                var selectedCategory = _getShaderParam("aiSelectedCategory", "")
                var useGpu = _getShaderParam("aiUseGpu", true)
                var tileSize = _getShaderParam("aiTileSize", 0)
                var modelParams = _getShaderParam("aiModelParams", {})

                params = {
                    modelId: selectedModelId,
                    category: selectedCategory,
                    useGpu: useGpu,
                    tileSize: tileSize
                }

                if (modelParams) {
                    var keys = Object.keys(modelParams)
                    for (var i = 0; i < keys.length; ++i) {
                        params[keys[i]] = modelParams[keys[i]]
                    }
                }
            }
            processingController.sendToProcessing(root.processingMode, params)
        }
    }
    
    function _openPendingFileViewer(index) {
        var model = typeof fileModel !== "undefined" ? fileModel : pendingFilesModel
        var files = []
        
        var count = model.count !== undefined ? model.count : (typeof model.rowCount === "function" ? model.rowCount() : 0)
        
        for (var i = 0; i < count; i++) {
            var item = null
            if (typeof model.get === "function") {
                item = model.get(i)
            } else if (typeof model.data === "function" && typeof model.index === "function") {
                var idx = model.index(i, 0)
                item = {
                    filePath: model.data(idx, 258) || "",
                    fileName: model.data(idx, 259) || ("file_" + (i+1)),
                    mediaType: model.data(idx, 262) !== undefined ? model.data(idx, 262) : 0,
                    thumbnail: model.data(idx, 263) || ""
                }
            }
            
            if (item) {
                var filePath = item.filePath || ""
                files.push({
                    "filePath":  filePath,
                    "fileName":  item.fileName || ("file_" + (i+1)),
                    "mediaType": item.mediaType !== undefined ? item.mediaType : 0,
                    "thumbnail": item.thumbnail || (filePath !== "" ? "image://thumbnail/" + filePath : ""),
                    "resultPath": "",
                    "originalPath": filePath
                })
            }
        }
        
        if (files.length > 0) {
            pendingEmbeddedViewer.mediaFiles = files
            pendingEmbeddedViewer.openAt(Math.min(index, files.length - 1))
        }
    }
    
    function _syncPendingViewerWindow() {
        if (!pendingEmbeddedViewer.isOpen) return
        
        var model = typeof fileModel !== "undefined" ? fileModel : pendingFilesModel
        var files = []
        
        var count = model.count !== undefined ? model.count : (typeof model.rowCount === "function" ? model.rowCount() : 0)
        
        for (var i = 0; i < count; i++) {
            var item = null
            if (typeof model.get === "function") {
                item = model.get(i)
            } else if (typeof model.data === "function" && typeof model.index === "function") {
                var idx = model.index(i, 0)
                item = {
                    filePath: model.data(idx, 258) || "",
                    fileName: model.data(idx, 259) || ("file_" + (i+1)),
                    mediaType: model.data(idx, 262) !== undefined ? model.data(idx, 262) : 0,
                    thumbnail: model.data(idx, 263) || ""
                }
            }
            
            if (item) {
                var filePath = item.filePath || ""
                files.push({
                    "filePath":  filePath,
                    "fileName":  item.fileName || ("file_" + (i+1)),
                    "mediaType": item.mediaType !== undefined ? item.mediaType : 0,
                    "thumbnail": item.thumbnail || (filePath !== "" ? "image://thumbnail/" + filePath : ""),
                    "resultPath": "",
                    "originalPath": filePath
                })
            }
        }
        
        if (files.length === 0) {
            pendingEmbeddedViewer.close()
            return
        }
        
        pendingEmbeddedViewer.mediaFiles = files
        if (pendingEmbeddedViewer.currentIndex >= files.length) {
            pendingEmbeddedViewer.currentIndex = Math.max(0, files.length - 1)
        }
    }
    
    function _addDemoFiles(urls) {
        for (var i = 0; i < urls.length; i++) {
            var url = urls[i]
            var path = url.toString()
            
            var localPath = url.toLocalFile ? url.toLocalFile() : path
            var name = localPath.split(/[/\\]/).pop()
            var isVideo = /\.(mp4|avi|mkv|mov|flv)$/i.test(name)
            
            pendingFilesModel.append({
                "filePath": localPath,
                "fileName": name,
                "mediaType": isVideo ? 1 : 0,
                "thumbnail": "",
                "status": 0,
                "resultPath": ""
            })
        }
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
