import QtQuick
import QtQuick.Controls
import "../styles"

/**
 * @brief 自定义文本输入框控件
 * 参考功能设计文档 11.6.2 节
 */
TextField {
    id: root

    // ========== 属性 ==========
    /**
     * @brief 输入框大小
     * 可选值: "sm", "md", "lg"
     */
    property string size: "md"

    // ========== 尺寸计算 ==========
    readonly property var sizeValues: ({
        sm: { height: 32, padding: 8 },
        md: { height: 40, padding: 12 },
        lg: { height: 48, padding: 16 }
    })

    implicitWidth: 200
    implicitHeight: sizeValues[size].height

    leftPadding: sizeValues[size].padding
    rightPadding: sizeValues[size].padding
    topPadding: 0
    bottomPadding: 0

    // ========== 字体 ==========
    font: Theme.typography.input
    color: root.enabled ? Theme.colors.foreground : Theme.colors.mutedForeground
    placeholderTextColor: Theme.colors.mutedForeground
    selectionColor: Theme.colors.primarySubtle
    selectedTextColor: Theme.colors.foreground

    // ========== 背景 ==========
    background: Rectangle {
        id: bg

        radius: Theme.radius.md
        color: root.enabled ? Theme.colors.input : Theme.colors.muted
        border.width: 1
        border.color: {
            if (!root.enabled) return Theme.colors.border
            if (root.activeFocus) return Theme.colors.ring
            if (root.hovered) return Theme.colors.borderHover
            return Theme.colors.inputBorder
        }

        // 动画
        Behavior on color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

}
