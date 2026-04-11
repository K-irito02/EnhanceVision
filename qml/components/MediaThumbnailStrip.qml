import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"
import "mediaViewer"

Item {
    id: root

    property var mediaModel: null
    property string messageId: ""
    property int thumbSize: 80
    property int thumbSpacing: 8
    property bool expandable: false
    property bool expanded: false
    property int collapsedMaxVisible: 10
    property bool onlyCompleted: false
    property bool showFailedFiles: false
    property bool messageMode: false
    property bool messagePaused: false
    property int totalCount: adapter.totalCount
    readonly property int visibleCount: adapter.model.count

    signal viewFile(int index)
    signal saveFile(int index)
    signal deleteFile(int index)
    signal retryFailedFile(int index)

    implicitHeight: contentLoader.implicitHeight

    MediaThumbnailAdapter {
        id: adapter
        mediaModel: root.mediaModel
        onlyCompleted: root.onlyCompleted
        showFailedFiles: root.showFailedFiles
    }

    Loader {
        id: contentLoader
        anchors.left: parent.left
        anchors.right: parent.right
        sourceComponent: root.expandable && root.expanded ? gridComponent : horizontalComponent
    }

    MediaThumbnailContextMenu {
        id: contextMenu
        messageMode: root.messageMode
        model: adapter.model
        onViewRequested: function(origIndex) { root.viewFile(origIndex) }
        onSaveRequested: function(origIndex) { root.saveFile(origIndex) }
        onRetryRequested: function(origIndex) { root.retryFailedFile(origIndex) }
        onDeleteRequested: function(origIndex) { root.deleteFile(origIndex) }
    }

    Component {
        id: horizontalComponent

        ColumnLayout {
            spacing: 6

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.thumbSize
                visible: adapter.model.count > 0
                clip: true

                ListView {
                    id: thumbListView
                    anchors.fill: parent
                    orientation: ListView.Horizontal
                    spacing: root.thumbSpacing
                    model: adapter.model
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentWidth > width
                    cacheBuffer: 500

                    delegate: MediaThumbnailDelegate {
                        width: root.thumbSize
                        height: root.thumbSize
                        messageMode: root.messageMode
                        contextMenuEnabled: true
                        requestedSourceSize: root.thumbSize * 2
                        onActivated: function(origIndex) { root.viewFile(origIndex) }
                        onDeleteRequested: function(origIndex) { root.deleteFile(origIndex) }
                        onContextMenuRequested: function(globalX, globalY, filteredIndex, itemStatus) {
                            contextMenu.targetIndex = filteredIndex
                            contextMenu.fileStatus = itemStatus
                            contextMenu.showAt(globalX, globalY)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        acceptedButtons: Qt.NoButton
                        onWheel: function(wheel) {
                            if (!thumbListView.interactive) {
                                wheel.accepted = false
                                return
                            }
                            var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                            thumbListView.contentX = Math.max(0, Math.min(thumbListView.contentX - delta * 0.5,
                                                                          Math.max(0, thumbListView.contentWidth - thumbListView.width)))
                            wheel.accepted = true
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                visible: root.expandable && adapter.model.count > root.collapsedMaxVisible
                color: "transparent"

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u2237"
                        color: Theme.colors.primary
                        font.pixelSize: 14
                        font.weight: Font.Bold
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("展开查看全部 %1 个文件").arg(adapter.model.count)
                        color: Theme.colors.primary
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    ColoredIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        source: Theme.icon("chevron-down")
                        iconSize: 14
                        color: Theme.colors.primary
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.expanded = true
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                visible: adapter.model.count === 0
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
            property int contentHeight: Math.ceil(adapter.model.count / columns) * (root.thumbSize + root.thumbSpacing) - root.thumbSpacing

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                visible: adapter.model.count > 0
                clip: true

                GridView {
                    id: thumbGridView
                    anchors.fill: parent
                    cellWidth: root.thumbSize + root.thumbSpacing
                    cellHeight: root.thumbSize + root.thumbSpacing
                    model: adapter.model
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height
                    cacheBuffer: 500

                    delegate: MediaThumbnailDelegate {
                        width: thumbGridView.cellWidth - root.thumbSpacing
                        height: thumbGridView.cellHeight - root.thumbSpacing
                        messageMode: root.messageMode
                        contextMenuEnabled: true
                        requestedSourceSize: root.thumbSize * 2
                        onActivated: function(origIndex) { root.viewFile(origIndex) }
                        onDeleteRequested: function(origIndex) { root.deleteFile(origIndex) }
                        onContextMenuRequested: function(globalX, globalY, filteredIndex, itemStatus) {
                            contextMenu.targetIndex = filteredIndex
                            contextMenu.fileStatus = itemStatus
                            contextMenu.showAt(globalX, globalY)
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        acceptedButtons: Qt.NoButton
                        onWheel: function(wheel) {
                            if (!thumbGridView.interactive) {
                                wheel.accepted = false
                                return
                            }
                            var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                            thumbGridView.contentY = Math.max(0, Math.min(thumbGridView.contentY - delta * 0.5,
                                                                          Math.max(0, thumbGridView.contentHeight - thumbGridView.height)))
                            wheel.accepted = true
                        }
                    }
                }

                Rectangle {
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
}
