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
            "thumbVersion": 0
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

    // 从 image://thumbnail/<resourceId>?v=N 中提取裸资源 ID
    // thumbnailReady 信号发出的 id 就是这个裸资源 ID
    function _extractResourceId(url) {
        if (!url || url === "") return ""
        var s = String(url)
        var q = s.indexOf("?")
        if (q >= 0) s = s.substring(0, q)
        var prefix = "image://thumbnail/"
        if (s.indexOf(prefix) === 0) return s.substring(prefix.length)
        return s
    }

    // 计算某个 filteredModel 行对应的裸资源 ID（与 thumbnailReady 信号 id 对齐）
    function _resourceIdForRow(rowData, isSuccess) {
        if (!rowData) return ""
        if (root.messageMode && isSuccess) {
            if (rowData.processedThumbnailId && rowData.processedThumbnailId !== "")
                return rowData.processedThumbnailId
            if (rowData.resultPath && rowData.resultPath !== "")
                return rowData.resultPath
        }
        var thumb = rowData.thumbnail
        if (thumb && thumb !== "") {
            if (thumb.indexOf("image://thumbnail/") === 0)
                return thumb.substring("image://thumbnail/".length)
            // 纯本地路径，不走版本机制
            return ""
        }
        return rowData.filePath || ""
    }

    // thumbnailReady 携带解码后的裸资源 ID → 找到所有匹配行 → 仅对那几行 thumbVersion+1
    // 这样只有对应 delegate 的 source 绑定重新计算，而不触发全部 delegate 刷新
    function _bumpThumbVersion(readyId) {
        if (!readyId || readyId === "") return
        for (var i = 0; i < filteredModel.count; i++) {
            var row = filteredModel.get(i)
            var isSuccess = (row.status === 2)
            var rid = _resourceIdForRow(row, isSuccess)
            if (rid === readyId) {
                filteredModel.setProperty(i, "thumbVersion", (row.thumbVersion || 0) + 1)
            }
        }
    }

    // 使用行内 thumbVersion 驱动版本后缀，仅该行变化时重新求值
    function _thumbnailSourceForItem(itemData, isSuccess) {
        if (!itemData) return ""

        var base = ""
        if (root.messageMode && isSuccess) {
            if (itemData.processedThumbnailId && itemData.processedThumbnailId !== "") {
                base = "image://thumbnail/" + itemData.processedThumbnailId
            } else if (itemData.resultPath && itemData.resultPath !== "") {
                base = "image://thumbnail/" + itemData.resultPath
            }
        }

        if (base === "") {
            var path = itemData.thumbnail
            if (path && path !== "") {
                if (path.indexOf("image://") === 0) {
                    base = path
                } else {
                    // 纯本地路径，直接返回，无需版本后缀
                    return path
                }
            } else {
                var fp = itemData.filePath
                if (fp && fp !== "") {
                    base = "image://thumbnail/" + fp
                }
            }
        }

        if (base === "") return ""

        // 用行内版本号驱动刷新：仅该行的 thumbVersion 变化时，source 绑定重新求值
        var version = itemData.thumbVersion || 0
        return base + "?v=" + version
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
            for (var i = 0; i < mediaModel.rowCount(); i++) {
                var idx = mediaModel.index(i, 0)
                var item = {
                    filePath: mediaModel.data(idx, 258) || "",
                    fileName: mediaModel.data(idx, 259) || "",
                    mediaType: mediaModel.data(idx, 262),
                    thumbnail: mediaModel.data(idx, 263) || "",
                    status: mediaModel.data(idx, 266),
                    resultPath: mediaModel.data(idx, 267) || "",
                    processedThumbnailId: ""
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
            item = {
                filePath: mediaModel.data(idx, 258) || "",
                fileName: mediaModel.data(idx, 259) || "",
                mediaType: mediaModel.data(idx, 262),
                thumbnail: mediaModel.data(idx, 263) || "",
                status: mediaModel.data(idx, 266),
                resultPath: mediaModel.data(idx, 267) || "",
                processedThumbnailId: ""
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

                            Image {
                                id: thumbImage
                                anchors.fill: parent
                                source: root._thumbnailSourceForItem(thumbDelegate.itemData, thumbDelegate.isSuccess)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                smooth: true
                                sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)
                                visible: status === Image.Ready
                                opacity: 1.0
                                layer.enabled: true
                                layer.samples: 4
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
                                    radius: Theme.radius.md
                                    color: "white"
                                    antialiasing: true
                                }
                            }

                            ColoredIcon {
                                anchors.centerIn: parent
                                source: !thumbDelegate.itemData ? Theme.icon("image") : 
                                        (thumbDelegate.itemData.mediaType === 1 ? Theme.icon("video") : Theme.icon("image"))
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
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    var status = thumbDelegate.itemData ? thumbDelegate.itemData.status : 0
                                    contextMenu.targetIndex = thumbDelegate.index
                                    contextMenu.fileStatus = status
                                    contextMenu.popup()
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
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                    if (root.messageId !== "") {
                                        root.fileRemoved(root.messageId, origIndex)
                                    }
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
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                    if (root.messageId !== "") {
                                        root.fileRemoved(root.messageId, origIndex)
                                    }
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

                            Image {
                                id: thumbImage
                                anchors.fill: parent
                                source: root._thumbnailSourceForItem(thumbDelegate.itemData, thumbDelegate.isSuccess)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                smooth: true
                                sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)
                                visible: status === Image.Ready
                                opacity: 1.0
                                layer.enabled: true
                                layer.samples: 4
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
                                    radius: Theme.radius.md
                                    color: "white"
                                    antialiasing: true
                                }
                            }

                            ColoredIcon {
                                anchors.centerIn: parent
                                source: !thumbDelegate.itemData ? Theme.icon("image") : 
                                        (thumbDelegate.itemData.mediaType === 1 ? Theme.icon("video") : Theme.icon("image"))
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
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    var status = thumbDelegate.itemData ? thumbDelegate.itemData.status : 0
                                    contextMenu.targetIndex = thumbDelegate.index
                                    contextMenu.fileStatus = status
                                    contextMenu.popup()
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
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                    if (root.messageId !== "") {
                                        root.fileRemoved(root.messageId, origIndex)
                                    }
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
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
                                    root.deleteFile(origIndex)
                                    if (root.messageId !== "") {
                                        root.fileRemoved(root.messageId, origIndex)
                                    }
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

    Menu {
        id: contextMenu
        property int targetIndex: -1
        property int fileStatus: 0
        readonly property bool isSuccess: fileStatus === 2
        readonly property bool isFailedOrCancelled: fileStatus === 3 || fileStatus === 4
        width: 160
        background: Rectangle { color: Theme.colors.popover; border.width: 1; border.color: Theme.colors.border; radius: Theme.radius.md }
        
        MenuItem {
            visible: !root.messageMode || contextMenu.isSuccess
            height: visible ? 32 : 0
            text: qsTr("放大查看")
            background: Rectangle { color: parent.highlighted ? Theme.colors.surfaceHover : "transparent"; radius: Theme.radius.sm }
            contentItem: Row {
                spacing: 8; leftPadding: 8
                ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("eye"); iconSize: 14; color: Theme.colors.foreground }
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("放大查看"); color: Theme.colors.foreground; font.pixelSize: 13 }
            }
            onTriggered: { if (contextMenu.targetIndex >= 0) { var item = filteredModel.get(contextMenu.targetIndex); root.viewFile(item ? item.origIndex : contextMenu.targetIndex) } }
        }

        MenuItem {
            visible: root.messageMode && contextMenu.isSuccess
            height: visible ? 32 : 0
            text: qsTr("保存")
            background: Rectangle { color: parent.highlighted ? Theme.colors.surfaceHover : "transparent"; radius: Theme.radius.sm }
            contentItem: Row {
                spacing: 8; leftPadding: 8
                ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("save"); iconSize: 14; color: Theme.colors.foreground }
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("保存"); color: Theme.colors.foreground; font.pixelSize: 13 }
            }
            onTriggered: { if (contextMenu.targetIndex >= 0) { var item = filteredModel.get(contextMenu.targetIndex); root.saveFile(item ? item.origIndex : contextMenu.targetIndex) } }
        }

        MenuItem {
            visible: root.messageMode && contextMenu.isFailedOrCancelled
            height: visible ? 32 : 0
            text: qsTr("重新处理")
            background: Rectangle { color: parent.highlighted ? Theme.colors.surfaceHover : "transparent"; radius: Theme.radius.sm }
            contentItem: Row {
                spacing: 8; leftPadding: 8
                ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("refresh-cw"); iconSize: 14; color: Theme.colors.foreground }
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("重新处理"); color: Theme.colors.foreground; font.pixelSize: 13 }
            }
            onTriggered: { if (contextMenu.targetIndex >= 0) { var item = filteredModel.get(contextMenu.targetIndex); root.retryFailedFile(item ? item.origIndex : contextMenu.targetIndex) } }
        }

        MenuSeparator { 
            visible: true
            height: 9
            contentItem: Rectangle { implicitHeight: 1; color: Theme.colors.border } 
        }

        MenuItem {
            visible: true
            height: 32
            text: qsTr("删除")
            background: Rectangle { color: parent.highlighted ? Theme.colors.destructiveSubtle : "transparent"; radius: Theme.radius.sm }
            contentItem: Row {
                spacing: 8; leftPadding: 8
                ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("trash"); iconSize: 14; color: Theme.colors.destructive }
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("删除"); color: Theme.colors.destructive; font.pixelSize: 13 }
            }
            onTriggered: { 
                if (contextMenu.targetIndex >= 0) { 
                    var item = filteredModel.get(contextMenu.targetIndex)
                    var origIndex = item ? item.origIndex : contextMenu.targetIndex
                    root.deleteFile(origIndex)
                    if (root.messageId !== "") {
                        root.fileRemoved(root.messageId, origIndex)
                    }
                } 
            }
        }
    }
}
