import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

Item {
    id: root

    visible: false
    z: 9999

    property var previewData: null
    property string targetMessageId: ""
    property var targetMessageIds: []

    readonly property bool isBatchMode: targetMessageIds.length > 1

    property bool deleteResultFiles: true
    property bool deleteThumbnails: true

    signal confirmed(var options)
    signal cancelled()

    function showSingle(messageId) {
        targetMessageId = messageId
        targetMessageIds = [messageId]
        if (typeof messageModel !== "undefined" && messageModel) {
            previewData = messageModel.getDeletionPreview(messageId)
        }
        _resetAndShow()
    }

    function showBatch(ids) {
        targetMessageId = ""
        targetMessageIds = ids
        if (typeof messageModel !== "undefined" && messageModel && ids.length > 0) {
            previewData = messageModel.getBatchDeletionPreview(ids)
        }
        _resetAndShow()
    }

    function _resetAndShow() {
        deleteResultFiles = true
        deleteThumbnails = true
        resultFilesExpanded = false
        thumbnailsExpanded = false
        dragOffset = Qt.point(0, 0)
        visible = true
        opacity = 0
        dialogRect.scale = 0.92
        showAnimation.start()
    }

    function hide() {
        hideAnimation.start()
    }

    property bool resultFilesExpanded: false
    property bool thumbnailsExpanded: false

    property point dragOffset: Qt.point(0, 0)
    property bool isDragging: false

    ParallelAnimation {
        id: showAnimation
        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { target: dialogRect; property: "scale"; from: 0.92; to: 1; duration: 250; easing.type: Easing.OutCubic }
    }

    ParallelAnimation {
        id: hideAnimation
        NumberAnimation { target: root; property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { target: dialogRect; property: "scale"; from: 1; to: 0.95; duration: 150; easing.type: Easing.InCubic }
        onFinished: { visible = false; cancelled() }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.7 : 0.5)

        MouseArea {
            anchors.fill: parent
            onClicked: root.hide()
        }
    }

    Rectangle {
        id: dialogRect
        x: __clampedCenterX + dragOffset.x
        y: __clampedCenterY + dragOffset.y
        width: Math.min(480, parent.width - 48)
        implicitHeight: Math.min(contentColumn.implicitHeight + 36, parent.height * 0.85)
        radius: Theme.radius.xl
        color: Theme.colors.card
        border.color: Theme.colors.cardBorder
        border.width: 1

        readonly property real __clampedCenterX: {
            var cx = (parent.width - width) / 2
            return Math.max(0, Math.min(cx, parent.width - width))
        }

        readonly property real __clampedCenterY: {
            var cy = (parent.height - implicitHeight) / 2
            return Math.max(0, Math.min(cy, parent.height - implicitHeight))
        }

        Flickable {
            id: contentFlickable
            anchors.fill: parent
            anchors.margins: 18
            contentHeight: contentColumn.implicitHeight
            contentWidth: width
            clip: true
            interactive: contentHeight > height

            ColumnLayout {
                id: contentColumn
                width: contentFlickable.contentWidth
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        width: 36; height: 36
                        radius: Theme.radius.lg
                        color: Theme.isDark ? Qt.rgba(239, 68, 68, 0.2) : Qt.rgba(239, 68, 68, 0.15)

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("alert-triangle")
                            iconSize: 18
                            color: Theme.colors.destructive
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("确认删除")
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: Theme.colors.foreground
                    }

                    IconButton {
                        iconName: "x"
                        iconSize: 12
                        btnSize: 24
                        onClicked: root.hide()
                    }
                }

                MouseArea {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    cursorShape: Qt.SizeAllCursor

                    property point lastMousePos: Qt.point(0, 0)

                    onPressed: function(mouse) {
                        lastMousePos = Qt.point(mouse.x, mouse.y)
                        isDragging = true
                    }
                    onReleased: { isDragging = false }
                    onPositionChanged: function(mouse) {
                        if (isDragging) {
                            var offsetX = dragOffset.x + mouse.x - lastMousePos.x
                            var offsetY = dragOffset.y + mouse.y - lastMousePos.y
                            var maxOffsetX = root.parent.width - dialogRect.width - dialogRect.__clampedCenterX
                            var maxOffsetY = root.parent.height - dialogRect.implicitHeight - dialogRect.__clampedCenterY
                            dragOffset = Qt.point(
                                Math.max(-dialogRect.__clampedCenterX, Math.min(offsetX, maxOffsetX)),
                                Math.max(-dialogRect.__clampedCenterY, Math.min(offsetY, maxOffsetY))
                            )
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        if (!previewData || !previewData.found) return qsTr("将删除此任务的相关数据，请选择需要删除的内容：")
                        var mode = previewData.modeName || ""
                        if (isBatchMode) {
                            return qsTr("将删除 %1 条%2任务的相关数据，请选择需要删除的内容：").arg(targetMessageIds.length).arg(mode ? mode : "")
                        }
                        return qsTr("将删除此%1任务的相关数据，请选择需要删除的内容：").arg(mode ? mode : "")
                    }
                    font.pixelSize: 13
                    color: Theme.colors.mutedForeground
                    wrapMode: Text.Wrap
                    lineHeight: 1.5
                    leftPadding: 48
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 48
                    height: 1
                    color: Theme.colors.border
                    visible: previewData && previewData.isProcessing === true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 48
                    spacing: 6
                    visible: previewData && previewData.isProcessing === true

                    ColoredIcon {
                        source: Theme.icon("info")
                        iconSize: 14
                        color: Theme.colors.warning
                    }

                    Text {
                        Layout.fillWidth: true
                        text: previewData ? (previewData.cancelHint || "") : ""
                        font.pixelSize: 12
                        color: Theme.colors.warning
                        wrapMode: Text.Wrap
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 48
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Switch {
                            checked: true
                            enabled: false
                            opacity: 0.7
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("任务记录")
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: Theme.colors.foreground
                            }
                            Text {
                                Layout.fillWidth: true
                                text: previewData && previewData.taskRecord ? previewData.taskRecord.description : ""
                                font.pixelSize: 11
                                color: Theme.colors.mutedForeground
                                wrapMode: Text.Wrap
                            }
                        }

                        Rectangle {
                            width: 56; height: 22
                            radius: Theme.radius.md
                            color: Theme.isDark ? Qt.rgba(100, 116, 139, 0.2) : Qt.rgba(100, 116, 139, 0.1)

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("必删")
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: Theme.colors.mutedForeground
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.colors.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Switch {
                                id: resultFilesSwitch
                                checked: root.deleteResultFiles
                                onToggled: root.deleteResultFiles = checked
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("处理结果文件")
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: Theme.colors.foreground
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        if (!previewData || !previewData.resultFiles) return ""
                                        var rf = previewData.resultFiles
                                        return rf.description || ""
                                    }
                                    font.pixelSize: 11
                                    color: Theme.colors.mutedForeground
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 48
                            spacing: 8
                            visible: previewData && previewData.resultFiles && previewData.resultFiles.count > 0

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    if (!previewData || !previewData.resultFiles) return ""
                                    var rf = previewData.resultFiles
                                    return qsTr("%1 个文件，共 %2").arg(rf.count).arg(rf.totalSizeText || "0 B")
                                }
                                font.pixelSize: 11
                                color: Theme.colors.mutedForeground
                            }

                            Text {
                                text: root.resultFilesExpanded ? qsTr("收起") : qsTr("查看详情")
                                font.pixelSize: 11
                                color: Theme.colors.primary
                                font.underline: true

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.resultFilesExpanded = !root.resultFilesExpanded
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 48
                            visible: root.resultFilesExpanded && previewData && previewData.resultFiles
                            spacing: 2

                            Repeater {
                                model: (root.resultFilesExpanded && previewData && previewData.resultFiles) ? previewData.resultFiles.files : []

                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    ColoredIcon {
                                        source: Theme.icon("file")
                                        iconSize: 12
                                        color: Theme.colors.mutedForeground
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.path || ""
                                        font.pixelSize: 10
                                        font.family: "Consolas, monospace"
                                        color: Theme.colors.mutedForeground
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 2
                                    }

                                    Text {
                                        text: modelData.sizeText || ""
                                        font.pixelSize: 10
                                        color: Theme.colors.mutedForeground
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.colors.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Switch {
                                id: thumbnailsSwitch
                                checked: root.deleteThumbnails
                                onToggled: root.deleteThumbnails = checked
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("缩略图缓存")
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: Theme.colors.foreground
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        if (!previewData || !previewData.thumbnails) return ""
                                        return previewData.thumbnails.description || ""
                                    }
                                    font.pixelSize: 11
                                    color: Theme.colors.mutedForeground
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 48
                            spacing: 8
                            visible: previewData && previewData.thumbnails && previewData.thumbnails.count > 0

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    if (!previewData || !previewData.thumbnails) return ""
                                    return qsTr("%1 个缩略图").arg(previewData.thumbnails.count)
                                }
                                font.pixelSize: 11
                                color: Theme.colors.mutedForeground
                            }

                            Text {
                                visible: !isBatchMode && previewData && previewData.thumbnails && previewData.thumbnails.items
                                text: root.thumbnailsExpanded ? qsTr("收起") : qsTr("查看详情")
                                font.pixelSize: 11
                                color: Theme.colors.primary
                                font.underline: true

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.thumbnailsExpanded = !root.thumbnailsExpanded
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 48
                            visible: root.thumbnailsExpanded && previewData && previewData.thumbnails && previewData.thumbnails.items
                            spacing: 2

                            Repeater {
                                model: (root.thumbnailsExpanded && previewData && previewData.thumbnails && previewData.thumbnails.items) ? previewData.thumbnails.items : []

                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    ColoredIcon {
                                        source: Theme.icon("image")
                                        iconSize: 12
                                        color: Theme.colors.mutedForeground
                                    }

                                    Text {
                                        text: modelData.type || ""
                                        font.pixelSize: 10
                                        color: Theme.colors.mutedForeground
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.id || ""
                                        font.pixelSize: 10
                                        font.family: "Consolas, monospace"
                                        color: Theme.colors.mutedForeground
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 2
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }
                        }
                    }
                }

                Item { height: 4 }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    Button {
                        variant: "secondary"
                        text: qsTr("取消")
                        size: "sm"
                        onClicked: root.hide()
                    }

                    Button {
                        variant: "destructive"
                        text: qsTr("确认删除")
                        size: "sm"
                        onClicked: {
                            var options = {
                                "deleteResultFiles": root.deleteResultFiles,
                                "deleteThumbnails": root.deleteThumbnails
                            }
                            confirmed(options)
                            hideAnimation.start()
                        }
                    }
                }
            }
        }
    }
}
