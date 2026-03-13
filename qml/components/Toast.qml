import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

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

    /**
     * @brief 获取 Toast 背景色
     */
    readonly property color backgroundColor: {
        switch (type) {
            case Toast.ToastType.Success:
                return Theme.colors.success
            case Toast.ToastType.Warning:
                return Theme.colors.warning
            case Toast.ToastType.Error:
                return Theme.colors.destructive
            default:
                return Theme.colors.primary
        }
    }

    /**
     * @brief 获取 Toast 图标名
     */
    readonly property string toastIconName: {
        switch (type) {
            case Toast.ToastType.Success:
                return "check-circle"
            case Toast.ToastType.Warning:
                return "alert-triangle"
            case Toast.ToastType.Error:
                return "x-circle"
            default:
                return "info"
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
            from: 0; to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "y"
            from: -20; to: 0
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
            from: 1; to: 0
            duration: 300
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: root
            property: "y"
            from: 0; to: -20
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
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(420, toastRow.implicitWidth + 40)
        height: 48
        radius: Theme.radius.xl
        color: backgroundColor

        RowLayout {
            id: toastRow
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 10

            // 图标
            Image {
                width: 18; height: 18
                source: Theme.icon(toastIconName)
                sourceSize: Qt.size(18, 18)
                smooth: true
                Layout.alignment: Qt.AlignVCenter
            }

            // 消息文本
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: message
                font.pixelSize: 13
                font.weight: Font.Medium
                color: "#FFFFFF"
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // 关闭按钮
            IconButton {
                btnSize: 24
                iconName: "x"
                iconSize: 12
                iconColor: "#FFFFFF"
                iconHoverColor: "#FFFFFF"
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
