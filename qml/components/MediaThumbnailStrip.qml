import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
 */
Item {
    id: root

    // ========== 属性定义 ==========
    /** @brief 媒体文件列表模型，每项需包含: filePath, fileName, mediaType, thumbnail, status, resultPath */
    property var mediaModel: ListModel {}

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
        enabled: mediaModel && typeof mediaModel.dataChanged !== "undefined"
        
        function onDataChanged(topLeft, topLeftRight) {
            _rebuildFiltered()
        }
    }

    function _rebuildFiltered() {
        filteredModel.clear()
        if (!mediaModel) return
        
        // 检查是否是 QAbstractListModel（有 data 方法）
        if (typeof mediaModel.data === "function" && typeof mediaModel.index === "function") {
            // C++ QAbstractListModel
            for (var i = 0; i < mediaModel.rowCount(); i++) {
                var idx = mediaModel.index(i, 0)  // 修复：添加 column 参数
                // 逐个获取角色值（FileModel 角色定义）
                var filePath = mediaModel.data(idx, 258)    // FilePathRole = 258
                var fileName = mediaModel.data(idx, 259)    // FileNameRole = 259
                var mediaType = mediaModel.data(idx, 262)   // MediaTypeRole = 262
                var thumbnail = mediaModel.data(idx, 263)   // ThumbnailRole = 263
                var status = mediaModel.data(idx, 266)      // StatusRole = 266
                var resultPath = mediaModel.data(idx, 267)  // ResultPathRole = 267
                
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
        } else {
            // QML ListModel
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
        }
    }

    // ========== 主布局 ==========
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
                flickDeceleration: 1500
                maximumFlickVelocity: 2000

                // 限制显示数量（收缩模式）
                model: {
                    if (root.expandable && !root.expanded) {
                        // 创建一个截断视图
                        return Math.min(filteredModel.count, root.collapsedMaxVisible)
                    }
                    return filteredModel.count
                }

                delegate: Item {
                    id: thumbDelegate
                    width: root.thumbSize
                    height: root.thumbSize

                    required property int index
                    property var itemData: index < filteredModel.count ? filteredModel.get(index) : null

                    // 缩略图容器
                    Rectangle {
                        id: thumbRect
                        anchors.fill: parent
                        radius: Theme.radius.md
                        color: thumbMouse.containsMouse ? Theme.colors.surfaceHover : Theme.colors.surface
                        border.width: 1
                        border.color: thumbMouse.containsMouse ? Theme.colors.primary : Theme.colors.border
                        clip: true

                        // 缩略图图片
                        Image {
                            id: thumbImage
                            anchors.fill: parent
                            anchors.margins: 2
                            source: {
                                if (!thumbDelegate.itemData) return ""
                                var path = thumbDelegate.itemData.thumbnail
                                if (path && path !== "") return path
                                // 如果有 filePath，通过 ThumbnailProvider 加载
                                var fp = thumbDelegate.itemData.filePath
                                if (fp && fp !== "") return "image://thumbnail/" + fp
                                return ""
                            }
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            smooth: true
                            sourceSize: Qt.size(root.thumbSize * 2, root.thumbSize * 2)
                            visible: status === Image.Ready
                        }

                        // 占位图标（图片未加载时）
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: {
                                if (!thumbDelegate.itemData) return Theme.icon("image")
                                return thumbDelegate.itemData.mediaType === 1 ? Theme.icon("video") : Theme.icon("image")
                            }
                            iconSize: 24
                            color: Theme.colors.mutedForeground
                            visible: thumbImage.status !== Image.Ready
                        }

                        // 视频标识角标
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

                        // 处理状态指示（未完成的文件，半透明遮罩）
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: Qt.rgba(0, 0, 0, 0.5)
                            visible: thumbDelegate.itemData && thumbDelegate.itemData.status !== 2 && root.messageMode

                            ColoredIcon {
                                anchors.centerIn: parent
                                source: thumbDelegate.itemData && thumbDelegate.itemData.status === 1
                                        ? Theme.icon("loader") : Theme.icon("loader")
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

                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
                    }

                    // 鼠标交互区域
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

                    // 悬停删除按钮
                    Rectangle {
                        id: deleteBtn
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 2
                        width: 18; height: 18
                        radius: 9
                        color: deleteBtnMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.6)
                        visible: thumbMouse.containsMouse
                        z: 10

                        Text {
                            anchors.centerIn: parent
                            text: "\u00D7"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: deleteBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.deleteFile(thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index)
                        }

                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }

                // 垂直滚轮 -> 水平滚动
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    acceptedButtons: Qt.NoButton
                    onWheel: function(wheel) {
                        var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                        var step = delta * 0.5
                        var newX = thumbListView.contentX - step
                        var maxX = Math.max(0, thumbListView.contentWidth - thumbListView.width)
                        thumbListView.contentX = Math.max(0, Math.min(newX, maxX))
                        wheel.accepted = true
                    }
                }
            }

            // 空状态
            Text {
                anchors.centerIn: parent
                visible: filteredModel.count === 0
                text: root.onlyCompleted ? qsTr("暂无已完成的文件") : qsTr("暂无文件")
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
            }
        }

        // ========== 展开/收缩按钮 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            visible: root.expandable && filteredModel.count > root.collapsedMaxVisible
            color: "transparent"

            Row {
                anchors.centerIn: parent
                spacing: 6

                // 展开/收缩图标（6点网格图标）
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u2237"
                    color: Theme.colors.primary
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.expanded
                          ? qsTr("收缩文件列表")
                          : qsTr("展开查看全部 %1 个文件（可拖拽或滚轮滑动）").arg(filteredModel.count)
                    color: Theme.colors.primary
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                ColoredIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    source: root.expanded ? Theme.icon("chevron-up") : Theme.icon("chevron-down")
                    iconSize: 14
                    color: Theme.colors.primary
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }
    }

    // ========== 右键上下文菜单 ==========
    Menu {
        id: contextMenu
        property int targetIndex: -1

        width: 140

        background: Rectangle {
            color: Theme.colors.popover
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.md
        }

        MenuItem {
            text: qsTr("放大查看")
            icon.source: Theme.icon("eye")
            onTriggered: {
                if (contextMenu.targetIndex >= 0) {
                    var item = filteredModel.get(contextMenu.targetIndex)
                    root.viewFile(item ? item.origIndex : contextMenu.targetIndex)
                }
            }

            background: Rectangle {
                color: parent.highlighted ? Theme.colors.surfaceHover : "transparent"
                radius: Theme.radius.sm
            }

            contentItem: Row {
                spacing: 8
                leftPadding: 8

                ColoredIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    source: Theme.icon("eye")
                    iconSize: 14
                    color: Theme.colors.foreground
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("放大查看")
                    color: Theme.colors.foreground
                    font.pixelSize: 13
                }
            }
        }

        MenuItem {
            text: qsTr("保存")

            background: Rectangle {
                color: parent.highlighted ? Theme.colors.surfaceHover : "transparent"
                radius: Theme.radius.sm
            }

            contentItem: Row {
                spacing: 8
                leftPadding: 8

                ColoredIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    source: Theme.icon("save")
                    iconSize: 14
                    color: Theme.colors.foreground
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("保存")
                    color: Theme.colors.foreground
                    font.pixelSize: 13
                }
            }

            onTriggered: {
                if (contextMenu.targetIndex >= 0) {
                    var item = filteredModel.get(contextMenu.targetIndex)
                    root.saveFile(item ? item.origIndex : contextMenu.targetIndex)
                }
            }
        }

        MenuSeparator {
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.colors.border
            }
        }

        MenuItem {
            text: qsTr("删除")

            background: Rectangle {
                color: parent.highlighted ? Theme.colors.destructiveSubtle : "transparent"
                radius: Theme.radius.sm
            }

            contentItem: Row {
                spacing: 8
                leftPadding: 8

                ColoredIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    source: Theme.icon("trash")
                    iconSize: 14
                    color: Theme.colors.destructive
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("删除")
                    color: Theme.colors.destructive
                    font.pixelSize: 13
                }
            }

            onTriggered: {
                if (contextMenu.targetIndex >= 0) {
                    var item = filteredModel.get(contextMenu.targetIndex)
                    root.deleteFile(item ? item.origIndex : contextMenu.targetIndex)
                }
            }
        }
    }
}
