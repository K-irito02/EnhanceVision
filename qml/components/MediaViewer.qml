import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

/**
 * @brief 媒体查看器组件
 * 全屏预览媒体文件
 */
Rectangle {
    id: root
    color: Theme.isDark ? "#040608" : "#0A1020"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 64; height: 64
            radius: 32
            color: Qt.rgba(1, 1, 1, 0.08)

            Image {
                anchors.centerIn: parent
                width: 28; height: 28
                source: Theme.icon("monitor")
                sourceSize: Qt.size(28, 28)
                smooth: true
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("媒体查看器")
            color: Qt.rgba(1, 1, 1, 0.8)
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("双击文件以全屏预览")
            color: Qt.rgba(1, 1, 1, 0.4)
            font.pixelSize: 12
        }
    }
}
