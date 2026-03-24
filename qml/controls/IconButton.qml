import QtQuick
import QtQuick.Controls
import "../styles"

Button {
    id: root

    property string iconName: ""
    property string iconText: ""
    property color iconColor: Theme.colors.icon
    property color iconHoverColor: Theme.colors.iconHover
    property int iconSize: 18
    property string tooltip: ""
    property int btnSize: 32
    property bool danger: false

    width: btnSize
    height: btnSize
    flat: true
    padding: 0

    Tooltip {
        id: customTooltip
        text: root.tooltip
    }

    Timer {
        id: showTooltipTimer
        interval: 500
        repeat: false
        onTriggered: {
            if (root.hovered && root.tooltip !== "") {
                customTooltip.show(root)
            }
        }
    }

    onHoveredChanged: {
        if (hovered && tooltip !== "") {
            showTooltipTimer.start()
        } else {
            showTooltipTimer.stop()
            customTooltip.close()
        }
    }

    // ========== 背景 ==========
    background: Rectangle {
        radius: Theme.radius.sm
        color: {
            if (root.danger && root.hovered) return Theme.colors.destructive
            if (root.pressed) return Theme.isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.08)
            if (root.hovered) return Theme.isDark ? Qt.rgba(1,1,1,0.08) : Qt.rgba(0,47,167,0.06)
            return "transparent"
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

    // ========== 内容 ==========
    contentItem: Item {
        implicitWidth: root.btnSize
        implicitHeight: root.btnSize

        // SVG 图标 - 使用 ColoredIcon 实现颜色叠加
        ColoredIcon {
            anchors.centerIn: parent
            source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
            iconSize: root.iconSize
            color: {
                if (root.danger && root.hovered) return "#FFFFFF"
                if (root.hovered) return root.iconHoverColor
                return root.iconColor
            }

            Behavior on color {
                ColorAnimation { duration: Theme.animation.fast }
            }
        }

        // 文本备用图标
        Text {
            anchors.centerIn: parent
            text: root.iconText
            visible: root.iconName === "" && root.iconText !== ""
            color: {
                if (root.danger && root.hovered) return "#FFFFFF"
                return root.hovered ? root.iconHoverColor : root.iconColor
            }
            font.pixelSize: root.iconSize
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter

            Behavior on color {
                ColorAnimation { duration: Theme.animation.fast }
            }
        }
    }

    // ========== 按下缩放 ==========
    scale: pressed ? 0.90 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
    }
}
