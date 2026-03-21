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
    property alias closeButton: closeButton

    signal closeClicked()

    height: 40
    color: Theme.colors.card

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.colors.border
        visible: root.showDivider
    }

    RowLayout {
        id: titleBarRow
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 8
        spacing: 8

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
            
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: function(mouse) {
                    if (root.windowRef) {
                        root.windowRef.startSystemMove()
                    }
                }
            }
        }

        IconButton {
            id: closeButton
            iconName: "x"
            iconSize: 14
            btnSize: 32
            danger: true
            tooltip: qsTr("关闭")
            Layout.alignment: Qt.AlignVCenter
            onClicked: {
                if (root.windowRef) {
                    root.windowRef.close()
                }
                root.closeClicked()
            }
        }
    }

    MouseArea {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        acceptedButtons: Qt.LeftButton
        onClicked: {
            if (root.windowRef) {
                root.windowRef.close()
            }
            root.closeClicked()
        }
    }
}
