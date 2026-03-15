import QtQuick
import QtQuick.Controls
import "../styles"

/**
 * @brief 自定义滑块控件
 * 克莱因蓝主题风格，现代化设计
 */
Slider {
    id: root
    from: 0
    to: 100
    value: 50

    implicitWidth: 200
    implicitHeight: 20

    // ========== 轨道背景 ==========
    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 3
        radius: 1.5

        // 未填充部分 - 淡蓝色背景
        color: Theme.isDark ? Qt.rgba(0.12, 0.27, 0.65, 0.3) : Qt.rgba(0, 0.18, 0.65, 0.12)

        // 已填充部分 - 蓝色渐变
        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius

            // 蓝色渐变效果
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: Theme.isDark ? "#1E56D0" : "#002FA7"
                }
                GradientStop {
                    position: 1.0
                    color: Theme.isDark ? "#3B82F6" : "#1A56DB"
                }
            }

            Behavior on width {
                NumberAnimation { duration: 50; easing.type: Easing.OutCubic }
            }
        }
    }

    // ========== 滑块手柄 ==========
    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 12
        height: 12
        radius: 6

        // 手柄颜色 - 蓝色
        color: root.enabled ? (root.pressed ? "#3B82F6" : "#1E56D0") : Theme.colors.mutedForeground
        border.width: 1.5
        border.color: root.pressed ? "#60A5FA" : (root.hovered ? "#3B82F6" : "#1E56D0")

        // 按下时微缩放效果
        scale: root.pressed ? 1.1 : (root.hovered ? 1.05 : 1.0)
        Behavior on scale {
            NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
        }

        // 外发光效果
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 6
            height: parent.height + 6
            radius: width / 2
            color: "transparent"
            border.width: root.hovered || root.pressed ? 1.5 : 0
            border.color: Qt.rgba(0.23, 0.51, 0.96, 0.4)
            visible: root.hovered || root.pressed

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }
        }

        // 内部高光
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.4
            height: parent.height * 0.4
            radius: width / 2
            color: Qt.rgba(1, 1, 1, 0.3)
            visible: root.enabled
        }
    }
}
