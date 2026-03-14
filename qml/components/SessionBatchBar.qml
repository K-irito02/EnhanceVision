import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 会话批量操作栏组件
 * 
 * 提供批量选择、全选、取消全选、批量删除、批量清空等功能
 */
Rectangle {
    id: root

    // ========== 属性定义 ==========
    property bool batchMode: false
    property int selectedCount: 0
    property int totalCount: 0
    property bool expanded: true

    // ========== 信号定义 ==========
    signal toggleBatchMode()
    signal selectAll()
    signal deselectAll()
    signal deleteSelected()
    signal clearSelected()

    // ========== 视觉属性 ==========
    height: expanded ? (batchMode ? 80 : 36) : 0
    color: "transparent"
    clip: true

    Behavior on height {
        NumberAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // ========== 标题栏：批量操作切换 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // 批量操作切换按钮
            Rectangle {
                width: batchToggleRow.implicitWidth + 12
                height: 28
                radius: Theme.radius.sm
                color: root.batchMode ? Theme.colors.primarySubtle : "transparent"
                border.width: root.batchMode ? 0 : 1
                border.color: Theme.colors.border

                Row {
                    id: batchToggleRow
                    anchors.centerIn: parent
                    spacing: 6

                    ColoredIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        source: Theme.icon(root.batchMode ? "check-square" : "square")
                        iconSize: 12
                        color: root.batchMode ? Theme.colors.primary : Theme.colors.mutedForeground
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.batchMode ? qsTr("退出批量") : qsTr("批量操作")
                        font.pixelSize: 11
                        color: root.batchMode ? Theme.colors.primary : Theme.colors.mutedForeground
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleBatchMode()
                }
            }

            Item { Layout.fillWidth: true }

            // 选中计数（批量模式下显示）
            Text {
                visible: root.batchMode
                text: qsTr("已选 %1/%2").arg(root.selectedCount).arg(root.totalCount)
                font.pixelSize: 11
                color: Theme.colors.mutedForeground
            }
        }

        // ========== 批量操作按钮栏（批量模式下显示）==========
        RowLayout {
            visible: root.batchMode
            Layout.fillWidth: true
            spacing: 6

            // 全选/取消全选按钮
            Rectangle {
                width: selectAllRow.implicitWidth + 12
                height: 28
                radius: Theme.radius.sm
                color: selectAllMouse.containsMouse ? Theme.colors.sidebarAccent : "transparent"
                border.width: 1
                border.color: Theme.colors.border

                Row {
                    id: selectAllRow
                    anchors.centerIn: parent
                    spacing: 4

                    ColoredIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        source: Theme.icon(root.selectedCount === root.totalCount ? "check-square" : "square")
                        iconSize: 12
                        color: Theme.colors.icon
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.selectedCount === root.totalCount ? qsTr("取消全选") : qsTr("全选")
                        font.pixelSize: 11
                        color: Theme.colors.sidebarForeground
                    }
                }

                MouseArea {
                    id: selectAllMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.selectedCount === root.totalCount) {
                            root.deselectAll()
                        } else {
                            root.selectAll()
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // 批量清空按钮
            IconButton {
                iconName: "trash-2"
                iconSize: 14
                btnSize: 28
                tooltip: qsTr("清空选中会话")
                enabled: root.selectedCount > 0
                opacity: enabled ? 1.0 : 0.4
                onClicked: root.clearSelected()
            }

            // 批量删除按钮
            IconButton {
                iconName: "x-circle"
                iconSize: 14
                btnSize: 28
                danger: true
                tooltip: qsTr("删除选中会话")
                enabled: root.selectedCount > 0
                opacity: enabled ? 1.0 : 0.4
                onClicked: root.deleteSelected()
            }
        }
    }
}
