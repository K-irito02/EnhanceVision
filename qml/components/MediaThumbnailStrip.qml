import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"
import "../controls"

/**
 * @brief 可复用的水平缩略图条组件
 * 
 * 支持功能：
 * - 缩略图水平排列显示
 * - 鼠标拖拽左右滑动
 * - 鼠标滚轮左右滑动
 * - 悬停显示删除按钮
 * - 右键上下文菜单（放大查看、保存、删除）
 * - 左键点击触发放大查看
 * - 可选展开/收缩功能
 * - 删除文件时同步更新
 */
Item {
    id: root

    // ========== 属性定义 ==========
    /** @brief 媒体文件列表模型，每项需包含: filePath, fileName, mediaType, thumbnail, status, resultPath */
    property var mediaModel: ListModel {}

    /** @brief 所属消息ID（用于删除同步） */
    property string messageId: ""

    /** @brief 缩略图尺寸 */
    property int thumbSize: 80

    /** @brief 缩略图间距 */
    property int thumbSpacing: 8

    /** @brief 是否显示展开/收缩按钮 */
    property bool expandable: false

    /** @brief 当前是否展开（显示所有文件） */
    property bool expanded: false

    /** @brief 收缩时默认显示的最大数量（超出则提示展开） */
    property int collapsedMaxVisible: 10

    /** @brief 是否只显示已完成的文件（用于消息区域） */
    property bool onlyCompleted: false

    /** @brief 是否为消息模式（支持原图对比） */
    property bool messageMode: false

    /** @brief 总文件数（包括未完成的） */
    property int totalCount: mediaModel ? mediaModel.count : 0

    /** @brief 可见文件数（根据 onlyCompleted 筛选后的数量） */
    readonly property int visibleCount: filteredModel.count

    // ========== 信号 ==========
    signal viewFile(int index)
    signal saveFile(int index)
    signal deleteFile(int index)
    signal fileRemoved(string messageId, int fileIndex)

    // ========== 尺寸 ==========
    implicitHeight: contentCol.implicitHeight

    // ========== 内部过滤模型 ==========
    ListModel {
        id: filteredModel
    }

    // 监听源模型变化，更新过滤模型
    onMediaModelChanged: _rebuildFiltered()
    onOnlyCompletedChanged: _rebuildFiltered()
    Component.onCompleted: _rebuildFiltered()
    
    // 监听 C++ QAbstractListModel 的数据变化
    Connections {
        target: mediaModel
        function onDataChanged(topLeft, bottomRight, roles) { _rebuildFiltered() }
        function onRowsInserted(parent, first, last) { _rebuildFiltered() }
        function onRowsRemoved(parent, first, last) { _handleRowsRemoved(first, last) }
        function onModelReset() { _rebuildFiltered() }
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

    function _rebuildFiltered() {
        filteredModel.clear()
        if (!mediaModel) return
        
        // 检查是否是 QML ListModel（有 get 方法和 count 属性）
        var isQmlListModel = (typeof mediaModel.get === "function") && (mediaModel.count !== undefined)
        var isCppModel = (typeof mediaModel.rowCount === "function") && (typeof mediaModel.data === "function")
        
        if (isQmlListModel) {
            for (var i = 0; i < mediaModel.count; i++) {
                var item = mediaModel.get(i)
                if (!onlyCompleted || item.status === 2) {
                    filteredModel.append({
                        "origIndex": i,
                        "filePath": item.filePath || "",
                        "fileName": item.fileName || "",
                        "mediaType": item.mediaType !== undefined ? item.mediaType : 0,
                        "thumbnail": item.thumbnail || "",
                        "status": item.status !== undefined ? item.status : 0,
                        "resultPath": item.resultPath || ""
                    })
                }
            }
        } else if (isCppModel) {
            for (var i = 0; i < mediaModel.rowCount(); i++) {
                var idx = mediaModel.index(i, 0)
                var filePath = mediaModel.data(idx, 258)
                var fileName = mediaModel.data(idx, 259)
                var mediaType = mediaModel.data(idx, 262)
                var thumbnail = mediaModel.data(idx, 263)
                var status = mediaModel.data(idx, 266)
                var resultPath = mediaModel.data(idx, 267)
                
                if (!onlyCompleted || status === 2) {
                    filteredModel.append({
                        "origIndex": i,
                        "filePath": filePath || "",
                        "fileName": fileName || "",
                        "mediaType": mediaType !== undefined ? mediaType : 0,
                        "thumbnail": thumbnail || "",
                        "status": status !== undefined ? status : 0,
                        "resultPath": resultPath || ""
                    })
                }
            }
        }
    }

    ColumnLayout {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        // ========== 缩略图滚动区域 ==========
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: root.thumbSize
            visible: filteredModel.count > 0
            clip: true

            ListView {
                id: thumbListView
                anchors.fill: parent
                orientation: ListView.Horizontal
                spacing: root.thumbSpacing
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                model: root.expandable && !root.expanded ? Math.min(filteredModel.count, root.collapsedMaxVisible) : filteredModel.count

                delegate: Item {
                    id: thumbDelegate
                    width: root.thumbSize
                    height: root.thumbSize
                    required property int index
                    property var itemData: index < filteredModel.count ? filteredModel.get(index) : null
                    property bool showDeleteBtn: thumbMouse.containsMouse || deleteBtnMouse.containsMouse

                    Rectangle {
                        id: hoverBorder
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: Theme.radius.md
                        color: "transparent"
                        border.width: 2
                        border.color: thumbMouse.containsMouse ? Theme.colors.primary : "transparent"
                        z: 5
                        
                        Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
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
                            source: {
                                if (!thumbDelegate.itemData) return ""
                                var path = thumbDelegate.itemData.thumbnail
                                if (path && path !== "") return path
                                var fp = thumbDelegate.itemData.filePath
                                if (fp && fp !== "") return "image://thumbnail/" + fp
                                return ""
                            }
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            smooth: true
                            sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)
                            visible: status === Image.Ready
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
                            layer.samples: 4
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
                            visible: thumbDelegate.itemData && thumbDelegate.itemData.mediaType === 1
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
                                color: "#FFFFFF"
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: Qt.rgba(0, 0, 0, 0.5)
                            visible: thumbDelegate.itemData && thumbDelegate.itemData.status !== 2 && root.messageMode
                            ColoredIcon {
                                anchors.centerIn: parent
                                source: Theme.icon("loader")
                                iconSize: 20
                                color: "#FFFFFF"
                                opacity: 0.8
                                RotationAnimation on rotation {
                                    from: 0; to: 360
                                    duration: 1500
                                    loops: Animation.Infinite
                                    running: thumbDelegate.itemData && thumbDelegate.itemData.status === 1
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: thumbMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                contextMenu.targetIndex = thumbDelegate.index
                                contextMenu.popup()
                            } else {
                                root.viewFile(thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index)
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
                        visible: thumbDelegate.showDeleteBtn
                        z: 10
                        
                        opacity: visible ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 100 } }
                        
                        Text {
                            anchors.centerIn: parent
                            text: "\u00D7"
                            color: "#FFFFFF"
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
                }

                MouseArea {
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.NoButton
                    onWheel: function(wheel) {
                        var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                        thumbListView.contentX = Math.max(0, Math.min(thumbListView.contentX - delta * 0.5, 
                            Math.max(0, thumbListView.contentWidth - thumbListView.width)))
                        wheel.accepted = true
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: filteredModel.count === 0
                text: root.onlyCompleted ? qsTr("暂无已完成的文件") : qsTr("暂无文件")
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
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
                Text { anchors.verticalCenter: parent.verticalCenter; text: root.expanded ? qsTr("收缩文件列表") : qsTr("展开查看全部 %1 个文件").arg(filteredModel.count); color: Theme.colors.primary; font.pixelSize: 12; font.weight: Font.Medium }
                ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: root.expanded ? Theme.icon("chevron-up") : Theme.icon("chevron-down"); iconSize: 14; color: Theme.colors.primary }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.expanded = !root.expanded }
        }
    }

    Menu {
        id: contextMenu
        property int targetIndex: -1
        width: 140
        background: Rectangle { color: Theme.colors.popover; border.width: 1; border.color: Theme.colors.border; radius: Theme.radius.md }

        MenuItem {
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
            text: qsTr("保存")
            background: Rectangle { color: parent.highlighted ? Theme.colors.surfaceHover : "transparent"; radius: Theme.radius.sm }
            contentItem: Row {
                spacing: 8; leftPadding: 8
                ColoredIcon { anchors.verticalCenter: parent.verticalCenter; source: Theme.icon("save"); iconSize: 14; color: Theme.colors.foreground }
                Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("保存"); color: Theme.colors.foreground; font.pixelSize: 13 }
            }
            onTriggered: { if (contextMenu.targetIndex >= 0) { var item = filteredModel.get(contextMenu.targetIndex); root.saveFile(item ? item.origIndex : contextMenu.targetIndex) } }
        }

        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.colors.border } }

        MenuItem {
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
