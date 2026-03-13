import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

/**
 * @brief 单条消息组件
 * 显示单条处理任务的详细信息，包括状态、进度、时间戳等
 * 参考功能设计文档 0.2 的 3.5 节
 */
Card {
    id: root

    // ========== 属性 ==========
    /**
     * @brief 消息ID
     */
    property string messageId: ""

    /**
     * @brief 时间戳
     */
    property var timestamp: new Date()

    /**
     * @brief 处理模式 (0: Shader, 1: AIInference, 2: Browse)
     */
    property int mode: 0

    /**
     * @brief 处理模式文本
     */
    property string modeText: qsTr("Shader 滤镜")

    /**
     * @brief 处理状态 (0: Pending, 1: Processing, 2: Completed, 3: Failed, 4: Cancelled)
     */
    property int status: 0

    /**
     * @brief 处理状态文本
     */
    property string statusText: qsTr("待处理")

    /**
     * @brief 处理状态颜色
     */
    property color statusColor: "#f59e0b"

    /**
     * @brief 处理进度 (0-100)
     */
    property int progress: 0

    /**
     * @brief 队列位置 (-1 表示不在队列)
     */
    property int queuePosition: -1

    /**
     * @brief 错误信息（失败时显示）
     */
    property string errorMessage: ""

    /**
     * @brief 是否被选中（批量操作）
     */
    property bool isSelected: false

    /**
     * @brief 点击编辑信号
     */
    signal editClicked()

    /**
     * @brief 点击删除信号
     */
    signal deleteClicked()

    /**
     * @brief 点击保存信号
     */
    signal saveClicked()

    /**
     * @brief 点击取消信号
     */
    signal cancelClicked()

    // ========== 卡片设置 ==========
    showShadow: true

    // ========== 选中边框 ==========
    border.width: isSelected ? 2 : 1
    border.color: isSelected ? Theme.colors.primary : Theme.colors.cardBorder

    // ========== 内容区域 ==========
    content: ColumnLayout {
        spacing: Theme.spacing._3

        // ========== 顶部行：时间戳、状态、操作按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2

            // 时间戳
            Text {
                Layout.fillWidth: true
                text: timestamp.toLocaleString(Qt.locale(), Qt.DefaultLocaleShortDate)
                color: Theme.colors.mutedForeground
                font.pixelSize: Theme.typography.fontSize.sm
            }

            // 队列位置（仅在队列中显示）
            Text {
                visible: queuePosition >= 0
                text: qsTr("#%1").arg(queuePosition)
                color: Theme.colors.mutedForeground
                font.pixelSize: Theme.typography.fontSize.sm
                font.bold: true
            }

            // 模式标签
            Rectangle {
                height: 20
                radius: Theme.radius.sm
                color: Theme.colors.secondary
                padding: 6

                Text {
                    anchors.centerIn: parent
                    text: modeText
                    color: Theme.colors.secondaryForeground
                    font.pixelSize: Theme.typography.fontSize.xs
                    font.bold: true
                }
            }

            // 状态标签
            Rectangle {
                height: 20
                radius: Theme.radius.sm
                color: statusColor + "20" // 20% 透明度
                padding: 6

                Text {
                    anchors.centerIn: parent
                    text: statusText
                    color: statusColor
                    font.pixelSize: Theme.typography.fontSize.xs
                    font.bold: true
                }
            }

            // 操作按钮区域
            Row {
                spacing: Theme.spacing._1

                // 取消按钮（仅在待处理或处理中时显示）
                IconButton {
                    visible: status === 0 || status === 1
                    iconSource: "qrc:///icons/x-circle.svg" // TODO: 替换为实际图标
                    iconSize: 16
                    onClicked: root.cancelClicked()
                }

                // 编辑按钮（仅在已完成时显示）
                IconButton {
                    visible: status === 2
                    iconSource: "qrc:///icons/edit.svg" // TODO: 替换为实际图标
                    iconSize: 16
                    onClicked: root.editClicked()
                }

                // 保存按钮（仅在已完成时显示）
                IconButton {
                    visible: status === 2
                    iconSource: "qrc:///icons/save.svg" // TODO: 替换为实际图标
                    iconSize: 16
                    onClicked: root.saveClicked()
                }

                // 删除按钮
                IconButton {
                    iconSource: "qrc:///icons/trash.svg" // TODO: 替换为实际图标
                    iconSize: 16
                    onClicked: root.deleteClicked()
                }
            }
        }

        // ========== 进度条区域（仅在待处理或处理中时显示） ==========
        Column {
            Layout.fillWidth: true
            visible: status === 0 || status === 1
            spacing: Theme.spacing._1

            // 进度条
            Rectangle {
                width: parent.width
                height: 8
                radius: Theme.radius.full
                color: Theme.colors.muted

                Rectangle {
                    width: parent.width * (progress / 100)
                    height: parent.height
                    radius: parent.radius
                    color: statusColor

                    Behavior on width {
                        NumberAnimation {
                            duration: Theme.animation.normal
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            // 进度文字
            RowLayout {
                width: parent.width
                spacing: Theme.spacing._1

                Text {
                    Layout.fillWidth: true
                    text: status === 1 ? qsTr("处理中... %1%").arg(progress) : qsTr("等待处理...")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: Theme.typography.fontSize.sm
                }

                Text {
                    text: "%1%".arg(progress)
                    color: statusColor
                    font.pixelSize: Theme.typography.fontSize.sm
                    font.bold: true
                }
            }
        }

        // ========== 错误信息（仅在失败时显示） ==========
        Rectangle {
            Layout.fillWidth: true
            visible: status === 3 && !errorMessage.isEmpty
            radius: Theme.radius.md
            color: Theme.colors.destructiveSubtle
            padding: Theme.spacing._3

            RowLayout {
                anchors.fill: parent
                spacing: Theme.spacing._2

                // 错误图标（使用简单的矩形代替）
                Rectangle {
                    width: 16
                    height: 16
                    radius: Theme.radius.full
                    color: Theme.colors.destructive
                }

                Text {
                    Layout.fillWidth: true
                    text: errorMessage
                    color: Theme.colors.destructive
                    font.pixelSize: Theme.typography.fontSize.sm
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ========== 媒体文件预览区域（占位） ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            radius: Theme.radius.md
            color: Theme.colors.secondary
            border.color: Theme.colors.border

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacing._2

                // 占位图标（使用简单的矩形代替）
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 48
                    height: 48
                    radius: Theme.radius.md
                    color: Theme.colors.muted
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("媒体文件预览")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: Theme.typography.fontSize.sm
                }
            }
        }
    }

    // ========== 选中状态背景 ==========
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: root.radius
        color: isSelected ? Theme.colors.primarySubtle : "transparent"
        z: -1
    }
}
