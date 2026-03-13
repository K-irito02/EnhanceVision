import QtQuick
import QtQuick.Controls
import "../styles"

/**
 * @brief 自定义滑块控件
 * 克莱因蓝主题风格
 */
Slider {
    id: root
    from: 0
    to: 100
    value: 50

    implicitWidth: 200
    implicitHeight: 24

    // ========== 轨道背景 ==========
    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: 2
        color: Theme.colors.muted

        // 已填充部分
        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: root.enabled ? Theme.colors.primary : Theme.colors.mutedForeground

            Behavior on width {
                NumberAnimation { duration: 50; easing.type: Easing.OutCubic }
            }
        }
    }

    // ========== 滑块手柄 ==========
    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: root.enabled ? Theme.colors.primary : Theme.colors.mutedForeground
        border.width: 2
        border.color: root.pressed ? Theme.colors.primaryHover : (root.enabled ? Theme.colors.primary : Theme.colors.mutedForeground)

        // 按下时放大效果
        scale: root.pressed ? 1.15 : 1.0
        Behavior on scale {
            NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
        }

        // 发光效果
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: parent.height + 8
            radius: width / 2
            color: "transparent"
            border.width: root.hovered || root.pressed ? 2 : 0
            border.color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.3)
            visible: root.hovered || root.pressed
        }
    }
}
