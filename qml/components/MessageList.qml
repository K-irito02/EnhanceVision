import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 消息列表组件
 * 展示所有处理任务的消息列表，支持滚动浏览
 * 每条消息包含缩略图、进度、操作按钮等
 *
 * 当 C++ messageModel 可用时，使用真实数据；
 * 否则回退到内置 demoModel 演示数据。
 */
Item {
    id: root

    // ========== 属性 ==========
    property bool batchMode: false
    property var selectedIds: []
    readonly property bool _hasRealModel: typeof messageModel !== "undefined"

    // ========== 消息列表 ==========
    ListView {
        id: messageList
        anchors.fill: parent
        clip: true
        spacing: 12
        bottomMargin: 12

        model: root._hasRealModel ? messageModel : demoModel

        delegate: MessageItem {
            id: msgDelegate
            width: messageList.width

            required property int index
            required property var model

            taskId: model.id || ""
            timestamp: model.timestamp ? Qt.formatDateTime(model.timestamp, "yyyy-MM-dd hh:mm:ss") : ""
            mode: model.mode !== undefined ? model.mode : 0
            status: model.status !== undefined ? model.status : 0
            progress: model.progress !== undefined ? model.progress : 0.0
            queuePosition: model.queuePosition !== undefined ? model.queuePosition : 0
            errorMessage: model.errorMessage || ""
            selectable: batchMode
            estimatedRemainingSec: model.status === 1 ? Math.max(0, Math.round((100 - (model.progress || 0)) * 0.6)) : 0

            // mediaFiles 和 completedCount 的构建
            property ListModel _cachedMedia: ListModel {}
            mediaFiles: _cachedMedia
            totalFileCount: _cachedMedia.count
            completedCount: _countCompleted()

            function _countCompleted() {
                var c = 0
                for (var i = 0; i < _cachedMedia.count; i++) {
                    if (_cachedMedia.get(i).status === 2) c++
                }
                return c
            }

            Component.onCompleted: _buildMediaForDelegate()

            Connections {
                target: model
                function onMediaFilesChanged() { msgDelegate._buildMediaForDelegate() }
            }

            function _buildMediaForDelegate() {
                _cachedMedia.clear()
                if (root._hasRealModel && model.mediaFiles) {
                    var files = model.mediaFiles
                    for (var i = 0; i < files.length; i++) {
                        var f = files[i]
                        
                        var thumbSource = ""
                        if (f.status === 2) {
                            if (f.processedThumbnailId && f.processedThumbnailId !== "") {
                                thumbSource = "image://thumbnail/" + f.processedThumbnailId
                            } else if (f.resultPath && f.resultPath !== "") {
                                thumbSource = "image://thumbnail/" + f.resultPath
                            } else if (f.filePath) {
                                thumbSource = "image://thumbnail/" + f.filePath
                            }
                        } else {
                            if (f.filePath) {
                                thumbSource = "image://thumbnail/" + f.filePath
                            }
                        }
                        
                        _cachedMedia.append({
                            "filePath":  f.filePath  || "",
                            "fileName":  f.fileName  || "",
                            "mediaType": f.mediaType !== undefined ? f.mediaType : 0,
                            "thumbnail": thumbSource,
                            "status":    f.status    !== undefined ? f.status : 0,
                            "resultPath": f.resultPath || "",
                            "processedThumbnailId": f.processedThumbnailId || ""
                        })
                    }
                } else {
                    root._buildDemoMedia(_cachedMedia, model.status)
                }
            }

            onCancelClicked: {
                if (typeof processingController !== "undefined")
                    processingController.cancelTask(model.id)
            }
            onEditClicked: {
                root._openEditDialog(model.id)
            }
            onSaveClicked: {
                root._handleDownload(model.id, model.status)
            }
            onDownloadClicked: {
                root._handleDownload(model.id, model.status)
            }
            onDeleteClicked: {
                if (root._hasRealModel)
                    messageModel.removeMessage(model.id)
                else
                    demoModel.remove(msgDelegate.index)
            }
            onViewMediaFile: function(idx) {
                root._openViewer(model.id, idx, model.status)
            }
            onSaveMediaFile: function(idx) {
                root._saveOneFile(model.id, idx)
            }
            onDeleteMediaFile: function(idx) {
                if (root._hasRealModel) {
                    messageModel.removeMediaFile(model.id, idx)
                } else {
                    console.log("删除文件（demo模式）:", model.id, "索引:", idx)
                }
            }
            onSelectionToggled: function(isSelected) {
                var mid = model.id
                if (isSelected) {
                    selectedIds = selectedIds.concat([mid])
                } else {
                    selectedIds = selectedIds.filter(function(id) { return id !== mid })
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            active: true
            policy: ScrollBar.AsNeeded
        }

        // 空状态
        Item {
            visible: messageList.count === 0
            anchors.fill: parent

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 48; height: 48; radius: 24
                    color: Theme.colors.primarySubtle

                    ColoredIcon {
                        anchors.centerIn: parent
                        source: Theme.icon("loader")
                        iconSize: 20
                        color: Theme.colors.icon
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("暂无处理任务")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 13
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("添加文件并点击发送开始处理")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                    opacity: 0.7
                }
            }
        }
    }

    // ========== 媒体查看器窗口 ==========
    MediaViewerWindow {
        id: viewerWindow
        messageMode: true
        property string currentMessageId: ""
        
        onFileRemoved: function(msgId, fileIndex) {
            if (root._hasRealModel) {
                messageModel.removeMediaFile(msgId, fileIndex)
            }
        }
    }
    
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
    
    // 监听 C++ MessageModel 的 mediaFileRemoved 信号
    Connections {
        target: root._hasRealModel ? messageModel : null
        enabled: root._hasRealModel
        function onMediaFileRemoved(messageId, fileIndex) {
            if (viewerWindow.visible && viewerWindow.currentMessageId === messageId) {
                root._syncViewerWindow(messageId, fileIndex)
            }
        }
        function onMessageRemoved(messageId) {
            if (viewerWindow.visible && viewerWindow.currentMessageId === messageId) {
                viewerWindow.close()
            }
        }
    }

    // ========== 演示数据模型 ==========
    ListModel {
        id: demoModel
    }

    Component.onCompleted: {
        if (!_hasRealModel) {
            _populateDemoData()
        }
    }

    // ========== 内部方法 ==========

    /** @brief 构建 demo 模式的媒体文件 */
    function _buildDemoMedia(targetModel, msgStatus) {
        var count = 5 + Math.floor(Math.random() * 16)
        for (var i = 0; i < count; i++) {
            var fileStatus = 0
            if (msgStatus === 2) fileStatus = 2
            else if (msgStatus === 1) fileStatus = (i < count * 0.6) ? 2 : 1
            targetModel.append({
                "filePath": "",
                "fileName": "media_" + (i+1) + (Math.random() > 0.3 ? ".jpg" : ".mp4"),
                "mediaType": Math.random() > 0.3 ? 0 : 1,
                "thumbnail": "",
                "status": fileStatus,
                "resultPath": ""
            })
        }
    }

    /** @brief 打开媒体查看器 */
    function _openViewer(msgId, fileIndex, msgStatus) {
        var files = []
        if (_hasRealModel) {
            var allFiles = messageModel.getMediaFiles(msgId)
            
            for (var i = 0; i < allFiles.length; i++) {
                var f = allFiles[i]
                var filePath = f.filePath || ""
                var resultPath = f.resultPath || ""
                
                if (f.status === 2) {
                    var thumbSource = ""
                    if (f.processedThumbnailId && f.processedThumbnailId !== "") {
                        thumbSource = "image://thumbnail/" + f.processedThumbnailId
                    } else if (resultPath && resultPath !== "") {
                        thumbSource = "image://thumbnail/" + resultPath
                    } else if (filePath && filePath !== "") {
                        thumbSource = "image://thumbnail/" + filePath
                    }
                    
                    files.push({
                        "filePath":  resultPath || filePath,
                        "fileName":  f.fileName  || "",
                        "mediaType": f.mediaType !== undefined ? f.mediaType : 0,
                        "thumbnail": thumbSource,
                        "resultPath": resultPath,
                        "originalPath": filePath,
                        "status": f.status,
                        "processedThumbnailId": f.processedThumbnailId || ""
                    })
                }
            }
        } else {
            for (var j = 0; j < 10; j++) {
                files.push({
                    "filePath":  "demo://placeholder_" + (j+1) + ".jpg",
                    "fileName":  "media_" + (j+1) + ".jpg",
                    "mediaType": 0,
                    "thumbnail": "",
                    "resultPath": "",
                    "originalPath": ""
                })
            }
        }
        
        if (files.length > 0) {
            viewerWindow.messageId = msgId
            viewerWindow.currentMessageId = msgId
            viewerWindow.mediaFiles = files
            
            // 获取消息存储的 shader 参数
            var shaderParams = _hasRealModel ? messageModel.getShaderParams(msgId) : {}
            
            // 设置 shaderEnabled 为 true，让 MediaViewerWindow 内部的 _shouldApplyShader 
            // 逻辑来决定是否真正应用 shader（基于每个文件的处理状态）
            // 这样可以正确处理：已处理文件显示结果图，查看原图时应用 shader
            var hasShaderModifications = shaderParams.hasShaderModifications === true
            viewerWindow.shaderEnabled = hasShaderModifications
            
            // 设置 shader 参数（使用正确的默认值）
            viewerWindow.shaderBrightness = shaderParams.brightness !== undefined ? shaderParams.brightness : 0.0
            viewerWindow.shaderContrast = shaderParams.contrast !== undefined ? shaderParams.contrast : 1.0
            viewerWindow.shaderSaturation = shaderParams.saturation !== undefined ? shaderParams.saturation : 1.0
            viewerWindow.shaderHue = shaderParams.hue !== undefined ? shaderParams.hue : 0.0
            viewerWindow.shaderSharpness = shaderParams.sharpness !== undefined ? shaderParams.sharpness : 0.0
            viewerWindow.shaderBlur = shaderParams.blur !== undefined ? shaderParams.blur : 0.0
            viewerWindow.shaderDenoise = shaderParams.denoise !== undefined ? shaderParams.denoise : 0.0
            viewerWindow.shaderExposure = shaderParams.exposure !== undefined ? shaderParams.exposure : 0.0
            viewerWindow.shaderGamma = shaderParams.gamma !== undefined ? shaderParams.gamma : 1.0
            viewerWindow.shaderTemperature = shaderParams.temperature !== undefined ? shaderParams.temperature : 0.0
            viewerWindow.shaderTint = shaderParams.tint !== undefined ? shaderParams.tint : 0.0
            viewerWindow.shaderVignette = shaderParams.vignette !== undefined ? shaderParams.vignette : 0.0
            viewerWindow.shaderHighlights = shaderParams.highlights !== undefined ? shaderParams.highlights : 0.0
            viewerWindow.shaderShadows = shaderParams.shadows !== undefined ? shaderParams.shadows : 0.0
            
            viewerWindow.openAt(Math.min(fileIndex, files.length - 1))
        }
    }
    
    /** @brief 同步更新查看器窗口数据 */
    function _syncViewerWindow(messageId, deletedIndex) {
        var files = []
        var allFiles = messageModel.getMediaFiles(messageId)
        
        for (var i = 0; i < allFiles.length; i++) {
            var f = allFiles[i]
            var filePath = f.filePath || ""
            var resultPath = f.resultPath || ""
            
            if (f.status === 2) {
                var thumbSource = ""
                if (f.processedThumbnailId && f.processedThumbnailId !== "") {
                    thumbSource = "image://thumbnail/" + f.processedThumbnailId
                } else if (resultPath && resultPath !== "") {
                    thumbSource = "image://thumbnail/" + resultPath
                } else if (filePath && filePath !== "") {
                    thumbSource = "image://thumbnail/" + filePath
                }
                
                files.push({
                    "filePath":  resultPath || filePath,
                    "fileName":  f.fileName  || "",
                    "mediaType": f.mediaType !== undefined ? f.mediaType : 0,
                    "thumbnail": thumbSource,
                    "resultPath": resultPath,
                    "originalPath": filePath,
                    "status": f.status,
                    "processedThumbnailId": f.processedThumbnailId || ""
                })
            }
        }
        
        if (files.length === 0) {
            viewerWindow.close()
            return
        }
        
        viewerWindow.mediaFiles = files
        
        if (viewerWindow.currentIndex >= files.length) {
            viewerWindow.currentIndex = Math.max(0, files.length - 1)
        }
    }

    /** @brief 下载已完成文件 */
    function _handleDownload(msgId, msgStatus) {
        if (_hasRealModel && typeof fileController !== "undefined") {
            var completedFiles = messageModel.getCompletedFiles(msgId)
            if (completedFiles.length === 0) {
                console.log("没有已完成的文件可下载:", msgId)
                return
            }
            var paths = []
            for (var i = 0; i < completedFiles.length; i++) {
                var rp = completedFiles[i].resultPath
                if (rp && rp !== "") paths.push(rp)
            }
            if (paths.length > 0) {
                var count = fileController.downloadCompletedFiles(paths)
                console.log("已保存", count, "个文件")
            }
        } else {
            console.log("下载已完成文件（demo模式）:", msgId)
        }
    }

    /** @brief 保存单个文件 */
    function _saveOneFile(msgId, fileIndex) {
        if (_hasRealModel && typeof fileController !== "undefined") {
            var allFiles = messageModel.getMediaFiles(msgId)
            if (fileIndex >= 0 && fileIndex < allFiles.length) {
                var file = allFiles[fileIndex]
                var path = file.resultPath || file.filePath
                if (path && path !== "") {
                    fileController.saveFileTo(path)
                }
            }
        } else {
            console.log("保存文件（demo模式）:", msgId, "索引:", fileIndex)
        }
    }

    /** @brief 打开编辑对话框 */
    function _openEditDialog(msgId) {
        console.log("编辑消息:", msgId)
    }

    /** @brief 填充演示数据 */
    function _populateDemoData() {
        demoModel.append({
            "id": "demo-001-abcd-efgh",
            "timestamp": new Date(2026, 2, 12, 1, 47, 35),
            "mode": 0,
            "status": 2,
            "progress": 100,
            "queuePosition": 0,
            "errorMessage": ""
        })
        demoModel.append({
            "id": "demo-002-ijkl-mnop",
            "timestamp": new Date(2026, 2, 13, 14, 22, 10),
            "mode": 1,
            "status": 1,
            "progress": 65,
            "queuePosition": 0,
            "errorMessage": ""
        })
        demoModel.append({
            "id": "demo-003-qrst-uvwx",
            "timestamp": new Date(2026, 2, 14, 8, 5, 0),
            "mode": 0,
            "status": 0,
            "progress": 0,
            "queuePosition": 2,
            "errorMessage": ""
        })
    }
}
