import QtQuick
import QtQuick.Controls

Button {
    id: root
    property string iconText: ""
    property string iconSource: ""
    property color iconColor: "#ffffff"
    property string tooltip: ""

    width: 32
    height: 32
    flat: true
    ToolTip.text: tooltip
    ToolTip.visible: hovered

    background: Rectangle {
        color: root.hovered ? "#3d3d3d" : "transparent"
        radius: 4
    }

    contentItem: Text {
        text: root.iconText !== "" ? root.iconText : (root.iconSource !== "" ? root.iconSource.charAt(0).toUpperCase() : "?")
        color: root.iconColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
