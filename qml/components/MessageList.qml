import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

Item {
    id: root

    property bool batchMode: false
    property var selectedIds: []
    readonly property bool _hasRealModel: typeof messageModel !== "undefined"
    
    property Item viewerContainer: null
    property var pendingViewer: null
    property var messageViewer: null
    property var minimizedDock: null

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
                            "filePath":  (f.status === 2 && f.resultPath) ? f.resultPath : (f.filePath || ""),
                            "fileName":  f.fileName  || "",
                            "mediaType": f.mediaType !== undefined ? f.mediaType : 0,
                            "thumbnail": thumbSource,
                            "status":    f.status    !== undefined ? f.status : 0,
                            "resultPath": f.resultPath || "",
                            "originalPath": f.filePath || "",
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
            onRetryClicked: {
                root._handleRetry(model.id)
            }
            onRetryFailedFilesClicked: {
                root._handleRetryFailedFiles(model.id)
            }
            onSaveSuccessfulFilesClicked: {
                root._handleSaveSuccessfulFiles(model.id)
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
                }
            }
            onRetrySingleFailedFile: function(idx) {
                root._handleRetrySingleFailedFile(model.id, idx)
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
    
    Connections {
        target: root._hasRealModel ? messageModel : null
        enabled: root._hasRealModel
        function onMediaFileRemoved(messageId, fileIndex) {
            if (root.messageViewer && root.messageViewer.isOpen && 
                root.messageViewer.currentMessageId === messageId) {
                root._syncViewerWindow(messageId, fileIndex)
            }
        }
        function onMessageRemoved(messageId) {
            if (root.messageViewer && root.messageViewer.isOpen && 
                root.messageViewer.currentMessageId === messageId) {
                root.messageViewer.close()
            }
        }
    }

    ListModel {
        id: demoModel
    }

    Component.onCompleted: {
        if (!_hasRealModel) {
            _populateDemoData()
        }
    }

    function _buildDemoMedia(targetModel, msgStatus) {
        var count = 5 + Math.floor(Math.random() * 16)
        for (var i = 0; i < count; i++) {
            var fileStatus = 0
            if (msgStatus === 2) fileStatus = 2
            else if (msgStatus === 1) fileStatus = (i < count * 0.6) ? 2 : 1
            else if (msgStatus === 3) fileStatus = (i < count * 0.3) ? 2 : 3
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

    function _openViewer(msgId, fileIndex, msgStatus) {
        if (!root.messageViewer) {
            return
        }
        
        var files = []
        if (_hasRealModel) {
            var allFiles = messageModel.getMediaFiles(msgId)
            
            for (var i = 0; i < allFiles.length; i++) {
                var f = allFiles[i]
                var filePath = f.filePath || ""
                var resultPath = f.resultPath || ""
                var mediaType = f.mediaType !== undefined ? f.mediaType : 0
                
                if (f.status === 2) {
                    var thumbSource = ""
                    if (f.processedThumbnailId && f.processedThumbnailId !== "") {
                        thumbSource = "image://thumbnail/" + f.processedThumbnailId
                    } else if (resultPath && resultPath !== "") {
                        thumbSource = "image://thumbnail/" + resultPath
                    } else if (filePath && filePath !== "") {
                        thumbSource = "image://thumbnail/" + filePath
                    }
                    
                    var viewPath = filePath
                    if (mediaType === 0 && resultPath && resultPath !== "") {
                        viewPath = resultPath
                    }
                    
                    files.push({
                        "filePath":  viewPath,
                        "fileName":  f.fileName  || "",
                        "mediaType": mediaType,
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
            var viewer = root.messageViewer
            viewer.messageId = msgId
            viewer.currentMessageId = msgId
            viewer.mediaFiles = files
            
            var shaderParams = _hasRealModel ? messageModel.getShaderParams(msgId) : {}
            
            var hasShaderModifications = shaderParams.hasShaderModifications === true
            viewer.shaderEnabled = hasShaderModifications
            
            viewer.shaderBrightness = shaderParams.brightness !== undefined ? shaderParams.brightness : 0.0
            viewer.shaderContrast = shaderParams.contrast !== undefined ? shaderParams.contrast : 1.0
            viewer.shaderSaturation = shaderParams.saturation !== undefined ? shaderParams.saturation : 1.0
            viewer.shaderHue = shaderParams.hue !== undefined ? shaderParams.hue : 0.0
            viewer.shaderSharpness = shaderParams.sharpness !== undefined ? shaderParams.sharpness : 0.0
            viewer.shaderBlur = shaderParams.blur !== undefined ? shaderParams.blur : 0.0
            viewer.shaderDenoise = shaderParams.denoise !== undefined ? shaderParams.denoise : 0.0
            viewer.shaderExposure = shaderParams.exposure !== undefined ? shaderParams.exposure : 0.0
            viewer.shaderGamma = shaderParams.gamma !== undefined ? shaderParams.gamma : 1.0
            viewer.shaderTemperature = shaderParams.temperature !== undefined ? shaderParams.temperature : 0.0
            viewer.shaderTint = shaderParams.tint !== undefined ? shaderParams.tint : 0.0
            viewer.shaderVignette = shaderParams.vignette !== undefined ? shaderParams.vignette : 0.0
            viewer.shaderHighlights = shaderParams.highlights !== undefined ? shaderParams.highlights : 0.0
            viewer.shaderShadows = shaderParams.shadows !== undefined ? shaderParams.shadows : 0.0
            
            viewer.openAt(Math.min(fileIndex, files.length - 1))
        }
    }
    
    function _syncViewerWindow(messageId, deletedIndex) {
        if (!root.messageViewer) return
        
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
            root.messageViewer.close()
            return
        }
        
        root.messageViewer.mediaFiles = files
        
        if (root.messageViewer.currentIndex >= files.length) {
            root.messageViewer.currentIndex = Math.max(0, files.length - 1)
        }
    }

    function _handleDownload(msgId, msgStatus) {
        if (_hasRealModel && typeof fileController !== "undefined") {
            var completedFiles = messageModel.getCompletedFiles(msgId)
            if (completedFiles.length === 0) {
                return
            }
            var paths = []
            for (var i = 0; i < completedFiles.length; i++) {
                var rp = completedFiles[i].resultPath
                if (rp && rp !== "") paths.push(rp)
            }
            if (paths.length > 0) {
                fileController.downloadCompletedFiles(paths)
            }
        }
    }

    function _handleSaveSuccessfulFiles(msgId) {
        if (_hasRealModel && typeof fileController !== "undefined") {
            var completedFiles = messageModel.getCompletedFiles(msgId)
            if (completedFiles.length === 0) {
                return
            }
            var paths = []
            for (var i = 0; i < completedFiles.length; i++) {
                var rp = completedFiles[i].resultPath
                if (rp && rp !== "") paths.push(rp)
            }
            if (paths.length > 0) {
                fileController.downloadCompletedFiles(paths)
            }
        }
    }

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
        }
    }

    function _handleRetry(msgId) {
        if (_hasRealModel && typeof processingController !== "undefined") {
            processingController.retryMessage(msgId)
        }
    }

    function _handleRetryFailedFiles(msgId) {
        if (_hasRealModel && typeof processingController !== "undefined") {
            processingController.retryFailedFiles(msgId)
        }
    }

    function _handleRetrySingleFailedFile(msgId, fileIndex) {
        if (_hasRealModel && typeof processingController !== "undefined") {
            processingController.retrySingleFailedFile(msgId, fileIndex)
        }
    }

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
            "status": 3,
            "progress": 0,
            "queuePosition": 0,
            "errorMessage": ""
        })
        demoModel.append({
            "id": "demo-004-yzab-cdef",
            "timestamp": new Date(2026, 2, 15, 10, 30, 0),
            "mode": 0,
            "status": 2,
            "progress": 100,
            "queuePosition": 0,
            "errorMessage": ""
        })
    }
}
