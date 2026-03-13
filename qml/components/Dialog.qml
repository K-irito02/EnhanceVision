import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./../styles"
import "./../controls"

/**
 * @brief 对话框组件
 * 用于显示确认对话框、错误对话框等
 * 参考功能设计文档 6.1 和 6.2 节
 */
Item {
    id: root

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

    /**
     * @brief 按钮配置
     */
    enum ButtonRole {
        Primary,    // 主按钮
        Secondary,  // 次要按钮
        Destructive // 危险按钮
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
    /**
     * @brief 主按钮点击信号
     */
    signal primaryButtonClicked()

    /**
     * @brief 次要按钮点击信号
     */
    signal secondaryButtonClicked()

    /**
     * @brief 对话框关闭信号
     */
    signal closed()

    // ========== 内部属性 ==========
    /**
     * @brief 当前主题颜色
     */
    readonly property var colors: Theme.isDark ? Colors.dark : Colors.light

    /**
     * @brief 获取对话框图标
     */
    readonly property string iconSource: {
        switch (type) {
            case Dialog.DialogType.Success:
                return "✓"
            case Dialog.DialogType.Warning:
                return "⚠"
            case Dialog.DialogType.Error:
                return "✕"
            case Dialog.DialogType.Confirm:
                return "?"
            default:
                return "ℹ"
        }
    }

    /**
     * @brief 获取对话框图标颜色
     */
    readonly property color iconColor: {
        switch (type) {
            case Dialog.DialogType.Success:
                return colors.chart5
            case Dialog.DialogType.Warning:
                return colors.chart4
            case Dialog.DialogType.Error:
                return colors.destructive
            case Dialog.DialogType.Confirm:
                return colors.primary
            default:
                return colors.primary
        }
    }

    // ========== 显示/隐藏方法 ==========
    /**
     * @brief 显示对话框
     * @param dialogTitle 标题
     * @param dialogMessage 消息
     * @param dialogType 对话框类型
     */
    function show(dialogTitle, dialogMessage, dialogType) {
        title = dialogTitle || ""
        message = dialogMessage || ""
        type = dialogType !== undefined ? dialogType : Dialog.DialogType.Info
        
        visible = true
        opacity = 0
        scale = 0.9
        
        // 启动动画
        showAnimation.start()
    }

    /**
     * @brief 隐藏对话框
     */
    function hide() {
        hideAnimation.start()
    }

    // ========== 动画 ==========
    /**
     * @brief 显示动画
     */
    ParallelAnimation {
        id: showAnimation
        NumberAnimation {
            target: root
            property: "opacity"
            from: 0
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "scale"
            from: 0.9
            to: 1
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    /**
     * @brief 隐藏动画
     */
    ParallelAnimation {
        id: hideAnimation
        NumberAnimation {
            target: root
            property: "opacity"
            from: 1
            to: 0
            duration: 150
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: root
            property: "scale"
            from: 1
            to: 0.95
            duration: 150
            easing.type: Easing.InCubic
        }
        onFinished: {
            visible = false
            closed()
        }
    }

    // ========== 背景遮罩 ==========
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.5)
        
        // 点击背景关闭对话框
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (type !== Dialog.DialogType.Confirm) {
                    root.hide()
                }
            }
        }
    }

    // ========== 对话框内容 ==========
    Rectangle {
        id: dialogRect
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 48)
        height: contentColumn.height + 48
        radius: 16
        color: colors.card
        border.color: colors.cardBorder
        border.width: 1

        ColumnLayout {
            id: contentColumn
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 24
            anchors.bottomMargin: 24
            spacing: 20

            // 图标和标题区域
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // 图标
                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 12
                    color: Qt.tint(iconColor, Qt.rgba(1, 1, 1, 0.1))
                    
                    Text {
                        anchors.centerIn: parent
                        text: iconSource
                        font.pixelSize: 24
                        color: iconColor
                    }
                }

                // 标题
                Text {
                    Layout.fillWidth: true
                    text: title
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    color: colors.foreground
                    elide: Text.ElideRight
                }
            }

            // 消息内容
            Text {
                Layout.fillWidth: true
                text: message
                font.pixelSize: 14
                color: colors.mutedForeground
                wrapMode: Text.Wrap
                lineHeight: 1.5
            }

            Item { Layout.fillHeight: true }

            // 按钮区域
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                // 次要按钮
                Button {
                    id: secondaryBtn
                    Layout.preferredWidth: 100
                    visible: showSecondaryButton
                    variant: "secondary"
                    text: secondaryButtonText
                    onClicked: {
                        root.secondaryButtonClicked()
                        root.hide()
                    }
                }

                // 主按钮
                Button {
                    id: primaryBtn
                    Layout.preferredWidth: 100
                    variant: type === Dialog.DialogType.Error ? "destructive" : "primary"
                    text: primaryButtonText
                    onClicked: {
                        root.primaryButtonClicked()
                        root.hide()
                    }
                }
            }
        }
    }
}
