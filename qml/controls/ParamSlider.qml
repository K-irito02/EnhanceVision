import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

ColumnLayout {
    id: root

    property string paramName: ""
    property real from: 0.0
    property real to: 1.0
    property real value: 0.0
    property real stepSize: 0.01
    property int decimals: 2

    signal paramValueChanged(real newValue)

    spacing: 4
    Layout.fillWidth: true

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: root.paramName
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.colors.foreground
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            width: 22
            height: 22
            radius: 5
            color: minusMouse.pressed 
                ? Theme.colors.primary
                : (minusMouse.containsMouse 
                    ? Qt.rgba(0.12, 0.34, 0.82, 0.15)
                    : "transparent")
            border.width: minusMouse.containsMouse ? 1 : 0
            border.color: Qt.rgba(0.12, 0.34, 0.82, 0.4)

            Text {
                anchors.centerIn: parent
                text: "−"
                font.pixelSize: 14
                font.weight: Font.Bold
                color: minusMouse.pressed ? "#FFFFFF" : Theme.colors.primary
            }

            MouseArea {
                id: minusMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    var newValue = Math.max(root.from, slider.value - root.stepSize * 10)
                    slider.value = newValue
                }
            }

            Behavior on color { ColorAnimation { duration: 100 } }
        }

        Rectangle {
            width: 22
            height: 22
            radius: 5
            color: plusMouse.pressed 
                ? Theme.colors.primary
                : (plusMouse.containsMouse 
                    ? Qt.rgba(0.12, 0.34, 0.82, 0.15)
                    : "transparent")
            border.width: plusMouse.containsMouse ? 1 : 0
            border.color: Qt.rgba(0.12, 0.34, 0.82, 0.4)

            Text {
                anchors.centerIn: parent
                text: "+"
                font.pixelSize: 14
                font.weight: Font.Bold
                color: plusMouse.pressed ? "#FFFFFF" : Theme.colors.primary
            }

            MouseArea {
                id: plusMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    var newValue = Math.min(root.to, slider.value + root.stepSize * 10)
                    slider.value = newValue
                }
            }

            Behavior on color { ColorAnimation { duration: 100 } }
        }

        Rectangle {
            width: 44
            height: 22
            radius: 5
            color: inputField.activeFocus ? Qt.rgba(0.12, 0.34, 0.82, 0.08) : Theme.colors.inputBackground
            border.width: 1
            border.color: inputField.activeFocus ? Theme.colors.primary : Theme.colors.border

            TextInput {
                id: inputField
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 11
                font.family: "Consolas"
                color: Theme.colors.foreground
                selectByMouse: true

                text: slider.value.toFixed(root.decimals)

                validator: DoubleValidator {
                    bottom: root.from
                    top: root.to
                    decimals: root.decimals
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        selectAll()
                    }
                }

                onEditingFinished: {
                    var newValue = parseFloat(text)
                    if (!isNaN(newValue)) {
                        newValue = Math.max(root.from, Math.min(root.to, newValue))
                        slider.value = newValue
                    }
                    focus = false
                }

                Keys.onReturnPressed: {
                    focus = false
                }
                Keys.onEscapePressed: {
                    text = slider.value.toFixed(root.decimals)
                    focus = false
                }
            }

            Behavior on border.color { ColorAnimation { duration: 100 } }
        }
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        Layout.preferredHeight: 20
        from: root.from
        to: root.to
        value: root.value
        stepSize: root.stepSize

        onValueChanged: {
            if (Math.abs(value - root.value) > 0.0001) {
                root.value = value
                root.paramValueChanged(value)
            }
        }

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 6
            radius: 3

            color: Theme.isDark ? Qt.rgba(0.12, 0.27, 0.65, 0.25) : Qt.rgba(0, 0.18, 0.65, 0.12)

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: parent.radius

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.isDark ? "#1E56D0" : "#002FA7" }
                    GradientStop { position: 1.0; color: Theme.isDark ? "#3B82F6" : "#1A56DB" }
                }

                Behavior on width {
                    NumberAnimation { duration: 50; easing.type: Easing.OutCubic }
                }
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16
            height: 16
            radius: 8

            color: slider.pressed ? "#3B82F6" : "#1E56D0"
            border.width: 2
            border.color: slider.pressed ? "#60A5FA" : (slider.hovered ? "#3B82F6" : "#1E56D0")

            scale: slider.pressed ? 1.12 : (slider.hovered ? 1.06 : 1.0)
            Behavior on scale {
                NumberAnimation { duration: 60; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 6
                height: parent.height + 6
                radius: width / 2
                color: "transparent"
                border.width: slider.hovered || slider.pressed ? 1 : 0
                border.color: Qt.rgba(0.23, 0.51, 0.96, 0.35)
                visible: slider.hovered || slider.pressed
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.35
                height: parent.height * 0.35
                radius: width / 2
                color: Qt.rgba(1, 1, 1, 0.5)
            }
        }
    }

    onValueChanged: {
        if (Math.abs(value - slider.value) > 0.0001) {
            slider.value = value
        }
    }
}
