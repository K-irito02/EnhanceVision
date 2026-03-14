import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 对话框组件
 * 用于显示确认对话框、错误对话框等
 * 参考功能设计文档 6.1 和 6.2 节
 */
Item {
    id: root

    // ========== 初始状态 ==========
    visible: false

    // ========== 类型定义 ==========
    /**
     * @brief 对话框类型枚举
     */
    enum DialogType {
        Info,       // 信息
        Success,    // 成功
        Warning,    // 警告
        Error,      // 错误
        Confirm     // 确认
    }

    // ========== 属性定义 ==========
    /**
     * @brief 对话框标题
     */
    property string title: ""

    /**
     * @brief 对话框消息
     */
    property string message: ""

    /**
     * @brief 对话框类型
     */
    property int type: Dialog.DialogType.Info

    /**
     * @brief 主按钮文本
     */
    property string primaryButtonText: qsTr("确定")

    /**
     * @brief 次要按钮文本
     */
    property string secondaryButtonText: qsTr("取消")

    /**
     * @brief 是否显示次要按钮
     */
    property bool showSecondaryButton: true

    // ========== 信号定义 ==========
    signal primaryButtonClicked()
    signal secondaryButtonClicked()
    signal closed()

    // ========== 内部属性 ==========
    readonly property string dialogIconName: {
        switch (type) {
            case Dialog.DialogType.Success: return "check-circle"
            case Dialog.DialogType.Warning: return "alert-triangle"
            case Dialog.DialogType.Error:   return "x-circle"
            case Dialog.DialogType.Confirm: return "help-circle"
            default:                        return "info"
        }
    }

    readonly property color dialogIconBgColor: {
        switch (type) {
            case Dialog.DialogType.Success: return Theme.colors.successSubtle
            case Dialog.DialogType.Warning: return Theme.colors.warningSubtle
            case Dialog.DialogType.Error:   return Theme.colors.destructiveSubtle
            default:                        return Theme.colors.primarySubtle
        }
    }

    readonly property color dialogIconColor: {
        switch (type) {
            case Dialog.DialogType.Success: return Theme.colors.success
            case Dialog.DialogType.Warning: return Theme.colors.warning
            case Dialog.DialogType.Error:   return Theme.colors.destructive
            default:                        return Theme.colors.primary
        }
    }

    // ========== 显示/隐藏方法 ==========
    function show(dialogTitle, dialogMessage, dialogType) {
        title = dialogTitle || ""
        message = dialogMessage || ""
        type = dialogType !== undefined ? dialogType : Dialog.DialogType.Info
        visible = true
        opacity = 0
        showAnimation.start()
    }

    function hide() {
        hideAnimation.start()
    }

    // ========== 动画 ==========
    ParallelAnimation {
        id: showAnimation
        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { target: dialogRect; property: "scale"; from: 0.92; to: 1; duration: 250; easing.type: Easing.OutCubic }
    }

    ParallelAnimation {
        id: hideAnimation
        NumberAnimation { target: root; property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { target: dialogRect; property: "scale"; from: 1; to: 0.95; duration: 150; easing.type: Easing.InCubic }
        onFinished: { visible = false; closed() }
    }

    // ========== 背景遮罩 ==========
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.6 : 0.4)

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (type !== Dialog.DialogType.Confirm) root.hide()
            }
        }
    }

    // ========== 对话框内容 ==========
    Rectangle {
        id: dialogRect
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 48)
        implicitHeight: contentColumn.implicitHeight + 48
        radius: Theme.radius.xxl
        color: Theme.colors.card
        border.color: Theme.colors.cardBorder
        border.width: 1

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            // 图标和标题区域
            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                // 图标
                Rectangle {
                    width: 44; height: 44
                    radius: Theme.radius.xl
                    color: dialogIconBgColor

                    Image {
                        anchors.centerIn: parent
                        width: 22; height: 22
                        source: Theme.icon(dialogIconName)
                        sourceSize: Qt.size(22, 22)
                        smooth: true
                    }
                }

                // 标题
                Text {
                    Layout.fillWidth: true
                    text: title
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: Theme.colors.foreground
                    elide: Text.ElideRight
                }

                // 关闭按钮
                IconButton {
                    iconName: "x"
                    iconSize: 14
                    btnSize: 28
                    onClicked: root.hide()
                }
            }

            // 消息内容
            Text {
                Layout.fillWidth: true
                text: message
                font.pixelSize: 14
                color: Theme.colors.mutedForeground
                wrapMode: Text.Wrap
                lineHeight: 1.6
                leftPadding: 58
            }

            // 间距
            Item { height: 4 }

            // 按钮区域
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                // 次要按钮
                Button {
                    visible: showSecondaryButton
                    variant: "secondary"
                    text: secondaryButtonText
                    size: "md"
                    onClicked: { root.secondaryButtonClicked(); root.hide() }
                }

                // 主按钮
                Button {
                    variant: type === Dialog.DialogType.Error ? "destructive" : "primary"
                    text: primaryButtonText
                    size: "md"
                    onClicked: { root.primaryButtonClicked(); root.hide() }
                }
            }
        }
    }
}
