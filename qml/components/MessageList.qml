import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 消息列表组件
 * 展示所有处理任务的消息列表，支持滚动、批量操作等
 * 参考功能设计文档 0.2 的 3.5 节
 */
Rectangle {
    id: root

    // ========== 属性 ==========
    property bool batchMode: false
    property var selectedIds: []

    // ========== 视觉属性 ==========
    color: Theme.colors.card
    border.width: 1
    border.color: Theme.colors.cardBorder
    radius: Theme.radius.lg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // ========== 顶部：标题和操作 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColoredIcon {
                source: Theme.icon("list")
                iconSize: 16
                color: Theme.colors.icon
            }

            Text {
                text: qsTr("处理任务")
                color: Theme.colors.foreground
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Item { Layout.fillWidth: true }

            Button {
                text: batchMode ? qsTr("退出选择") : qsTr("批量操作")
                variant: "ghost"
                size: "sm"
                onClicked: {
                    batchMode = !batchMode
                    if (!batchMode) selectedIds = []
                }
            }
        }

        // ========== 批处理操作栏 ==========
        RowLayout {
            visible: batchMode && selectedIds.length > 0
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("已选择 %1 项").arg(selectedIds.length)
                color: Theme.colors.primary
                font.pixelSize: 12
                font.weight: Font.Medium
            }

            Item { Layout.fillWidth: true }

            Button { text: qsTr("批量保存"); iconName: "save"; variant: "secondary"; size: "sm" }
            Button { text: qsTr("批量删除"); variant: "destructive"; size: "sm" }
        }

        // ========== 消息列表 ==========
        ListView {
            id: messageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6

            // 使用 ListModel 作为临时模型，直到 ProcessingModel 实现为 QAbstractListModel
            model: ListModel {
                id: tempMessageModel
                // 临时测试数据
            }

            delegate: MessageItem {
                width: messageList.width
                taskId: model.taskId || ""
                timestamp: model.timestamp || ""
                mode: model.mode !== undefined ? model.mode : 0
                status: model.status !== undefined ? model.status : 0
                progress: model.progress !== undefined ? model.progress : 0.0
                queuePosition: model.queuePosition !== undefined ? model.queuePosition : 0
                errorMessage: model.errorMessage || ""
                selectable: batchMode

                onCancelClicked: processingController.cancelTask(model.taskId)
                onEditClicked: console.log("编辑任务:", model.taskId)
                onSaveClicked: console.log("保存任务:", model.taskId)
                onDeleteClicked: processingController.deleteTask(model.taskId)

                onSelectionToggled: function(isSelected) {
                    if (isSelected) {
                        selectedIds = selectedIds.concat([model.taskId])
                    } else {
                        selectedIds = selectedIds.filter(function(id) { return id !== model.taskId })
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { active: true }

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
                        text: qsTr("点击\"处理\"按钮开始")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 11
                        opacity: 0.7
                    }
                }
            }
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
