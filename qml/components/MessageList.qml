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

            Component.onCompleted: {
                _buildMediaForDelegate()
            }

            // 当 C++ 端 mediaFiles 角色数据变化时重新构建
            function _buildMediaForDelegate() {
                _cachedMedia.clear()
                if (root._hasRealModel && model.mediaFiles) {
                    // 真实数据：从 C++ MessageModel.MediaFilesRole 获取 QVariantList
                    var files = model.mediaFiles
                    for (var i = 0; i < files.length; i++) {
                        var f = files[i]
                        _cachedMedia.append({
                            "filePath":  f.filePath  || "",
                            "fileName":  f.fileName  || "",
                            "mediaType": f.mediaType !== undefined ? f.mediaType : 0,
                            "thumbnail": f.filePath ? ("image://thumbnail/" + f.filePath) : "",
                            "status":    f.status    !== undefined ? f.status : 0,
                            "resultPath": f.resultPath || ""
                        })
                    }
                } else {
                    // Demo 模式
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
                console.log("删除文件:", model.id, "索引:", idx)
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
            // 从 C++ MessageModel 获取已完成的文件
            var completedFiles = messageModel.getCompletedFiles(msgId)
            for (var i = 0; i < completedFiles.length; i++) {
                var cf = completedFiles[i]
                files.push({
                    "filePath":  cf.resultPath || cf.filePath || "",
                    "fileName":  cf.fileName  || "",
                    "mediaType": cf.mediaType !== undefined ? cf.mediaType : 0,
                    "thumbnail": cf.filePath ? ("image://thumbnail/" + cf.filePath) : "",
                    "resultPath": cf.resultPath || "",
                    "originalPath": cf.filePath || ""
                })
            }
        } else {
            for (var j = 0; j < 10; j++) {
                files.push({
                    "filePath": "",
                    "fileName": "media_" + (j+1) + ".jpg",
                    "mediaType": 0,
                    "thumbnail": "",
                    "resultPath": ""
                })
            }
        }
        if (files.length > 0) {
            viewerWindow.mediaFiles = files
            viewerWindow.openAt(Math.min(fileIndex, files.length - 1))
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
        // TODO: 打开编辑对话框
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
