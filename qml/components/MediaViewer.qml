import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 媒体查看器组件（嵌入式占位 / 快捷入口）
 * 
 * 实际的全功能查看器已迁移至 MediaViewerWindow.qml（独立窗口）。
 * 此组件保留为嵌入式占位，提供 openViewer() 方法快捷打开独立窗口。
 */
Rectangle {
    id: root
    color: Theme.isDark ? "#040608" : "#0A1020"

    /** @brief 要查看的媒体文件列表 */
    property var mediaFiles: []

    /** @brief 是否为消息模式（启用原图对比） */
    property bool messageMode: false

    /** @brief 打开独立查看器窗口 */
    function openViewer(index) {
        viewerWindow.mediaFiles = root.mediaFiles
        viewerWindow.openAt(index || 0)
    }

    MediaViewerWindow {
        id: viewerWindow
        messageMode: root.messageMode
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 64; height: 64
            radius: 32
            color: Qt.rgba(1, 1, 1, 0.08)

            ColoredIcon {
                anchors.centerIn: parent
                source: Theme.icon("monitor")
                iconSize: 28
                color: Qt.rgba(1, 1, 1, 0.6)
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
            text: qsTr("点击文件缩略图以预览")
            color: Qt.rgba(1, 1, 1, 0.4)
            font.pixelSize: 12
        }
    }
}
