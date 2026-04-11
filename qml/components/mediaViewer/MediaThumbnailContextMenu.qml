import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../styles"
import "../../controls"

Popup {
    id: root

    property bool messageMode: false
    property int targetIndex: -1
    property int fileStatus: 0
    property var model: null
    readonly property bool isSuccess: fileStatus === 2
    readonly property bool isFailedOrCancelled: fileStatus === 3 || fileStatus === 4

    signal viewRequested(int origIndex)
    signal saveRequested(int origIndex)
    signal retryRequested(int origIndex)
    signal deleteRequested(int origIndex)

    parent: Overlay.overlay
    padding: 6
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function _targetOrigIndex() {
        if (!model || targetIndex < 0 || targetIndex >= model.count) {
            return targetIndex
        }
        var item = model.get(targetIndex)
        return item ? item.origIndex : targetIndex
    }

    function showAt(globalX, globalY) {
        if (!parent) {
            return
        }

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

        x = Math.max(margin, Math.min(finalX, parent.width - menuWidth - margin))
        y = Math.max(margin, Math.min(finalY, parent.height - menuHeight - margin))
        open()
    }

    function calculateMenuHeight() {
        var itemHeight = 34
        var separatorHeight = 9
        var paddingSize = 12
        var spacingSize = 3
        var count = 0
        var hasSeparator = false

        if (!messageMode || isSuccess) {
            count++
        }
        if (messageMode && isSuccess) {
            count++
        }
        if (messageMode && isFailedOrCancelled) {
            count++
        }
        if (count > 0) {
            hasSeparator = true
        }

        count++

        var height = paddingSize * 2 + count * itemHeight + (count - 1) * spacingSize
        if (hasSeparator) {
            height += separatorHeight + spacingSize
        }
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
            visible: !root.messageMode || root.isSuccess
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
                    root.close()
                    root.viewRequested(root._targetOrigIndex())
                }
            }
        }

        Rectangle {
            visible: root.messageMode && root.isSuccess
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
                    root.close()
                    root.saveRequested(root._targetOrigIndex())
                }
            }
        }

        Rectangle {
            visible: root.messageMode && root.isFailedOrCancelled
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
                    root.close()
                    root.retryRequested(root._targetOrigIndex())
                }
            }
        }

        Rectangle {
            visible: (!root.messageMode || root.isSuccess) || (root.messageMode && root.isFailedOrCancelled)
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
                    root.close()
                    root.deleteRequested(root._targetOrigIndex())
                }
            }
        }
    }
}
