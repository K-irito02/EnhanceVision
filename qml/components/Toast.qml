import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./../styles"

/**
 * @brief Toast 提示组件
 * 用于显示短暂的提示信息，如成功、错误、警告等
 * 参考功能设计文档 6.1 和 6.2 节
 */
Item {
    id: root

    // ========== 类型定义 ==========
    /**
     * @brief Toast 类型枚举
     */
    enum ToastType {
        Info,       // 信息
        Success,    // 成功
        Warning,    // 警告
        Error       // 错误
    }

    // ========== 属性定义 ==========
    /**
     * @brief 消息内容
     */
    property string message: ""

    /**
     * @brief Toast 类型
     */
    property int type: Toast.ToastType.Info

    /**
     * @brief 显示时长 (毫秒)
     */
    property int duration: 3000

    // ========== 信号定义 ==========
    /**
     * @brief Toast 显示完成信号
     */
    signal dismissed()

    // ========== 内部属性 ==========
    /**
     * @brief 当前主题颜色
     */
    readonly property var colors: Theme.isDark ? Colors.dark : Colors.light

    /**
     * @brief 获取 Toast 背景色
     */
    readonly property color backgroundColor: {
        switch (type) {
            case Toast.ToastType.Success:
                return colors.chart5
            case Toast.ToastType.Warning:
                return colors.chart4
            case Toast.ToastType.Error:
                return colors.destructive
            default:
                return colors.primary
        }
    }

    /**
     * @brief 获取 Toast 图标
     */
    readonly property string iconSource: {
        switch (type) {
            case Toast.ToastType.Success:
                return "✓"
            case Toast.ToastType.Warning:
                return "⚠"
            case Toast.ToastType.Error:
                return "✕"
            default:
                return "ℹ"
        }
    }

    // ========== 定时器 ==========
    /**
     * @brief 自动消失定时器
     */
    Timer {
        id: dismissTimer
        interval: root.duration
        onTriggered: root.hide()
    }

    // ========== 显示/隐藏方法 ==========
    /**
     * @brief 显示 Toast
     * @param msg 消息内容
     * @param toastType Toast 类型
     * @param toastDuration 显示时长
     */
    function show(msg, toastType, toastDuration) {
        message = msg
        type = toastType !== undefined ? toastType : Toast.ToastType.Info
        duration = toastDuration !== undefined ? toastDuration : 3000
        
        visible = true
        opacity = 0
        y = -20
        
        // 启动动画
        showAnimation.start()
        
        // 启动自动消失定时器
        dismissTimer.restart()
    }

    /**
     * @brief 隐藏 Toast
     */
    function hide() {
        dismissTimer.stop()
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
            duration: 300
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "y"
            from: -20
            to: 0
            duration: 300
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
            duration: 300
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: root
            property: "y"
            from: 0
            to: -20
            duration: 300
            easing.type: Easing.InCubic
        }
        onFinished: {
            visible = false
            dismissed()
        }
    }

    // ========== UI 布局 ==========
    Rectangle {
        id: toastRect
        anchors.centerIn: parent
        width: Math.min(400, message.length * 12 + 80)
        height: 56
        radius: 12
        color: backgroundColor
        opacity: 0.95

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            // 图标
            Text {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                Layout.alignment: Qt.AlignVCenter
                text: iconSource
                font.pixelSize: 20
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            // 消息文本
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: message
                font.pixelSize: 14
                font.weight: Font.Medium
                color: "#ffffff"
                elide: Text.ElideRight
                wrapMode: Text.Wrap
            }

            // 关闭按钮
            IconButton {
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignVCenter
                iconText: "✕"
                iconColor: "#ffffff"
                onClicked: root.hide()
            }
        }
    }

    // 点击 Toast 也可以关闭
    MouseArea {
        anchors.fill: toastRect
        onClicked: root.hide()
        cursorShape: Qt.PointingHandCursor
    }
}
