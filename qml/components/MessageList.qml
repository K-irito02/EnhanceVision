import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../components"

/**
 * @brief 消息列表组件
 * 展示所有处理任务的消息列表，支持滚动、批量操作等
 * 参考功能设计文档 0.2 的 3.5 节
 */
Item {
    id: root

    // ========== 属性 ==========
    /**
     * @brief 消息模型
     */
    property var messageModel: null

    /**
     * @brief 是否处于批量选择模式
     */
    property bool batchMode: false

    /**
     * @brief 编辑消息信号
     * @param messageId 消息ID
     */
    signal editMessage(string messageId)

    /**
     * @brief 删除消息信号
     * @param messageId 消息ID
     */
    signal deleteMessage(string messageId)

    /**
     * @brief 保存消息信号
     * @param messageId 消息ID
     */
    signal saveMessage(string messageId)

    /**
     * @brief 取消消息信号
     * @param messageId 消息ID
     */
    signal cancelMessage(string messageId)

    /**
     * @brief 批量保存信号
     */
    signal batchSave()

    /**
     * @brief 批量删除信号
     */
    signal batchDelete()

    // ========== 主布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing._3
        spacing: Theme.spacing._3

        // ========== 顶部工具栏 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2

            // 标题
            Text {
                Layout.fillWidth: true
                text: qsTr("处理任务")
                color: Theme.colors.foreground
                font.pixelSize: Theme.typography.fontSize.lg
                font.bold: true
            }

            // 批量选择按钮
            Button {
                text: batchMode ? qsTr("取消选择") : qsTr("批量选择")
                onClicked: batchMode = !batchMode
            }

            // 批量保存按钮（仅在批量模式下显示）
            Button {
                visible: batchMode
                text: qsTr("批量保存")
                iconSource: "qrc:///icons/save.svg" // TODO: 替换为实际图标
                onClicked: root.batchSave()
            }

            // 批量删除按钮（仅在批量模式下显示）
            Button {
                visible: batchMode
                text: qsTr("批量删除")
                iconSource: "qrc:///icons/trash.svg" // TODO: 替换为实际图标
                onClicked: root.batchDelete()
            }
        }

        // ========== 消息列表 ==========
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacing._3
            model: messageModel
            delegate: MessageItem {
                width: listView.width - Theme.spacing._6
                anchors.horizontalCenter: parent.horizontalCenter

                // 绑定数据
                messageId: model.id
                timestamp: model.timestamp
                mode: model.mode
                modeText: model.modeText
                status: model.status
                statusText: model.statusText
                statusColor: model.statusColor
                progress: model.progress
                queuePosition: model.queuePosition
                errorMessage: model.errorMessage
                isSelected: root.batchMode && model.isSelected

                // 信号连接
                onEditClicked: root.editMessage(messageId)
                onDeleteClicked: root.deleteMessage(messageId)
                onSaveClicked: root.saveMessage(messageId)
                onCancelClicked: root.cancelMessage(messageId)

                // 点击选中/取消选中（批量模式）
                MouseArea {
                    anchors.fill: parent
                    enabled: root.batchMode
                    onClicked: {
                        if (messageModel) {
                            messageModel.selectMessage(messageId, !model.isSelected)
                        }
                    }
                    cursorShape: Qt.PointingHandCursor
                }
            }

            // 空状态
            placeholder: Item {
                anchors.fill: parent

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacing._3

                    // 占位图标
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 64
                        height: 64
                        radius: Theme.radius.xl
                        color: Theme.colors.muted
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("暂无处理任务")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: Theme.typography.fontSize.base
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("添加文件后点击\"发送\"开始处理")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: Theme.typography.fontSize.sm
                    }
                }
            }

            // 滚动条
            ScrollBar.vertical: ScrollBar {
                active: listView.movingVertically || listView.atYBeginning || listView.atYEnd
            }
        }
    }
}
