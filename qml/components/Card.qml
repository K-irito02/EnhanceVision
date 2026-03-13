import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../styles"

/**
 * @brief 自定义卡片容器控件
 * 参考 ui-design.md
 */
Rectangle {
    id: root

    // ========== 属性 ==========
    /**
     * @brief 卡片内容区域
     * 可通过 contentItem 访问
     */
    property alias content: contentItem.children

    /**
     * @brief 是否显示阴影
     */
    property bool showShadow: true

    /**
     * @brief 是否可点击
     */
    property bool clickable: false

    /**
     * @brief 点击信号
     */
    signal clicked()

    // ========== 基础样式 ==========
    color: Theme.colors.card
    radius: Theme.radius.lg
    border.width: 1
    border.color: Theme.colors.cardBorder

    implicitWidth: 280
    implicitHeight: contentItem.implicitHeight + Theme.spacing._4 * 2

    // ========== 阴影效果 ==========
    layer.enabled: showShadow
    layer.effect: DropShadow {
        horizontalOffset: 0
        verticalOffset: 2
        radius: 8
        samples: 16
        color: Theme.isDark ? "#40000000" : "#20000000"
    }

    // ========== 内容区域 ==========
    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: Theme.spacing._4
        implicitHeight: childrenRect.height
    }

    // ========== 点击区域 ==========
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.clickable
        hoverEnabled: root.clickable
        cursorShape: root.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: root.clicked()
    }

    // ========== 悬停效果 ==========
    scale: mouseArea.pressed ? 0.98 : (mouseArea.containsMouse && root.clickable ? 1.01 : 1.0)
    Behavior on scale {
        NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
    }

    opacity: mouseArea.containsMouse && root.clickable ? 0.95 : 1.0
    Behavior on opacity {
        NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
    }

    // ========== 动画 ==========
    Behavior on color {
        ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
    }
    Behavior on border.color {
        ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
    }
}
