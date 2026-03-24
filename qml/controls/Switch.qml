import QtQuick
import "../styles"

Item {
    id: control

    implicitWidth: 40
    implicitHeight: 22
    width: 40
    height: 22

    property bool checked: false
    property bool enabled: true
    property alias checkedColor: backgroundRect.checkedColor
    property alias uncheckedColor: backgroundRect.uncheckedColor

    signal toggled()

    Rectangle {
        id: backgroundRect
        anchors.fill: parent
        radius: 11

        property color checkedColor: Theme.colors.switchOn
        property color uncheckedColor: Theme.colors.switchOff

        color: !control.enabled ? Theme.colors.muted :
               control.checked ? checkedColor : uncheckedColor
        opacity: control.enabled ? 1.0 : 0.6

        Behavior on color {
            ColorAnimation {
                duration: Theme.animation.fast
                easing.type: Easing.OutCubic
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.animation.fast
            }
        }

        Rectangle {
            id: handle
            x: control.checked ? parent.width - width - 3 : 3
            y: parent.height / 2 - height / 2
            width: 16
            height: 16
            radius: 8
            color: "#FFFFFF"

            Behavior on x {
                NumberAnimation {
                    duration: Theme.animation.fast
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        enabled: control.enabled
        onClicked: {
            control.checked = !control.checked
            control.toggled()
        }
    }
}
