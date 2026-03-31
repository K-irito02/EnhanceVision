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
    
    property string currentSessionId: ""
    
    property bool isLastMessageFullyVisible: true
    readonly property bool canOverlayLastMessage: !isLastMessageFullyVisible

    ListView {
        id: messageList
        anchors.fill: parent
        clip: true
        spacing: 12
        
        // 性能优化：增加缓存区域，预加载更多项目避免滚动时卡片收缩
        cacheBuffer: 2000  // 缓存区域高度（像素）
        displayMarginBeginning: 500  // 在可见区域上方额外渲染
        displayMarginEnd: 500  // 在可见区域下方额外渲染
        reuseItems: true  // 启用项目复用以提高性能
        
        // 会话切换时隐藏内容，避免用户看到滚动过程
        opacity: isSessionSwitching ? 0 : 1
        
        property bool isScrolling: moving || flicking
        property int scrollTimeout: 0
        property bool autoScrollEnabled: true
        property bool userHasScrolledUp: false
        property bool isSessionSwitching: false
        
        // 标记用户是否在当前会话中主动滚动过（用于决定是否保存位置）
        property bool userHasInteracted: false
        
        // 当用户主动滚动时标记已交互
        onMovingChanged: {
            if (moving && !isSessionSwitching) {
                userHasInteracted = true
            }
        }
        

        model: root._hasRealModel ? messageModel : demoModel
        
        function scrollToBottom() {
            if (messageList.count > 0) {
                messageList.positionViewAtEnd()
            }
        }
        
        function scrollToBottomAnimated() {
            if (messageList.count > 0) {
                var targetY = Math.max(0, messageList.contentHeight - messageList.height)
                if (Math.abs(messageList.contentY - targetY) > 5) {
                    scrollAnimation.to = targetY
                    scrollAnimation.restart()
                }
            }
        }
        
        function scrollToBottomImmediate() {
            if (messageList.count > 0) {
                scrollAnimation.stop()
                messageList.positionViewAtEnd()
            }
        }
        
        function isNearBottom() {
            return messageList.atYEnd || 
                   (messageList.contentHeight - messageList.contentY - messageList.height) < 100
        }
        
        NumberAnimation {
            id: scrollAnimation
            target: messageList
            property: "contentY"
            duration: 300
            easing.type: Easing.OutCubic
            
            onRunningChanged: {
                if (!running) {
                    messageList.positionViewAtEnd()
                }
            }
        }

        delegate: MessageItem {
            id: msgDelegate
            width: messageList.width

            required property int index
            required property var model
            
            // 始终加载内容，避免滚动时卡片收缩
            property bool shouldLoadContent: true

            taskId: model.id || ""
            timestamp: model.timestamp ? Qt.formatDateTime(model.timestamp, "yyyy-MM-dd hh:mm:ss") : ""
            mode: model.mode !== undefined ? model.mode : 0
            status: model.status !== undefined ? model.status : 0
            progress: model.progress !== undefined ? model.progress : 0.0
            queuePosition: model.queuePosition !== undefined ? model.queuePosition : 0
            errorMessage: model.errorMessage || ""
            selectable: batchMode
            // estimatedRemainingSec 现在由 MessageItem 内部基于实际进度速率计算

            property ListModel _cachedMedia: ListModel {}
            property int _successFileCount: 0
            property int _failedFileCount: 0
            property int _pendingFileCount: 0
            property int _processingFileCount: 0
            property bool _fileStatsInitialized: false
            mediaFiles: _cachedMedia
            totalFileCount: _cachedMedia.count
            completedCount: _successFileCount

            function _countCompleted() {
                return _successFileCount
            }

            function _resetFileStatsFromModel() {
                var success = 0
                var failed = 0
                var pending = 0
                var processing = 0
                for (var i = 0; i < _cachedMedia.count; i++) {
                    var entry = _cachedMedia.get(i)
                    if (entry.status === 2) {
                        success++
                    } else if (entry.status === 3 || entry.status === 4) {
                        failed++
                    } else if (entry.status === 1) {
                        processing++
                    } else {
                        pending++
                    }
                }

                _successFileCount = success
                _failedFileCount = failed
                _pendingFileCount = pending
                _processingFileCount = processing
                _fileStatsInitialized = true
                msgDelegate._applyFileStats(success, failed, pending, processing)
            }

            function _bumpFileStats(oldStatus, newStatus) {
                function decByStatus(statusValue) {
                    if (statusValue === 2) {
                        _successFileCount = Math.max(0, _successFileCount - 1)
                    } else if (statusValue === 3 || statusValue === 4) {
                        _failedFileCount = Math.max(0, _failedFileCount - 1)
                    } else if (statusValue === 1) {
                        _processingFileCount = Math.max(0, _processingFileCount - 1)
                    } else {
                        _pendingFileCount = Math.max(0, _pendingFileCount - 1)
                    }
                }

                function incByStatus(statusValue) {
                    if (statusValue === 2) {
                        _successFileCount += 1
                    } else if (statusValue === 3 || statusValue === 4) {
                        _failedFileCount += 1
                    } else if (statusValue === 1) {
                        _processingFileCount += 1
                    } else {
                        _pendingFileCount += 1
                    }
                }

                decByStatus(oldStatus)
                incByStatus(newStatus)

                msgDelegate._applyFileStats(_successFileCount, _failedFileCount, _pendingFileCount, _processingFileCount)
            }

            Component.onCompleted: {
                if (msgDelegate.shouldLoadContent) {
                    _buildMediaForDelegate()
                }
            }

            Connections {
                target: model
                function onStatusChanged() {
                    if (msgDelegate.shouldLoadContent && msgDelegate._cachedMedia.count === 0) {
                        msgDelegate._buildMediaForDelegate()
                    }
                }
            }
            
            onShouldLoadContentChanged: {
                if (shouldLoadContent && _cachedMedia.count === 0) {
                    _buildMediaForDelegate()
                }
            }

            function _findCachedFileIndexById(fileId) {
                for (var i = 0; i < _cachedMedia.count; i++) {
                    if (_cachedMedia.get(i).id === fileId) {
                        return i
                    }
                }
                return -1
            }

            function _patchCachedMediaFile(fileId, fileStatus, fileResultPath) {
                var idx = _findCachedFileIndexById(fileId)
                if (idx < 0) {
                    if (msgDelegate.shouldLoadContent) {
                        _buildMediaForDelegate()
                    }
                    return
                }

                var entry = _cachedMedia.get(idx)
                var oldStatus = entry.status
                var nextStatus = fileStatus
                var nextResultPath = (fileResultPath !== undefined) ? fileResultPath : (entry.resultPath || "")
                var sourcePath = entry.originalPath || entry.filePath || ""
                var viewPath = sourcePath

                if (nextStatus === 2 && nextResultPath && nextResultPath !== "") {
                    viewPath = nextResultPath
                }

                var processedThumbId = entry.processedThumbnailId || ""
                if (nextStatus === 2 && nextResultPath && nextResultPath !== "") {
                    processedThumbId = "processed_" + fileId
                }

                var thumbSource = ""
                if (nextStatus === 2) {
                    if (processedThumbId !== "") {
                        thumbSource = "image://thumbnail/" + processedThumbId
                    } else if (nextResultPath && nextResultPath !== "") {
                        thumbSource = "image://thumbnail/" + nextResultPath
                    } else if (sourcePath !== "") {
                        thumbSource = "image://thumbnail/" + sourcePath
                    }
                } else if (sourcePath !== "") {
                    thumbSource = "image://thumbnail/" + sourcePath
                }

                _cachedMedia.setProperty(idx, "status", nextStatus)
                _cachedMedia.setProperty(idx, "resultPath", nextResultPath)
                _cachedMedia.setProperty(idx, "filePath", viewPath)
                _cachedMedia.setProperty(idx, "thumbnail", thumbSource)
                _cachedMedia.setProperty(idx, "processedThumbnailId", processedThumbId)

                if (_fileStatsInitialized && oldStatus !== nextStatus) {
                    _bumpFileStats(oldStatus, nextStatus)
                }
            }

            function _buildMediaForDelegate() {
                if (!(root._hasRealModel && model.mediaFiles)) {
                    if (_cachedMedia.count === 0) {
                        root._buildDemoMedia(_cachedMedia, model.status)
                    }
                    _resetFileStatsFromModel()
                    return
                }

                var files = model.mediaFiles
                var targetCount = files.length

                for (var i = 0; i < targetCount; i++) {
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
                    } else if (f.filePath) {
                        thumbSource = "image://thumbnail/" + f.filePath
                    }

                    var filePath = (f.status === 2 && f.resultPath) ? f.resultPath : (f.filePath || "")
                    var row = {
                        "id": f.id || "",
                        "filePath": filePath,
                        "fileName": f.fileName || "",
                        "mediaType": f.mediaType !== undefined ? f.mediaType : 0,
                        "thumbnail": thumbSource,
                        "status": f.status !== undefined ? f.status : 0,
                        "resultPath": f.resultPath || "",
                        "originalPath": f.filePath || "",
                        "processedThumbnailId": f.processedThumbnailId || ""
                    }

                    if (i < _cachedMedia.count) {
                        _cachedMedia.set(i, row)
                    } else {
                        _cachedMedia.append(row)
                    }
                }

                while (_cachedMedia.count > targetCount) {
                    _cachedMedia.remove(_cachedMedia.count - 1)
                }

                _resetFileStatsFromModel()
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
    
    // 模型重置后恢复滚动位置的定时器
    Timer {
        id: restoreAfterResetTimer
        interval: 100  // 给 ListView 足够时间计算内容高度
        repeat: false
        onTriggered: {
            // 检查是否有保存的位置（只有用户主动滚动过才会保存）
            var savedPos = root._sessionScrollPositions[root.currentSessionId]
            if (savedPos !== undefined && savedPos.userHasInteracted) {
                // 恢复到用户之前滚动到的位置
                var targetY = Math.max(0, Math.min(savedPos.contentY, messageList.contentHeight - messageList.height)) + messageList.originY
                messageList.contentY = targetY
                messageList.userHasScrolledUp = savedPos.userHasScrolledUp
                messageList.userHasInteracted = true
            } else {
                // 用户从未在此会话中滚动过，显示底部最新消息
                messageList.positionViewAtEnd()
                messageList.userHasScrolledUp = false
                messageList.userHasInteracted = false
            }
            messageList.isSessionSwitching = false
        }
    }
    
    Connections {
        target: root._hasRealModel ? messageModel : null
        enabled: root._hasRealModel

        // 模型即将重置：只有用户主动滚动过才保存位置
        function onModelAboutToReset() {
            // 保存当前会话的位置（如果用户主动滚动过）
            if (root.currentSessionId !== "" && messageList.userHasInteracted) {
                root._sessionScrollPositions[root.currentSessionId] = {
                    contentY: messageList.contentY - messageList.originY,
                    userHasScrolledUp: messageList.userHasScrolledUp,
                    userHasInteracted: true
                }
            }
            messageList.isSessionSwitching = true
        }
        
        // 模型重置完成：恢复滚动位置
        function onModelResetCompleted() {
            // 延迟执行，确保 ListView 内容高度已计算完成
            restoreAfterResetTimer.restart()
        }

        function _patchVisibleDelegate(messageId, fileId, fileStatus, resultPath) {
            if (!messageList.contentItem) {
                return
            }

            var children = messageList.contentItem.children
            for (var i = 0; i < children.length; i++) {
                var child = children[i]
                if (child && child.taskId === messageId && child._patchCachedMediaFile) {
                    child._patchCachedMediaFile(fileId, fileStatus, resultPath)
                    break
                }
            }
        }

        function onMediaFilePatched(messageId, fileId, fileStatus, resultPath) {
            _patchVisibleDelegate(messageId, fileId, fileStatus, resultPath)
        }

        function onMessageFileStatsChanged(messageId, successCount, failedCount, pendingCount, processingCount) {
            if (!messageList.contentItem) {
                return
            }

            var children = messageList.contentItem.children
            for (var i = 0; i < children.length; i++) {
                var child = children[i]
                if (child && child.taskId === messageId && child._applyFileStats) {
                    child._successFileCount = successCount
                    child._failedFileCount = failedCount
                    child._pendingFileCount = pendingCount
                    child._processingFileCount = processingCount
                    child._fileStatsInitialized = true
                    child._applyFileStats(successCount, failedCount, pendingCount, processingCount)
                    break
                }
            }
        }

        function onMessageMediaFilesReloaded(messageId) {
            if (!messageList.contentItem) {
                return
            }

            var children = messageList.contentItem.children
            for (var i = 0; i < children.length; i++) {
                var child = children[i]
                if (child && child.taskId === messageId && child._buildMediaForDelegate) {
                    child._buildMediaForDelegate()
                    break
                }
            }
        }

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
        
        function onMessageAdded(messageId) {
            if (messageList.autoScrollEnabled || messageList.isNearBottom()) {
                Qt.callLater(messageList.scrollToBottomAnimated)
            }
        }
        
        function onStatusUpdated(messageId, status) {
            if (status === 2 && !messageList.userHasScrolledUp) {
                Qt.callLater(messageList.scrollToBottomAnimated)
            }
        }
    }
    
    Connections {
        target: messageList
        function onCountChanged() {
            // 会话切换期间不自动滚动
            if (messageList.count > 0 && !messageList.userHasScrolledUp && !messageList.isSessionSwitching) {
                Qt.callLater(messageList.scrollToBottomAnimated)
            }
            Qt.callLater(root._updateLastMessageVisibility)
        }
        
        function onMovingChanged() {
            if (messageList.moving && !messageList.isNearBottom()) {
                messageList.userHasScrolledUp = true
            }
            if (messageList.moving && messageList.isNearBottom()) {
                messageList.userHasScrolledUp = false
            }
        }
        
        function onContentYChanged() {
            Qt.callLater(root._updateLastMessageVisibility)
        }
        
        function onHeightChanged() {
            Qt.callLater(root._updateLastMessageVisibility)
        }
        
        function onContentHeightChanged() {
            Qt.callLater(root._updateLastMessageVisibility)
        }
    }
    
    // 会话滚动位置存储（只保存用户主动滚动过的会话）
    property var _sessionScrollPositions: ({})
    
    function _updateLastMessageVisibility() {
        if (messageList.count === 0) {
            root.isLastMessageFullyVisible = true
            return
        }
        
        var lastIndex = messageList.count - 1
        var lastDelegate = messageList.itemAtIndex(lastIndex)
        if (!lastDelegate) {
            root.isLastMessageFullyVisible = true
            return
        }
        
        var lastItemTop = lastDelegate.y
        var lastItemBottom = lastDelegate.y + lastDelegate.height
        var visibleTop = messageList.contentY
        var visibleBottom = messageList.contentY + messageList.height
        
        root.isLastMessageFullyVisible = (lastItemTop >= visibleTop && lastItemBottom <= visibleBottom)
    }
    
    Connections {
        target: root.minimizedDock
        enabled: root.minimizedDock !== null
        function onHeightChanged() {
            if (!messageList.userHasScrolledUp || messageList.isNearBottom()) {
                Qt.callLater(messageList.scrollToBottom)
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
        Qt.callLater(messageList.scrollToBottom)
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
                    if (resultPath && resultPath !== "") {
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
