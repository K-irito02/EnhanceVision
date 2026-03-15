import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    id: root

    property string title: ""
    property string icon: ""
    property bool expanded: true
    property alias content: contentLoader.sourceComponent
    property bool hasModifiedValues: false

    default property alias contentData: contentLoader.sourceComponent

    spacing: 0
    Layout.fillWidth: true
    Layout.rightMargin: 0

    Rectangle {
        id: header
        Layout.fillWidth: true
        Layout.rightMargin: 0
        height: 42
        radius: Theme.radius.md
        color: headerMouse.containsMouse ? Theme.colors.surfaceHover : "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 8
            spacing: 10

            ColoredIcon {
                source: Theme.icon(root.icon)
                iconSize: 18
                color: Theme.colors.primary
            }

            Text {
                text: root.title
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Theme.colors.foreground
                Layout.fillWidth: true
            }

            Rectangle {
                visible: root.hasModifiedValues
                width: 8
                height: 8
                radius: 4
                color: Theme.colors.primary
            }

            ColoredIcon {
                source: Theme.icon("chevron-down")
                iconSize: 16
                color: Theme.colors.mutedForeground
                rotation: root.expanded ? 0 : -90
                Behavior on rotation { NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic } }
            }
        }

        MouseArea {
            id: headerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton
            propagateComposedEvents: true
            onClicked: {
                root.expanded = !root.expanded
                mouse.accepted = false
            }
            onWheel: function(wheel) {
                wheel.accepted = false
            }
        }

        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
    }

    ColumnLayout {
        id: contentContainer
        Layout.fillWidth: true
        Layout.leftMargin: 4
        Layout.rightMargin: 0
        Layout.topMargin: 6
        spacing: 8

        visible: root.expanded
        clip: true

        Loader {
            id: contentLoader
            Layout.fillWidth: true
        }

        Behavior on opacity {
            NumberAnimation { duration: Theme.animation.fast }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.rightMargin: 0
        Layout.topMargin: 8
        height: 1
        color: Theme.colors.border
        opacity: 0.3
    }
}
