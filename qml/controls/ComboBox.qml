import QtQuick
import QtQuick.Controls
import "../styles"

/**
 * @brief 自定义下拉框控件
 * 克莱因蓝主题风格
 */
ComboBox {
    id: root
    model: []

    implicitWidth: 200
    implicitHeight: 36

    // ========== 背景 ==========
    background: Rectangle {
        radius: Theme.radius.md
        color: Theme.colors.input
        border.width: 1
        border.color: {
            if (!root.enabled) return Theme.colors.border
            if (root.pressed || root.popup.visible) return Theme.colors.ring
            if (root.hovered) return Theme.colors.borderHover
            return Theme.colors.inputBorder
        }

        Behavior on border.color {
            ColorAnimation { duration: Theme.animation.fast }
        }
    }

    // ========== 内容显示 ==========
    contentItem: Text {
        leftPadding: 12
        rightPadding: root.indicator.width + 8
        text: root.displayText
        font.pixelSize: 13
        font.weight: Font.Normal
        color: root.enabled ? Theme.colors.foreground : Theme.colors.mutedForeground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    // ========== 下拉箭头 ==========
    indicator: Image {
        x: root.width - width - 10
        y: root.topPadding + (root.availableHeight - height) / 2
        width: 14
        height: 14
        source: Theme.icon("chevron-right")
        sourceSize: Qt.size(14, 14)
        rotation: root.popup.visible ? -90 : 90
        smooth: true

        Behavior on rotation {
            NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
        }
    }

    // ========== 弹出框 ==========
    popup: Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: contentItem.implicitHeight + 8
        padding: 4

        background: Rectangle {
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.md

            layer.enabled: true
            layer.effect: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                    radius: Theme.radius.md + 4
                    z: -1
                }
            }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollBar.vertical: ScrollBar { active: true }
        }
    }

    // ========== 委托项 ==========
    delegate: ItemDelegate {
        width: root.width - 8
        height: 32
        padding: 0
        leftPadding: 12

        contentItem: Text {
            text: modelData
            color: highlighted ? Theme.colors.primaryForeground : Theme.colors.foreground
            font.pixelSize: 13
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        highlighted: root.highlightedIndex === index

        background: Rectangle {
            radius: Theme.radius.xs
            color: {
                if (highlighted) return Theme.colors.primary
                if (hovered) return Theme.colors.accent
                return "transparent"
            }

            Behavior on color {
                ColorAnimation { duration: Theme.animation.fast }
            }
        }
    }
}
