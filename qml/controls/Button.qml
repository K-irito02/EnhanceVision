import QtQuick
import QtQuick.Controls
import "../styles"

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
        sm: { height: 30, padding: 10, fontSize: 12 },
        md: { height: 36, padding: 16, fontSize: 13 },
        lg: { height: 44, padding: 24, fontSize: 14 }
    })

    implicitWidth: Math.max(buttonRow.implicitWidth + leftPadding + rightPadding, 80)
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
                if (root.pressed) return Theme.colors.primaryHover
                if (root.hovered) return Theme.colors.primaryHover
                return Theme.colors.primary
            }
            if (v === "secondary") {
                if (root.pressed) return Theme.colors.secondaryHover
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

        // 边框（secondary 和 ghost 悬浮时）
        border.width: root.variant === "secondary" ? 1 : 0
        border.color: {
            if (!root.enabled) return Theme.colors.border
            if (root.hovered) return Theme.colors.borderHover
            return Theme.colors.border
        }

        // 透明度（禁用状态）
        opacity: root.enabled ? 1.0 : 0.5

        // 动画
        Behavior on color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

    // ========== 内容 ==========
    contentItem: Row {
        id: buttonRow
        spacing: 6
        anchors.centerIn: parent

        // 可选图标
        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
            sourceSize: Qt.size(16, 16)
            visible: root.iconName !== ""
            smooth: true
        }

        // 文本
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.pixelSize: root.sizeValues[root.size].fontSize
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
            color: {
                if (!root.enabled) return Theme.colors.mutedForeground
                var v = root.variant
                if (v === "primary") return Theme.colors.primaryForeground
                if (v === "secondary") return Theme.colors.secondaryForeground
                if (v === "destructive") return Theme.colors.destructiveForeground
                return Theme.colors.foreground // ghost
            }
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
            }
        }
    }

    // ========== 按下效果 ==========
    scale: root.pressed ? 0.97 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 80; easing.type: Easing.OutCubic }
    }
}
