import QtQuick
import QtQuick.Controls
import "../styles"
import "../controls"

/**
 * @brief 自定义按钮控件
 * 支持多种样式变体：primary, secondary, destructive, ghost
 * 参考功能设计文档 11.6.1 节
 */
Button {
    id: root

    // ========== 属性 ==========
    /**
     * @brief 按钮样式变体
     * 可选值: "primary", "secondary", "destructive", "ghost"
     */
    property string variant: "primary"

    /**
     * @brief 按钮大小
     * 可选值: "sm", "md", "lg"
     */
    property string size: "md"

    /**
     * @brief 图标名称（可选，显示在文字左侧）
     */
    property string iconName: ""

    // ========== 尺寸计算 ==========
    readonly property var sizeValues: ({
        sm: { height: 28, padding: 10, fontSize: 12, iconSize: 14 },
        md: { height: 34, padding: 14, fontSize: 13, iconSize: 16 },
        lg: { height: 42, padding: 20, fontSize: 14, iconSize: 18 }
    })

    implicitWidth: buttonRow.implicitWidth + sizeValues[size].padding * 2
    implicitHeight: sizeValues[size].height

    leftPadding: sizeValues[size].padding
    rightPadding: sizeValues[size].padding
    topPadding: 0
    bottomPadding: 0

    // ========== 背景 ==========
    background: Rectangle {
        id: bg

        radius: Theme.radius.md
        color: {
            var v = root.variant
            if (!root.enabled) {
                if (v === "ghost") return "transparent"
                return Theme.isDark ? Qt.rgba(1,1,1,0.05) : Qt.rgba(0,0,0,0.04)
            }
            if (v === "primary") {
                if (root.pressed) return Qt.darker(Theme.colors.primary, 1.1)
                if (root.hovered) return Theme.colors.primaryHover
                return Theme.colors.primary
            }
            if (v === "secondary") {
                if (root.pressed) return Theme.isDark ? Qt.rgba(1,1,1,0.12) : Qt.rgba(0,0,0,0.08)
                if (root.hovered) return Theme.colors.secondaryHover
                return Theme.colors.secondary
            }
            if (v === "destructive") {
                if (root.pressed) return Qt.darker(Theme.colors.destructive, 1.1)
                if (root.hovered) return Qt.lighter(Theme.colors.destructive, 1.1)
                return Theme.colors.destructive
            }
            // ghost
            if (root.pressed) return Theme.isDark ? Qt.rgba(1,1,1,0.1) : Qt.rgba(0,47,167,0.08)
            if (root.hovered) return Theme.isDark ? Qt.rgba(1,1,1,0.06) : Qt.rgba(0,47,167,0.05)
            return "transparent"
        }

        // 边框
        border.width: root.variant === "secondary" || root.variant === "ghost" ? 1 : 0
        border.color: {
            if (!root.enabled) return Theme.colors.border
            if (root.hovered) return Theme.colors.borderHover
            return Theme.colors.border
        }

        // 动画
        Behavior on color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

    // ========== 内容 ==========
    contentItem: Row {
        id: buttonRow
        spacing: root.iconName !== "" ? 6 : 0
        anchors.centerIn: parent

        // 可选图标 - 使用 ColoredIcon
        ColoredIcon {
            anchors.verticalCenter: parent.verticalCenter
            source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
            iconSize: root.sizeValues[root.size].iconSize
            color: textColor
            visible: root.iconName !== ""
        }

        // 文本
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.pixelSize: root.sizeValues[root.size].fontSize
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
            color: root.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
            }
        }
    }

    // ========== 文本颜色 ==========
    property color textColor: {
        if (!root.enabled) return Theme.colors.mutedForeground
        var v = root.variant
        if (v === "primary") return Theme.colors.primaryForeground
        if (v === "secondary") return Theme.colors.secondaryForeground
        if (v === "destructive") return Theme.colors.destructiveForeground
        return Theme.colors.foreground // ghost
    }

    // ========== 按下效果 ==========
    scale: root.pressed ? 0.97 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
    }
}
