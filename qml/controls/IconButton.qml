import QtQuick
import QtQuick.Controls
import "../styles"

/**
 * @brief 现代化图标按钮控件
 * 支持 SVG 图标加载，带悬浮/按下动效
 */
Button {
    id: root

    // ========== 属性 ==========
    /**
     * @brief 图标名称（对应 resources/icons/ 下的 SVG 文件名，不含扩展名）
     */
    property string iconName: ""

    /**
     * @brief 备用：直接传文本字符作为图标
     */
    property string iconText: ""

    /**
     * @brief 图标颜色
     */
    property color iconColor: Theme.colors.icon

    /**
     * @brief 悬浮时图标颜色
     */
    property color iconHoverColor: Theme.colors.iconHover

    /**
     * @brief 图标尺寸
     */
    property int iconSize: 18

    /**
     * @brief 提示文本
     */
    property string tooltip: ""

    /**
     * @brief 按钮尺寸
     */
    property int btnSize: 32

    /**
     * @brief 是否为危险操作按钮
     */
    property bool danger: false

    // ========== 基础设置 ==========
    width: btnSize
    height: btnSize
    flat: true
    padding: 0

    ToolTip.text: tooltip
    ToolTip.visible: hovered && tooltip !== ""
    ToolTip.delay: 600

    // ========== 背景 ==========
    background: Rectangle {
        radius: Theme.radius.sm
        color: {
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

        // SVG 图标
        Image {
            id: svgIcon
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize
            source: root.iconName !== "" ? Theme.icon(root.iconName) : ""
            sourceSize: Qt.size(root.iconSize, root.iconSize)
            visible: root.iconName !== ""
            smooth: true
            antialiasing: true
        }

        // 文本备用图标
        Text {
            anchors.centerIn: parent
            text: root.iconText
            visible: root.iconName === "" && root.iconText !== ""
            color: {
                if (root.danger && root.hovered) return Theme.colors.destructive
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
