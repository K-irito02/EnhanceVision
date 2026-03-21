import QtQuick
import QtQuick.Controls
import "../styles"

Slider {
    id: root
    from: 0
    to: 100
    value: 50

    implicitWidth: 200
    implicitHeight: 20

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: 2

        color: Theme.isDark ? Qt.rgba(0.12, 0.27, 0.65, 0.25) : Qt.rgba(0, 0.18, 0.65, 0.1)

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: parent.radius

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

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 14
        height: 14
        radius: 7

        color: root.enabled ? "#FFFFFF" : Theme.colors.mutedForeground
        border.width: 2
        border.color: root.enabled ? (root.pressed ? "#3B82F6" : (root.hovered ? "#3B82F6" : Theme.colors.primary)) : Theme.colors.muted

        scale: root.pressed ? 1.1 : (root.hovered ? 1.05 : 1.0)
        Behavior on scale {
            NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 5
            height: 5
            radius: 2.5
            color: Theme.colors.primary
            visible: root.enabled && !root.pressed
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: parent.height + 8
            radius: width / 2
            color: "transparent"
            border.width: root.hovered || root.pressed ? 1 : 0
            border.color: Qt.rgba(0.23, 0.51, 0.96, 0.3)
            visible: root.hovered || root.pressed

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }
        }
    }
}
