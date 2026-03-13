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

    // ========== 尺寸计算 ==========
    readonly property var sizeValues: ({
        sm: { width: 0, height: 28, padding: 8 },
        md: { width: 0, height: 36, padding: 16 },
        lg: { width: 0, height: 44, padding: 24 }
    })

    implicitWidth: Math.max(contentItem.implicitWidth + leftPadding + rightPadding, 80)
    implicitHeight: sizeValues[size].height

    leftPadding: sizeValues[size].padding
    rightPadding: sizeValues[size].padding
    topPadding: 0
    bottomPadding: 0

    // ========== 背景 ==========
    background: Rectangle {
        id: bg

        readonly property var variantColors: ({
            primary: {
                default: Theme.colors.primary,
                hover: Theme.colors.primaryHover,
                pressed: Theme.colors.primaryHover,
                disabled: Theme.colors.primary
            },
            secondary: {
                default: Theme.colors.secondary,
                hover: Theme.colors.secondaryHover,
                pressed: Theme.colors.secondaryHover,
                disabled: Theme.colors.secondary
            },
            destructive: {
                default: Theme.colors.destructive,
                hover: Theme.colors.destructive,
                pressed: Theme.colors.destructive,
                disabled: Theme.colors.destructive
            },
            ghost: {
                default: "transparent",
                hover: Theme.colors.accent,
                pressed: Theme.colors.accent,
                disabled: "transparent"
            }
        })

        readonly property var colors: variantColors[root.variant]

        radius: Theme.radius.md
        color: {
            if (!root.enabled) return colors.disabled
            if (root.pressed) return colors.pressed
            if (root.hovered) return colors.hover
            return colors.default
        }

        // 边框（仅 secondary 变体有边框）
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
        Behavior on border.color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

    // ========== 内容 ==========
    contentItem: Text {
        id: textItem

        text: root.text
        font.pixelSize: size === "sm" ? Theme.typography.size.xs : Theme.typography.size.sm
        font.weight: Font.Medium
        color: {
            if (!root.enabled) {
                return Theme.colors.mutedForeground
            }
            switch (root.variant) {
                case "primary":
                    return Theme.colors.primaryForeground
                case "secondary":
                    return Theme.colors.secondaryForeground
                case "destructive":
                    return Theme.colors.destructiveForeground
                case "ghost":
                    return Theme.colors.foreground
                default:
                    return Theme.colors.primaryForeground
            }
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        // 动画
        Behavior on color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

    // ========== 按下效果 ==========
    scale: root.pressed ? 0.98 : 1.0
    Behavior on scale {
        NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
    }
}
