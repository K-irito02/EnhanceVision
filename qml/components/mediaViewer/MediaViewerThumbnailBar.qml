import QtQuick
import QtQuick.Controls
import "../../styles"
import "."

Rectangle {
    id: root

    property var viewer: null
    property bool deleteEnabled: true

    height: 60
    color: Theme.colors.mediaControlBg

    MediaThumbnailAdapter {
        id: adapter
        mediaModel: root.viewer ? root.viewer.mediaFiles : null
        showFailedFiles: true
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.colors.mediaControlBorder
    }

    ListView {
        id: thumbListView
        anchors.fill: parent
        anchors.margins: 6
        orientation: ListView.Horizontal
        spacing: 6
        clip: true
        model: adapter.model
        currentIndex: root.viewer ? root.viewer.currentIndex : -1

        delegate: MediaThumbnailDelegate {
            width: 48
            height: 48
            selected: root.viewer ? index === root.viewer.currentIndex : false
            messageMode: root.viewer ? root.viewer.messageMode : false
            deleteEnabled: root.deleteEnabled
            requestedSourceSize: 96
            showHoverBorder: false
            onActivated: function(origIndex) {
                if (root.viewer) {
                    root.viewer.currentIndex = origIndex
                }
            }
            onDeleteRequested: function(origIndex) {
                if (root.viewer) {
                    root.viewer.fileRemoved(root.viewer.messageId, origIndex)
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.NoButton
            onWheel: function(wheel) {
                var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                thumbListView.contentX = Math.max(0, Math.min(thumbListView.contentX - delta, thumbListView.contentWidth - thumbListView.width))
                wheel.accepted = true
            }
        }
    }
}
