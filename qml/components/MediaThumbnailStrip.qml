import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"
import "../controls"

Item {
    id: root

    property var mediaModel: ListModel {}
    property string messageId: ""
    property int thumbSize: 80
    property int thumbSpacing: 8
    property bool expandable: false
    property bool expanded: false
    property int collapsedMaxVisible: 10
    property bool onlyCompleted: false
    property bool showFailedFiles: false
    property bool messageMode: false
    property int totalCount: mediaModel ? mediaModel.count : 0
    readonly property int visibleCount: filteredModel.count

    signal viewFile(int index)
    signal saveFile(int index)
    signal deleteFile(int index)
    signal fileRemoved(string messageId, int fileIndex)
    signal retryFailedFile(int index)

    implicitHeight: contentLoader.implicitHeight

    ListModel {
        id: filteredModel
    }

    onMediaModelChanged: _rebuildFiltered()
    onOnlyCompletedChanged: _rebuildFiltered()
    onShowFailedFilesChanged: _rebuildFiltered()
    Component.onCompleted: {
        _rebuildFiltered()
    }
    
    Connections {
        target: mediaModel
        function onDataChanged(topLeft, bottomRight, roles) { _handleDataChanged(topLeft, bottomRight, roles) }
        function onRowsInserted(parent, first, last) { _handleRowsInserted(first, last) }
        function onRowsRemoved(parent, first, last) { _handleRowsRemoved(first, last) }
        function onModelReset() { _rebuildFiltered() }
    }
    
    function _handleDataChanged(topLeft, bottomRight, roles) {
        var changedStart = topLeft !== undefined && topLeft !== null && topLeft.row !== undefined
            ? topLeft.row
            : topLeft
        var changedEnd = bottomRight !== undefined && bottomRight !== null && bottomRight.row !== undefined
            ? bottomRight.row
            : bottomRight

        if (changedStart === undefined || changedEnd === undefined) {
            _rebuildFiltered()
            return
        }

        for (var i = changedStart; i <= changedEnd; i++) {
            var sourceItem = _getSourceItemAtIndex(i)
            var filteredIdx = _findItemByOrigIndex(i)

            if (sourceItem && _shouldIncludeItem(sourceItem)) {
                if (filteredIdx >= 0) {
                    _updateFilteredItem(filteredIdx, sourceItem)
                } else {
                    filteredModel.insert(_findInsertPositionByOrigIndex(i), _createFilteredItem(i, sourceItem))
                }
            } else if (filteredIdx >= 0) {
                filteredModel.remove(filteredIdx)
            }
        }
    }
    
    function _handleRowsInserted(first, last) {
        var insertedCount = last - first + 1

        for (var i = 0; i < filteredModel.count; i++) {
            var orig = filteredModel.get(i).origIndex
            if (orig >= first) {
                filteredModel.setProperty(i, "origIndex", orig + insertedCount)
            }
        }

        for (var sourceIndex = first; sourceIndex <= last; sourceIndex++) {
            var sourceItem = _getSourceItemAtIndex(sourceIndex)
            if (sourceItem && _shouldIncludeItem(sourceItem)) {
                if (_findItemByOrigIndex(sourceIndex) < 0) {
                    filteredModel.insert(_findInsertPositionByOrigIndex(sourceIndex), _createFilteredItem(sourceIndex, sourceItem))
                }
            }
        }
    }
    
    function _handleRowsRemoved(first, last) {
        for (var i = filteredModel.count - 1; i >= 0; i--) {
            var item = filteredModel.get(i)
            if (item.origIndex >= first && item.origIndex <= last) {
                filteredModel.remove(i)
            } else if (item.origIndex > last) {
                filteredModel.setProperty(i, "origIndex", item.origIndex - (last - first + 1))
            }
        }
    }

    Connections {
        target: typeof thumbnailProvider !== 'undefined' ? thumbnailProvider : null
        enabled: typeof thumbnailProvider !== 'undefined'
        function onThumbnailReady(id) {
            root._bumpThumbVersion(id)
        }
    }

    function _shouldIncludeItem(item) {
        if (onlyCompleted) {
            return item.status === 2
        }
        if (!showFailedFiles) {
            return item.status === 2 || item.status === 1 || item.status === 0
        }
        return true
    }

    function _findItemByOrigIndex(origIdx) {
        for (var i = 0; i < filteredModel.count; i++) {
            if (filteredModel.get(i).origIndex === origIdx) {
                return i
            }
        }
        return -1
    }

    function _findInsertPositionByOrigIndex(origIdx) {
        for (var i = 0; i < filteredModel.count; i++) {
            if (filteredModel.get(i).origIndex > origIdx) {
                return i
            }
        }
        return filteredModel.count
    }

    function _updateItemProperty(filteredIdx, key, value) {
        var current = filteredModel.get(filteredIdx)[key]
        if (current !== value) {
            filteredModel.setProperty(filteredIdx, key, value)
        }
    }

    function _createFilteredItem(origIdx, item) {
        return {
            "origIndex": origIdx,
            "filePath": item.filePath || "",
            "fileName": item.fileName || "",
            "mediaType": item.mediaType !== undefined ? item.mediaType : 0,
            "thumbnail": item.thumbnail || "",
            "status": item.status !== undefined ? item.status : 0,
            "resultPath": item.resultPath || "",
            "processedThumbnailId": item.processedThumbnailId || "",
            "thumbVersion": 1
        }
    }

    function _updateFilteredItem(filteredIdx, item) {
        _updateItemProperty(filteredIdx, "filePath", item.filePath || "")
        _updateItemProperty(filteredIdx, "fileName", item.fileName || "")
        _updateItemProperty(filteredIdx, "mediaType", item.mediaType !== undefined ? item.mediaType : 0)
        _updateItemProperty(filteredIdx, "thumbnail", item.thumbnail || "")
        _updateItemProperty(filteredIdx, "status", item.status !== undefined ? item.status : 0)
        _updateItemProperty(filteredIdx, "resultPath", item.resultPath || "")
        _updateItemProperty(filteredIdx, "processedThumbnailId", item.processedThumbnailId || "")
        // 注意：不重置 thumbVersion，保留已有的版本号
    }

    // 计算某个 filteredModel 行当前应使用的裸缓存键（与 C++ thumbnailReady 信号 id 对齐）
    // 规则：已完成→processedThumbnailId 或 resultPath；其他→原始 filePath
    function _cacheKeyForRow(rowData) {
        if (!rowData) return ""
        var isSuccess = (rowData.status === 2)
        if (root.messageMode && isSuccess) {
            if (rowData.processedThumbnailId && rowData.processedThumbnailId !== "")
                return rowData.processedThumbnailId
            if (rowData.resultPath && rowData.resultPath !== "")
                return rowData.resultPath
        }
        return rowData.filePath || ""
    }

    // thumbnailReady 信号 → 找到所有匹配行 → 仅递增那几行的 thumbVersion
    function _bumpThumbVersion(readyId) {
        if (!readyId || readyId === "") return
        for (var i = 0; i < filteredModel.count; i++) {
            var row = filteredModel.get(i)
            var rid = _cacheKeyForRow(row)
            if (rid === readyId) {
                filteredModel.setProperty(i, "thumbVersion", (row.thumbVersion || 0) + 1)
            }
        }
    }

    // 唯一的缩略图 URL 构建入口 — 不依赖 thumbnail 字段，直接从原始路径计算
    function _thumbnailSourceForItem(itemData, isSuccess) {
        if (!itemData) return ""

        var resourceId = ""

        // 已完成的文件：优先使用处理后的缩略图 ID
        if (root.messageMode && isSuccess) {
            if (itemData.processedThumbnailId && itemData.processedThumbnailId !== "") {
                resourceId = itemData.processedThumbnailId
            } else if (itemData.resultPath && itemData.resultPath !== "") {
                resourceId = itemData.resultPath
            }
        }

        // 回退：使用原始文件路径
        if (resourceId === "") {
            resourceId = itemData.filePath || ""
        }

        if (resourceId === "") return ""

        // 用行内版本号驱动刷新：仅该行的 thumbVersion 变化时，source 绑定重新求值
        var version = itemData.thumbVersion || 0
        return "image://thumbnail/" + resourceId + "?v=" + version
    }

    function _getSourceItems() {
        var items = []
        if (!mediaModel) return items
        
        var isQmlListModel = (typeof mediaModel.get === "function") && (mediaModel.count !== undefined)
        var isCppModel = (typeof mediaModel.rowCount === "function") && (typeof mediaModel.data === "function")
        var isArray = Array.isArray(mediaModel) || (typeof mediaModel.length === "number" && typeof mediaModel.get !== "function")
        
        if (isQmlListModel) {
            for (var i = 0; i < mediaModel.count; i++) {
                var item = mediaModel.get(i)
                if (_shouldIncludeItem(item)) {
                    items.push(_createFilteredItem(i, item))
                }
            }
        } else if (isArray) {
            for (var i = 0; i < mediaModel.length; i++) {
                var item = mediaModel[i]
                if (_shouldIncludeItem(item)) {
                    items.push(_createFilteredItem(i, item))
                }
            }
        } else if (isCppModel) {
            var processedThumbnailIds = mediaModel.data(mediaModel.index(0, 0), 276) || []
            for (var i = 0; i < mediaModel.rowCount(); i++) {
                var idx = mediaModel.index(i, 0)
                var item = {
                    filePath: mediaModel.data(idx, 258) || "",
                    fileName: mediaModel.data(idx, 259) || "",
                    mediaType: mediaModel.data(idx, 262),
                    thumbnail: mediaModel.data(idx, 263) || "",
                    status: mediaModel.data(idx, 266),
                    resultPath: mediaModel.data(idx, 267) || "",
                    processedThumbnailId: (processedThumbnailIds && processedThumbnailIds[i]) ? processedThumbnailIds[i] : ""
                }
                if (_shouldIncludeItem(item)) {
                    items.push(_createFilteredItem(i, item))
                }
            }
        }
        return items
    }

    function _getSourceItemAtIndex(sourceIdx) {
        if (!mediaModel) return null
        
        var isQmlListModel = (typeof mediaModel.get === "function") && (mediaModel.count !== undefined)
        var isCppModel = (typeof mediaModel.rowCount === "function") && (typeof mediaModel.data === "function")
        var isArray = Array.isArray(mediaModel) || (typeof mediaModel.length === "number" && typeof mediaModel.get !== "function")
        
        var item = null
        if (isQmlListModel && sourceIdx >= 0 && sourceIdx < mediaModel.count) {
            item = mediaModel.get(sourceIdx)
        } else if (isArray && sourceIdx >= 0 && sourceIdx < mediaModel.length) {
            item = mediaModel[sourceIdx]
        } else if (isCppModel && sourceIdx >= 0 && sourceIdx < mediaModel.rowCount()) {
            var idx = mediaModel.index(sourceIdx, 0)
            var processedThumbnailIds = mediaModel.data(mediaModel.index(0, 0), 276) || []
            item = {
                filePath: mediaModel.data(idx, 258) || "",
                fileName: mediaModel.data(idx, 259) || "",
                mediaType: mediaModel.data(idx, 262),
                thumbnail: mediaModel.data(idx, 263) || "",
                status: mediaModel.data(idx, 266),
                resultPath: mediaModel.data(idx, 267) || "",
                processedThumbnailId: (processedThumbnailIds && processedThumbnailIds[sourceIdx]) ? processedThumbnailIds[sourceIdx] : ""
            }
        }
        return item
    }

    function _rebuildFiltered() {
        if (!mediaModel) {
            filteredModel.clear()
            return
        }
        
        var sourceItems = _getSourceItems()
        
        var sourceIndexMap = {}
        for (var i = 0; i < sourceItems.length; i++) {
            sourceIndexMap[sourceItems[i].origIndex] = sourceItems[i]
        }
        
        for (var i = filteredModel.count - 1; i >= 0; i--) {
            var filteredItem = filteredModel.get(i)
            if (!(filteredItem.origIndex in sourceIndexMap)) {
                filteredModel.remove(i)
            }
        }
        
        for (var i = 0; i < sourceItems.length; i++) {
            var sourceItem = sourceItems[i]
            var filteredIdx = _findItemByOrigIndex(sourceItem.origIndex)
            
            if (filteredIdx >= 0) {
                _updateFilteredItem(filteredIdx, sourceItem)
            } else {
                filteredModel.append(sourceItem)
            }
        }
        
        _refreshAllThumbnails()
    }

    function _refreshAllThumbnails() {
        for (var i = 0; i < filteredModel.count; i++) {
            var row = filteredModel.get(i)
            filteredModel.setProperty(i, "thumbVersion", (row.thumbVersion || 0) + 1)
        }
    }

    Loader {
        id: contentLoader
        anchors.left: parent.left
        anchors.right: parent.right
        sourceComponent: root.expandable && root.expanded ? gridComponent : horizontalComponent
    }

    Component {
        id: horizontalComponent

        ColumnLayout {
            spacing: 6

            Item {
                id: horizontalContainer
                Layout.fillWidth: true
                Layout.preferredHeight: root.thumbSize
                visible: filteredModel.count > 0
                clip: true

                ListView {
                    id: thumbListView
                    anchors.fill: parent
                    orientation: ListView.Horizontal
                    spacing: root.thumbSpacing
                    model: filteredModel
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentWidth > width
                    cacheBuffer: 500
                    reuseItems: false

                    delegate: Item {
                        id: thumbDelegate
                        width: root.thumbSize
                        height: root.thumbSize
                        // required properties 从 ListModel 角色直接绑定，setProperty 变化时自动更新
                        required property int index
                        required property string filePath
                        required property string fileName
                        required property int mediaType
                        required property string thumbnail
                        required property int status
                        required property string resultPath
                        required property string processedThumbnailId
                        required property int thumbVersion
                        required property int origIndex

                        // 兼容旧代码中对 itemData 的访问
                        property var itemData: ({
                            filePath: filePath,
                            fileName: fileName,
                            mediaType: mediaType,
                            thumbnail: thumbnail,
                            status: status,
                            resultPath: resultPath,
                            processedThumbnailId: processedThumbnailId,
                            thumbVersion: thumbVersion,
                            origIndex: origIndex
                        })

                        property int itemStatus: status
                        property bool showDeleteBtn: thumbMouse.containsMouse || deleteBtnMouse.containsMouse || deleteBtnForFailedMouse.containsMouse
                        property bool isPending: itemStatus === 0
                        property bool isProcessing: itemStatus === 1
                        property bool isSuccess: itemStatus === 2
                        property bool isFailed: itemStatus === 3
                        property bool isCancelled: itemStatus === 4
                        property bool isUnavailable: !isSuccess
                        property bool canRetry: isFailed || isCancelled
                        property int itemMediaType: mediaType
                        property bool _deleteInProgress: false
                        property real _deleteOpacity: 1.0
                        property real _imageOpacity: thumbImage.status === Image.Ready ? 1.0 : 0.0

                        Behavior on _deleteOpacity {
                            NumberAnimation { duration: 200; easing.type: Easing.InOutCubic }
                        }
                        
                        Behavior on _imageOpacity {
                            NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                        }

                        Timer {
                            id: debounceTimer
                            interval: 500
                            repeat: false
                            onTriggered: {
                                thumbDelegate._deleteInProgress = false
                                thumbDelegate._deleteOpacity = 1.0
                            }
                        }

                        Rectangle{
                            id: hoverBorder
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: Theme.radius.md
                            color: "transparent"
                            border.width: {
                                if (!thumbDelegate.isSuccess && root.messageMode) return 2
                                if (thumbMouse.containsMouse) return 2
                                return 0
                            }
                            border.color: {
                                if (!thumbDelegate.isSuccess && root.messageMode) return Theme.colors.destructive
                                if (thumbMouse.containsMouse) return Theme.colors.primary
                                return "transparent"
                            }
                            z: 5
                            
                            Behavior on border.color{ ColorAnimation{ duration: Theme.animation.fast } }
                            Behavior on border.width{ NumberAnimation{ duration: Theme.animation.fast } }
                        }

                        Rectangle {
                            id: thumbRect
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: Theme.radius.md
                            color: Theme.colors.surface
                            opacity: thumbDelegate._deleteOpacity

                            Image {
                                id: thumbImage
                                anchors.fill: parent
                                source: root._thumbnailSourceForItem(thumbDelegate.itemData, thumbDelegate.isSuccess)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                smooth: true
                                sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)
                                visible: status === Image.Ready
                                opacity: thumbDelegate._imageOpacity
                                layer.enabled: true
                                layer.samples: 4
                                layer.effect: MultiEffect {
                                    maskEnabled: true
                                    maskThresholdMin: 0.5
                                    maskSpreadAtMin: 1.0
                                    maskSource: thumbMask
                                }

                                property int _retryCount: 0
                                onStatusChanged: {
                                    if (status === Image.Ready) {
                                        _retryCount = 0
                                        thumbRetryTimer.stop()
                                    } else if ((status === Image.Error || status === Image.Null) && _retryCount < 15 && source !== "") {
                                        thumbRetryTimer.interval = 600 * Math.min(_retryCount + 1, 8)
                                        thumbRetryTimer.start()
                                    }
                                }

                                Component.onCompleted: {
                                    // 延迟检查：如果缩略图已在缓存中但当前显示占位图，触发刷新
                                    thumbInitTimer.start()
                                }

                                Timer {
                                    id: thumbInitTimer
                                    interval: 100
                                    repeat: false
                                    onTriggered: {
                                        if (thumbImage.status !== Image.Ready && thumbImage.source !== "") {
                                            var idx = thumbDelegate.index
                                            if (idx >= 0 && idx < filteredModel.count) {
                                                var row = filteredModel.get(idx)
                                                filteredModel.setProperty(idx, "thumbVersion", (row.thumbVersion || 0) + 1)
                                            }
                                        }
                                    }
                                }

                                Timer {
                                    id: thumbRetryTimer
                                    repeat: false
                                    onTriggered: {
                                        if (thumbImage._retryCount < 15) {
                                            thumbImage._retryCount++
                                            var idx = thumbDelegate.index
                                            if (idx >= 0 && idx < filteredModel.count) {
                                                var row = filteredModel.get(idx)
                                                filteredModel.setProperty(idx, "thumbVersion", (row.thumbVersion || 0) + 1)
                                            }
                                        }
                                    }
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
                                    radius: Theme.radius.md
                                    color: "white"
                                    antialiasing: true
                                }
                            }

                            ColoredIcon {
                                anchors.centerIn: parent
                                source: thumbDelegate.itemMediaType === 1 ? Theme.icon("video") : Theme.icon("image")
                                iconSize: 24
                                color: Theme.colors.mutedForeground
                                visible: thumbImage.status !== Image.Ready
                            }

                            Rectangle {
                                visible: thumbDelegate.itemMediaType === 1 && !thumbDelegate.isFailed && !thumbDelegate.isPending && !thumbDelegate.isProcessing
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.margins: 4
                                width: 20; height: 16
                                radius: 3
                                color: Qt.rgba(0, 0, 0, 0.65)
                                ColoredIcon {
                                    anchors.centerIn: parent
                                    source: Theme.icon("play")
                                    iconSize: 10
                                    color: Theme.colors.textOnPrimary
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: Qt.rgba(0, 0, 0, 0.5)
                                visible: thumbDelegate.isProcessing && root.messageMode
                                ColoredIcon {
                                    anchors.centerIn: parent
                                    source: Theme.icon("loader")
                                    iconSize: 20
                                    color: Theme.colors.textOnPrimary
                                    opacity: 0.8
                                    RotationAnimation on rotation {
                                        from: 0; to: 360
                                        duration: 1500
                                        loops: Animation.Infinite
                                        running: thumbDelegate.isProcessing
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: thumbMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: (!root.messageMode || thumbDelegate.isSuccess) ? Qt.PointingHandCursor : Qt.ArrowCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            propagateComposedEvents: true
                            onPressed: function(mouse) {
                                var deleteBtnArea = Qt.rect(parent.width - 24, 4, 20, 20)
                                var inDeleteBtn = mouse.x >= deleteBtnArea.x && mouse.x <= deleteBtnArea.x + deleteBtnArea.width &&
                                                  mouse.y >= deleteBtnArea.y && mouse.y <= deleteBtnArea.y + deleteBtnArea.height
                                if (inDeleteBtn && thumbDelegate.showDeleteBtn) {
                                    mouse.accepted = false
                                }
                            }
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    var status = thumbDelegate.itemData ? thumbDelegate.itemData.status : 0
                                    contextMenu.targetIndex = thumbDelegate.index
                                    contextMenu.fileStatus = status
                                    var globalPos = mapToGlobal(mouse.x, mouse.y)
                                    contextMenu.showAt(globalPos.x, globalPos.y)
                                } else {
                                    if (!root.messageMode || thumbDelegate.isSuccess) {
                                        root.viewFile(thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: deleteBtn
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 4
                            width: 20; height: 20
                            radius: 10
                            color: deleteBtnMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
                            visible: thumbDelegate.showDeleteBtn && !thumbDelegate.isFailed
                            z: 10
                            
                            opacity: visible ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 100 } }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "\u00D7"
                                color: Theme.colors.textOnPrimary
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                            MouseArea {
                                id: deleteBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: thumbDelegate._deleteInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor
                                preventStealing: true
                                onClicked: {
                                    if (thumbDelegate._deleteInProgress) return
                                    thumbDelegate._deleteInProgress = true
                                    thumbDelegate._deleteOpacity = 0.3
                                    debounceTimer.start()
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                }
                            }
                            Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        }

                        Rectangle {
                            id: deleteBtnForFailed
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 4
                            width: 20; height: 20
                            radius: 10
                            color: deleteBtnForFailedMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
                            visible: thumbDelegate.showDeleteBtn && (thumbDelegate.isFailed || thumbDelegate.isCancelled)
                            z: 10
                            
                            opacity: visible ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 100 } }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "\u00D7"
                                color: Theme.colors.textOnPrimary
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                            MouseArea {
                                id: deleteBtnForFailedMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: thumbDelegate._deleteInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor
                                preventStealing: true
                                onClicked: {
                                    if (thumbDelegate._deleteInProgress) return
                                    thumbDelegate._deleteInProgress = true
                                    thumbDelegate._deleteOpacity = 0.3
                                    debounceTimer.start()
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                }
                            }
                            Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        acceptedButtons: Qt.NoButton
                        onWheel: function(wheel) {
                            if (thumbListView.interactive) {
                                var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                                thumbListView.contentX = Math.max(0, Math.min(thumbListView.contentX - delta * 0.5, 
                                    Math.max(0, thumbListView.contentWidth - thumbListView.width)))
                                wheel.accepted = true
                            } else {
                                wheel.accepted = false
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                visible: root.expandable && filteredModel.count > root.collapsedMaxVisible
                color: "transparent"
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Text { anchors.verticalCenter: parent.verticalCenter; text: "\u2237"; color: Theme.colors.primary; font.pixelSize: 14; font.weight: Font.Bold }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("展开查看全部 %1 个文件").arg(filteredModel.count); color: Theme.colors.primary; font.pixelSize: 12; font.weight: Font.Medium }
                    ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("chevron-down"); iconSize: 14; color: Theme.colors.primary }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.expanded = true }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                visible: filteredModel.count === 0
                text: root.onlyCompleted ? qsTr("暂无已完成的文件") : qsTr("暂无文件")
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Component {
        id: gridComponent

        ColumnLayout {
            spacing: 6

            property int columns: Math.max(1, Math.floor((root.width - root.thumbSpacing) / (root.thumbSize + root.thumbSpacing)))
            property int contentHeight: Math.ceil(filteredModel.count / columns) * (root.thumbSize + root.thumbSpacing) - root.thumbSpacing

            Item {
                id: gridContainer
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                visible: filteredModel.count > 0
                clip: true

                GridView {
                    id: thumbGridView
                    anchors.fill: parent
                    cellWidth: root.thumbSize + root.thumbSpacing
                    cellHeight: root.thumbSize + root.thumbSpacing
                    model: filteredModel
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height
                    cacheBuffer: 500
                    reuseItems: false
                    
                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        acceptedButtons: Qt.NoButton
                        onWheel: function(wheel) {
                            if (thumbGridView.interactive) {
                                var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                                thumbGridView.contentY = Math.max(0, Math.min(thumbGridView.contentY - delta * 0.5, 
                                    Math.max(0, thumbGridView.contentHeight - thumbGridView.height)))
                                wheel.accepted = true
                            } else {
                                wheel.accepted = false
                            }
                        }
                    }

                    delegate: Item {
                        id: thumbDelegate
                        width: thumbGridView.cellWidth - root.thumbSpacing
                        height: thumbGridView.cellHeight - root.thumbSpacing
                        // required properties 从 ListModel 角色直接绑定，setProperty 变化时自动更新
                        required property int index
                        required property string filePath
                        required property string fileName
                        required property int mediaType
                        required property string thumbnail
                        required property int status
                        required property string resultPath
                        required property string processedThumbnailId
                        required property int thumbVersion
                        required property int origIndex

                        // 兼容旧代码中对 itemData 的访问
                        property var itemData: ({
                            filePath: filePath,
                            fileName: fileName,
                            mediaType: mediaType,
                            thumbnail: thumbnail,
                            status: status,
                            resultPath: resultPath,
                            processedThumbnailId: processedThumbnailId,
                            thumbVersion: thumbVersion,
                            origIndex: origIndex
                        })

                        property int itemStatus: status
                        property bool showDeleteBtn: thumbMouse.containsMouse || deleteBtnMouse.containsMouse || deleteBtnForFailedMouse.containsMouse
                        property bool isPending: itemStatus === 0
                        property bool isProcessing: itemStatus === 1
                        property bool isSuccess: itemStatus === 2
                        property bool isFailed: itemStatus === 3
                        property bool isCancelled: itemStatus === 4
                        property bool isUnavailable: !isSuccess
                        property bool canRetry: isFailed || isCancelled
                        property int itemMediaType: mediaType
                        property bool _deleteInProgress: false
                        property real _deleteOpacity: 1.0
                        property real _imageOpacity: thumbImage.status === Image.Ready ? 1.0 : 0.0

                        Behavior on _deleteOpacity {
                            NumberAnimation { duration: 200; easing.type: Easing.InOutCubic }
                        }
                        
                        Behavior on _imageOpacity {
                            NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                        }

                        Timer {
                            id: gridDebounceTimer
                            interval: 500
                            repeat: false
                            onTriggered: {
                                thumbDelegate._deleteInProgress = false
                                thumbDelegate._deleteOpacity = 1.0
                            }
                        }

                        Rectangle{
                            id: hoverBorder
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: Theme.radius.md
                            color: "transparent"
                            border.width: {
                                if (!thumbDelegate.isSuccess && root.messageMode) return 2
                                if (thumbMouse.containsMouse) return 2
                                return 0
                            }
                            border.color: {
                                if (!thumbDelegate.isSuccess && root.messageMode) return Theme.colors.destructive
                                if (thumbMouse.containsMouse) return Theme.colors.primary
                                return "transparent"
                            }
                            z: 5
                            
                            Behavior on border.color{ ColorAnimation{ duration: Theme.animation.fast } }
                            Behavior on border.width{ NumberAnimation{ duration: Theme.animation.fast } }
                        }

                        Rectangle {
                            id: thumbRect
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: Theme.radius.md
                            color: Theme.colors.surface
                            opacity: thumbDelegate._deleteOpacity

                            Image {
                                id: thumbImage
                                anchors.fill: parent
                                source: root._thumbnailSourceForItem(thumbDelegate.itemData, thumbDelegate.isSuccess)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                smooth: true
                                sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)
                                visible: status === Image.Ready
                                opacity: thumbDelegate._imageOpacity
                                layer.enabled: true
                                layer.samples: 4
                                layer.effect: MultiEffect {
                                    maskEnabled: true
                                    maskThresholdMin: 0.5
                                    maskSpreadAtMin: 1.0
                                    maskSource: thumbMask
                                }

                                property int _retryCount: 0
                                onStatusChanged: {
                                    if (status === Image.Ready) {
                                        _retryCount = 0
                                        thumbRetryTimer.stop()
                                    } else if ((status === Image.Error || status === Image.Null) && _retryCount < 15 && source !== "") {
                                        thumbRetryTimer.interval = 600 * Math.min(_retryCount + 1, 8)
                                        thumbRetryTimer.start()
                                    }
                                }

                                Component.onCompleted: {
                                    // 延迟检查：如果缩略图已在缓存中但当前显示占位图，触发刷新
                                    thumbInitTimer.start()
                                }

                                Timer {
                                    id: thumbInitTimer
                                    interval: 100
                                    repeat: false
                                    onTriggered: {
                                        if (thumbImage.status !== Image.Ready && thumbImage.source !== "") {
                                            var idx = thumbDelegate.index
                                            if (idx >= 0 && idx < filteredModel.count) {
                                                var row = filteredModel.get(idx)
                                                filteredModel.setProperty(idx, "thumbVersion", (row.thumbVersion || 0) + 1)
                                            }
                                        }
                                    }
                                }

                                Timer {
                                    id: thumbRetryTimer
                                    repeat: false
                                    onTriggered: {
                                        if (thumbImage._retryCount < 15) {
                                            thumbImage._retryCount++
                                            var idx = thumbDelegate.index
                                            if (idx >= 0 && idx < filteredModel.count) {
                                                var row = filteredModel.get(idx)
                                                filteredModel.setProperty(idx, "thumbVersion", (row.thumbVersion || 0) + 1)
                                            }
                                        }
                                    }
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
                                    radius: Theme.radius.md
                                    color: "white"
                                    antialiasing: true
                                }
                            }

                            ColoredIcon {
                                anchors.centerIn: parent
                                source: thumbDelegate.itemMediaType === 1 ? Theme.icon("video") : Theme.icon("image")
                                iconSize: 24
                                color: Theme.colors.mutedForeground
                                visible: thumbImage.status !== Image.Ready
                            }

                            Rectangle {
                                visible: thumbDelegate.itemMediaType === 1 && !thumbDelegate.isFailed && !thumbDelegate.isPending && !thumbDelegate.isProcessing
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.margins: 4
                                width: 20; height: 16
                                radius: 3
                                color: Qt.rgba(0, 0, 0, 0.65)
                                ColoredIcon {
                                    anchors.centerIn: parent
                                    source: Theme.icon("play")
                                    iconSize: 10
                                    color: Theme.colors.textOnPrimary
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: Qt.rgba(0, 0, 0, 0.5)
                                visible: thumbDelegate.isProcessing && root.messageMode
                                ColoredIcon {
                                    anchors.centerIn: parent
                                    source: Theme.icon("loader")
                                    iconSize: 20
                                    color: Theme.colors.textOnPrimary
                                    opacity: 0.8
                                    RotationAnimation on rotation {
                                        from: 0; to: 360
                                        duration: 1500
                                        loops: Animation.Infinite
                                        running: thumbDelegate.isProcessing
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: thumbMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: (!root.messageMode || thumbDelegate.isSuccess) ? Qt.PointingHandCursor : Qt.ArrowCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            propagateComposedEvents: true
                            onPressed: function(mouse) {
                                var deleteBtnArea = Qt.rect(parent.width - 24, 4, 20, 20)
                                var inDeleteBtn = mouse.x >= deleteBtnArea.x && mouse.x <= deleteBtnArea.x + deleteBtnArea.width &&
                                                  mouse.y >= deleteBtnArea.y && mouse.y <= deleteBtnArea.y + deleteBtnArea.height
                                if (inDeleteBtn && thumbDelegate.showDeleteBtn) {
                                    mouse.accepted = false
                                }
                            }
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    var status = thumbDelegate.itemData ? thumbDelegate.itemData.status : 0
                                    contextMenu.targetIndex = thumbDelegate.index
                                    contextMenu.fileStatus = status
                                    var globalPos = mapToGlobal(mouse.x, mouse.y)
                                    contextMenu.showAt(globalPos.x, globalPos.y)
                                } else {
                                    if (!root.messageMode || thumbDelegate.isSuccess) {
                                        root.viewFile(thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: deleteBtn
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 4
                            width: 20; height: 20
                            radius: 10
                            color: deleteBtnMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
                            visible: thumbDelegate.showDeleteBtn && !thumbDelegate.isFailed
                            z: 10
                            
                            opacity: visible ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 100 } }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "\u00D7"
                                color: Theme.colors.textOnPrimary
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                            MouseArea {
                                id: deleteBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: thumbDelegate._deleteInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor
                                preventStealing: true
                                onClicked: {
                                    if (thumbDelegate._deleteInProgress) return
                                    thumbDelegate._deleteInProgress = true
                                    thumbDelegate._deleteOpacity = 0.3
                                    gridDebounceTimer.start()
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                }
                            }
                            Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        }

                        Rectangle {
                            id: deleteBtnForFailed
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 4
                            width: 20; height: 20
                            radius: 10
                            color: deleteBtnForFailedMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
                            visible: thumbDelegate.showDeleteBtn && thumbDelegate.isFailed
                            z: 10
                            
                            opacity: visible ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 100 } }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "\u00D7"
                                color: Theme.colors.textOnPrimary
                                font.pixelSize: 14
                                font.weight: Font.Bold
                            }
                            MouseArea {
                                id: deleteBtnForFailedMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: thumbDelegate._deleteInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor
                                preventStealing: true
                                onClicked: {
                                    if (thumbDelegate._deleteInProgress) return
                                    thumbDelegate._deleteInProgress = true
                                    thumbDelegate._deleteOpacity = 0.3
                                    gridDebounceTimer.start()
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                }
                            }
                            Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        }
                    }
                }

                Rectangle {
                    id: collapseButton
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: Math.max(0, (root.thumbSize - 32) / 2 + 2)
                    anchors.rightMargin: 8
                    width: 32
                    height: 32
                    radius: 16
                    color: Qt.rgba(0, 0, 0, 0.6)
                    visible: root.expandable && root.expanded
                    z: 100
                    
                    ColoredIcon {
                        anchors.centerIn: parent
                        source: Theme.icon("chevron-up")
                        iconSize: 16
                        color: Theme.colors.textOnPrimary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.expanded = false
                    }
                }
            }
        }
    }

    Popup {
        id: contextMenu
        property int targetIndex: -1
        property int fileStatus: 0
        readonly property bool isSuccess: fileStatus === 2
        readonly property bool isFailedOrCancelled: fileStatus === 3 || fileStatus === 4
        
        parent: Overlay.overlay
        padding: 6
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        function showAt(globalX, globalY) {
            var menuWidth = 160
            var menuHeight = calculateMenuHeight()
            var margin = 8
            var offsetX = 4
            var offsetY = 4
            
            var overlayPos = parent.mapFromGlobal(globalX, globalY)
            
            var finalX = overlayPos.x + offsetX
            var finalY = overlayPos.y + offsetY
            
            if (finalX + menuWidth + margin > parent.width) {
                finalX = overlayPos.x - menuWidth - offsetX
            }
            if (finalY + menuHeight + margin > parent.height) {
                finalY = overlayPos.y - menuHeight - offsetY
            }
            
            finalX = Math.max(margin, Math.min(finalX, parent.width - menuWidth - margin))
            finalY = Math.max(margin, Math.min(finalY, parent.height - menuHeight - margin))
            
            contextMenu.x = finalX
            contextMenu.y = finalY
            contextMenu.open()
        }
        
        function calculateMenuHeight() {
            var itemHeight = 34
            var separatorHeight = 9
            var padding = 12
            var spacing = 3
            var count = 0
            var hasSeparator = false
            
            if (!root.messageMode || contextMenu.isSuccess) count++
            if (root.messageMode && contextMenu.isSuccess) count++
            if (root.messageMode && contextMenu.isFailedOrCancelled) count++
            if (count > 0) hasSeparator = true
            count++ // 删除项始终显示
            
            var height = padding * 2 + count * itemHeight + (count - 1) * spacing
            if (hasSeparator) height += separatorHeight + spacing
            
            return height
        }
        
        background: Rectangle {
            implicitWidth: 160
            color: Theme.colors.popover
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg
            
            Rectangle {
                anchors.fill: parent
                anchors.margins: -10
                color: "transparent"
                border.width: 10
                border.color: "transparent"
                z: -1
                
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 10
                    radius: Theme.radius.lg + 3
                    color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                    z: -1
                }
            }
        }
        
        contentItem: ColumnLayout {
            spacing: 3
            
            Rectangle {
                visible: !root.messageMode || contextMenu.isSuccess
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: viewMouse.containsMouse ? Theme.colors.primary : "transparent"
                
                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    
                    ColoredIcon {
                        source: Theme.icon("eye")
                        iconSize: 16
                        color: viewMouse.containsMouse ? Theme.colors.textOnPrimary : Theme.colors.foreground
                    }
                    
                    Text {
                        text: qsTr("放大查看")
                        color: viewMouse.containsMouse ? Theme.colors.textOnPrimary : Theme.colors.foreground
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                MouseArea {
                    id: viewMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        contextMenu.close()
                        if (contextMenu.targetIndex >= 0) {
                            var item = filteredModel.get(contextMenu.targetIndex)
                            root.viewFile(item ? item.origIndex : contextMenu.targetIndex)
                        }
                    }
                }
            }
            
            Rectangle {
                visible: root.messageMode && contextMenu.isSuccess
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: saveMouse.containsMouse ? Theme.colors.primary : "transparent"
                
                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    
                    ColoredIcon {
                        source: Theme.icon("save")
                        iconSize: 16
                        color: saveMouse.containsMouse ? Theme.colors.textOnPrimary : Theme.colors.foreground
                    }
                    
                    Text {
                        text: qsTr("保存")
                        color: saveMouse.containsMouse ? Theme.colors.textOnPrimary : Theme.colors.foreground
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                MouseArea {
                    id: saveMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        contextMenu.close()
                        if (contextMenu.targetIndex >= 0) {
                            var item = filteredModel.get(contextMenu.targetIndex)
                            root.saveFile(item ? item.origIndex : contextMenu.targetIndex)
                        }
                    }
                }
            }
            
            Rectangle {
                visible: root.messageMode && contextMenu.isFailedOrCancelled
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: retryMouse.containsMouse ? Theme.colors.primary : "transparent"
                
                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    
                    ColoredIcon {
                        source: Theme.icon("refresh-cw")
                        iconSize: 16
                        color: retryMouse.containsMouse ? Theme.colors.textOnPrimary : Theme.colors.foreground
                    }
                    
                    Text {
                        text: qsTr("重新处理")
                        color: retryMouse.containsMouse ? Theme.colors.textOnPrimary : Theme.colors.foreground
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                MouseArea {
                    id: retryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        contextMenu.close()
                        if (contextMenu.targetIndex >= 0) {
                            var item = filteredModel.get(contextMenu.targetIndex)
                            root.retryFailedFile(item ? item.origIndex : contextMenu.targetIndex)
                        }
                    }
                }
            }
            
            Rectangle {
                visible: (!root.messageMode || contextMenu.isSuccess) || 
                         (root.messageMode && contextMenu.isSuccess) || 
                         (root.messageMode && contextMenu.isFailedOrCancelled)
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                height: 1
                color: Theme.colors.border
            }
            
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: deleteMouse.containsMouse ? Theme.colors.destructive : "transparent"
                
                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    
                    ColoredIcon {
                        source: Theme.icon("trash")
                        iconSize: 16
                        color: deleteMouse.containsMouse ? Theme.colors.textOnDestructive : Theme.colors.destructive
                    }
                    
                    Text {
                        text: qsTr("删除")
                        color: deleteMouse.containsMouse ? Theme.colors.textOnDestructive : Theme.colors.destructive
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    Item { Layout.fillWidth: true }
                }
                
                MouseArea {
                    id: deleteMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        contextMenu.close()
                        if (contextMenu.targetIndex >= 0) {
                            var item = filteredModel.get(contextMenu.targetIndex)
                            var origIndex = item ? item.origIndex : contextMenu.targetIndex
                            root.deleteFile(origIndex)
                        }
                    }
                }
            }
        }
    }
}
