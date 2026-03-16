import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 对话框标题栏组件
 *
 * 用于子窗口的自定义标题栏，支持：
 * - 标题文本显示
 * - 拖拽移动窗口
 * - 关闭按钮
 * - 主题样式统一
 */
Rectangle {
    id: root

    property string titleText: ""
    property var windowRef: null
    property bool showDivider: true

    signal closeClicked()

    height: 40
    color: Theme.colors.titleBar

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.colors.titleBarBorder
        visible: root.showDivider
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        z: 0
        onPressed: function(mouse) {
            if (root.windowRef) {
                root.windowRef.startSystemMove()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 8
        spacing: 8
        z: 1

        Text {
            text: root.titleText
            color: Theme.colors.foreground
            font.pixelSize: 14
            font.weight: Font.DemiBold
            Layout.alignment: Qt.AlignVCenter
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        IconButton {
            iconName: "x"
            iconSize: 14
            btnSize: 32
            danger: true
            tooltip: qsTr("关闭")
            onClicked: root.closeClicked()
        }
    }

    Behavior on color {
        ColorAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
    }
}
