import QtQuick

Item {
    id: root

    property var mediaModel: null
    property bool onlyCompleted: false
    property bool showFailedFiles: false
    property int totalCount: 0
    property alias model: filteredModel

    ListModel {
        id: filteredModel
    }

    onMediaModelChanged: rebuild()
    onOnlyCompletedChanged: rebuild()
    onShowFailedFilesChanged: rebuild()

    function rebuild() {
        if (!mediaModel) {
            filteredModel.clear()
            totalCount = 0
            return
        }

        var sourceItems = _getSourceItems()
        totalCount = _sourceCount()

        var sourceIndexMap = {}
        for (var i = 0; i < sourceItems.length; ++i) {
            sourceIndexMap[sourceItems[i].origIndex] = sourceItems[i]
        }

        for (var removeIndex = filteredModel.count - 1; removeIndex >= 0; --removeIndex) {
            var filteredItem = filteredModel.get(removeIndex)
            if (!(filteredItem.origIndex in sourceIndexMap)) {
                filteredModel.remove(removeIndex)
            }
        }

        for (var sourceIndex = 0; sourceIndex < sourceItems.length; ++sourceIndex) {
            var sourceItem = sourceItems[sourceIndex]
            var filteredIndex = _findItemByOrigIndex(sourceItem.origIndex)
            if (filteredIndex >= 0) {
                _updateFilteredItem(filteredIndex, sourceItem)
            } else {
                filteredModel.append(sourceItem)
            }
        }
    }

    function _handleDataChanged(topLeft, bottomRight) {
        var changedStart = topLeft !== undefined && topLeft !== null && topLeft.row !== undefined ? topLeft.row : topLeft
        var changedEnd = bottomRight !== undefined && bottomRight !== null && bottomRight.row !== undefined ? bottomRight.row : bottomRight

        if (changedStart === undefined || changedEnd === undefined) {
            rebuild()
            return
        }

        totalCount = _sourceCount()

        for (var sourceIndex = changedStart; sourceIndex <= changedEnd; ++sourceIndex) {
            var sourceItem = _getSourceItemAtIndex(sourceIndex)
            var filteredIndex = _findItemByOrigIndex(sourceIndex)

            if (sourceItem && _shouldIncludeItem(sourceItem)) {
                if (filteredIndex >= 0) {
                    _updateFilteredItem(filteredIndex, sourceItem)
                } else {
                    filteredModel.insert(_findInsertPositionByOrigIndex(sourceIndex), _createFilteredItem(sourceIndex, sourceItem))
                }
            } else if (filteredIndex >= 0) {
                filteredModel.remove(filteredIndex)
            }
        }
    }

    function _handleRowsInserted(first, last) {
        var insertedCount = last - first + 1
        totalCount = _sourceCount()

        for (var filteredIndex = 0; filteredIndex < filteredModel.count; ++filteredIndex) {
            var origIndex = filteredModel.get(filteredIndex).origIndex
            if (origIndex >= first) {
                filteredModel.setProperty(filteredIndex, "origIndex", origIndex + insertedCount)
            }
        }

        for (var sourceIndex = first; sourceIndex <= last; ++sourceIndex) {
            var sourceItem = _getSourceItemAtIndex(sourceIndex)
            if (sourceItem && _shouldIncludeItem(sourceItem) && _findItemByOrigIndex(sourceIndex) < 0) {
                filteredModel.insert(_findInsertPositionByOrigIndex(sourceIndex), _createFilteredItem(sourceIndex, sourceItem))
            }
        }
    }

    function _handleRowsRemoved(first, last) {
        totalCount = _sourceCount()

        for (var filteredIndex = filteredModel.count - 1; filteredIndex >= 0; --filteredIndex) {
            var item = filteredModel.get(filteredIndex)
            if (item.origIndex >= first && item.origIndex <= last) {
                filteredModel.remove(filteredIndex)
            } else if (item.origIndex > last) {
                filteredModel.setProperty(filteredIndex, "origIndex", item.origIndex - (last - first + 1))
            }
        }
    }

    function _sourceCount() {
        if (!mediaModel) {
            return 0
        }
        if (_isQmlListModel()) {
            return mediaModel.count
        }
        if (_isArray()) {
            return mediaModel.length
        }
        if (_isCppModel()) {
            return mediaModel.rowCount()
        }
        return 0
    }

    function _isQmlListModel() {
        return !!mediaModel && typeof mediaModel.get === "function" && mediaModel.count !== undefined
    }

    function _isCppModel() {
        return !!mediaModel && typeof mediaModel.rowCount === "function" && typeof mediaModel.data === "function"
    }

    function _isArray() {
        return !!mediaModel && (Array.isArray(mediaModel) || (typeof mediaModel.length === "number" && typeof mediaModel.get !== "function"))
    }

    function _processedThumbnailIds() {
        if (!_isCppModel() || _sourceCount() <= 0) {
            return []
        }
        return mediaModel.data(mediaModel.index(0, 0), 276) || []
    }

    function _shouldIncludeItem(item) {
        if (onlyCompleted) {
            return item.status === 2
        }
        if (!showFailedFiles) {
            return item.status === 0 || item.status === 1 || item.status === 2 || item.status === 5 || item.status === 6
        }
        return true
    }

    function _createFilteredItem(origIdx, item) {
        return {
            "origIndex": origIdx,
            "itemId": item.itemId || item.id || "",
            "filePath": item.filePath || "",
            "fileName": item.fileName || "",
            "mediaType": item.mediaType !== undefined ? item.mediaType : 0,
            "thumbnail": item.thumbnail || "",
            "status": item.status !== undefined ? item.status : 0,
            "resultPath": item.resultPath || "",
            "originalPath": item.originalPath || "",
            "processedThumbnailId": item.processedThumbnailId || ""
        }
    }

    function _updateFilteredItem(filteredIdx, item) {
        _updateItemProperty(filteredIdx, "itemId", item.itemId || item.id || "")
        _updateItemProperty(filteredIdx, "filePath", item.filePath || "")
        _updateItemProperty(filteredIdx, "fileName", item.fileName || "")
        _updateItemProperty(filteredIdx, "mediaType", item.mediaType !== undefined ? item.mediaType : 0)
        _updateItemProperty(filteredIdx, "thumbnail", item.thumbnail || "")
        _updateItemProperty(filteredIdx, "status", item.status !== undefined ? item.status : 0)
        _updateItemProperty(filteredIdx, "resultPath", item.resultPath || "")
        _updateItemProperty(filteredIdx, "originalPath", item.originalPath || "")
        _updateItemProperty(filteredIdx, "processedThumbnailId", item.processedThumbnailId || "")
    }

    function _updateItemProperty(filteredIdx, key, value) {
        if (filteredModel.get(filteredIdx)[key] !== value) {
            filteredModel.setProperty(filteredIdx, key, value)
        }
    }

    function _findItemByOrigIndex(origIdx) {
        for (var i = 0; i < filteredModel.count; ++i) {
            if (filteredModel.get(i).origIndex === origIdx) {
                return i
            }
        }
        return -1
    }

    function _findInsertPositionByOrigIndex(origIdx) {
        for (var i = 0; i < filteredModel.count; ++i) {
            if (filteredModel.get(i).origIndex > origIdx) {
                return i
            }
        }
        return filteredModel.count
    }

    function _getSourceItemAtIndex(sourceIdx) {
        if (!mediaModel || sourceIdx < 0) {
            return null
        }

        var item = null
        if (_isQmlListModel() && sourceIdx < mediaModel.count) {
            item = mediaModel.get(sourceIdx)
        } else if (_isArray() && sourceIdx < mediaModel.length) {
            item = mediaModel[sourceIdx]
        } else if (_isCppModel() && sourceIdx < mediaModel.rowCount()) {
            var processedThumbnailIds = _processedThumbnailIds()
            var idx = mediaModel.index(sourceIdx, 0)
            item = {
                filePath: mediaModel.data(idx, 258) || "",
                fileName: mediaModel.data(idx, 259) || "",
                mediaType: mediaModel.data(idx, 262),
                thumbnail: mediaModel.data(idx, 263) || "",
                status: mediaModel.data(idx, 266),
                resultPath: mediaModel.data(idx, 267) || "",
                originalPath: mediaModel.data(idx, 258) || "",
                processedThumbnailId: processedThumbnailIds[sourceIdx] || ""
            }
        }

        return item
    }

    function _getSourceItems() {
        var items = []
        var count = _sourceCount()
        for (var i = 0; i < count; ++i) {
            var item = _getSourceItemAtIndex(i)
            if (item && _shouldIncludeItem(item)) {
                items.push(_createFilteredItem(i, item))
            }
        }
        return items
    }

    Connections {
        target: root._isQmlListModel() || root._isCppModel() ? root.mediaModel : null
        function onDataChanged(topLeft, bottomRight) { root._handleDataChanged(topLeft, bottomRight) }
        function onRowsInserted(parent, first, last) { root._handleRowsInserted(first, last) }
        function onRowsRemoved(parent, first, last) { root._handleRowsRemoved(first, last) }
        function onModelReset() { root.rebuild() }
    }

    Component.onCompleted: rebuild()
}
