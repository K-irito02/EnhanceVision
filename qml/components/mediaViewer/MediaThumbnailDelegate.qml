import QtQuick
import "../../styles"
import "../../controls"
import ".."
import "."
import "MediaViewerHelpers.js" as MediaViewerHelpers

Item {
    id: root

    required property int index
    required property int origIndex
    required property string itemId
    required property string filePath
    required property string fileName
    required property int mediaType
    required property string thumbnail
    required property int status
    required property string resultPath
    required property string originalPath
    required property string processedThumbnailId

    property bool messageMode: false
    property bool contextMenuEnabled: false
    property bool selected: false
    property bool deleteEnabled: true
    property bool showHoverBorder: true
    property real cornerRadius: Theme.radius.md
    property color backgroundColor: Theme.colors.surface
    property int requestedSourceSize: Math.max(width, height) * 2
    property bool _deleteInProgress: false
    property real _deleteOpacity: 1.0
    property real _imageOpacity: 1.0

    readonly property bool isPending: status === 0
    readonly property bool isProcessing: status === 1
    readonly property bool isSuccess: status === 2
    readonly property bool isFailed: status === 3
    readonly property bool isCancelled: status === 4
    readonly property bool isPaused: status === 5
    readonly property bool isRecoverable: status === 6
    readonly property bool isUnavailable: !isSuccess
    readonly property bool canRetry: isFailed || isCancelled
    readonly property bool _showDeleteButton: deleteEnabled && (thumbMouse.containsMouse || deleteMouse.containsMouse)

    signal activated(int origIndex)
    signal deleteRequested(int origIndex)
    signal contextMenuRequested(real globalX, real globalY, int filteredIndex, int status)

    Behavior on _deleteOpacity {
        NumberAnimation { duration: 200; easing.type: Easing.InOutCubic }
    }

    Behavior on _imageOpacity {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }

    Timer {
        id: deleteDebounceTimer
        interval: 500
        repeat: false
        onTriggered: {
            root._deleteInProgress = false
            root._deleteOpacity = 1.0
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: root.cornerRadius
        color: "transparent"
        border.width: {
            if (root.selected) {
                return 2
            }
            if (root.messageMode && root.isRecoverable) {
                return 2
            }
            if (root.messageMode && root.isUnavailable) {
                return 2
            }
            if (root.showHoverBorder && thumbMouse.containsMouse) {
                return 2
            }
            return 0
        }
        border.color: {
            if (root.selected || (root.showHoverBorder && thumbMouse.containsMouse)) {
                return Theme.colors.primary
            }
            if (root.messageMode && root.isRecoverable) {
                return Theme.colors.primary
            }
            if (root.messageMode && root.isUnavailable) {
                return Theme.colors.destructive
            }
            return "transparent"
        }
        z: 5

        Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
        Behavior on border.width { NumberAnimation { duration: Theme.animation.fast } }
    }

    Rectangle {
        id: thumbRect
        anchors.fill: parent
        anchors.margins: 2
        radius: root.cornerRadius
        color: root.backgroundColor
        opacity: root._deleteOpacity
        clip: true

        ThumbnailStatusImage {
            anchors.fill: parent
            baseThumbnailId: MediaViewerHelpers.baseThumbnailKey({
                filePath: root.filePath,
                originalPath: root.originalPath
            })
            preferredThumbnailId: root.processedThumbnailId !== "" ? root.processedThumbnailId : MediaViewerHelpers.preferredThumbnailKey({
                resultPath: root.resultPath
            })
            preferPreferredThumbnail: root.messageMode && root.isSuccess && preferredThumbnailId !== ""
            mediaType: root.mediaType
            requestedSourceSize: Qt.size(root.requestedSourceSize, root.requestedSourceSize)
            fillMode: Image.PreserveAspectCrop
            cornerRadius: root.cornerRadius
            backgroundColor: root.backgroundColor
            showFailureBorder: true
            opacity: root._imageOpacity
        }

        Rectangle {
            visible: root.mediaType === 1 && !root.isFailed && !root.isPending && !root.isProcessing
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 4
            width: 20
            height: 16
            radius: 3
            color: Qt.rgba(0, 0, 0, 0.65)

            ColoredIcon {
                anchors.centerIn: parent
                source: Theme.icon("play")
                iconSize: 10
                color: Theme.colors.textOnPrimary
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Qt.rgba(0, 0, 0, 0.5)
            visible: root.messageMode && (root.isProcessing || root.isPaused || root.isRecoverable)

            Column {
                anchors.centerIn: parent
                spacing: 2

                ColoredIcon {
                    id: processingIcon
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: root.isRecoverable ? Theme.icon("refresh-cw") : (root.isPaused ? Theme.icon("pause") : Theme.icon("loader"))
                    iconSize: 20
                    color: root.isRecoverable ? Theme.colors.primary : (root.isPaused ? Theme.colors.warning : Theme.colors.textOnPrimary)
                    opacity: 0.9
                    rotation: (root.isPaused || root.isRecoverable) ? 0 : _loaderRotation
                    property real _loaderRotation: 0

                    RotationAnimation on _loaderRotation {
                        from: 0
                        to: 360
                        duration: 1500
                        loops: Animation.Infinite
                        running: root.isProcessing && !root.isPaused && !root.isRecoverable
                    }
                }
            }
        }
    }

    MouseArea {
        id: thumbMouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        propagateComposedEvents: true
        cursorShape: (!root.messageMode || root.isSuccess) ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: function(mouse) {
            var deleteButtonArea = Qt.rect(parent.width - 24, 4, 20, 20)
            var inDeleteButton = mouse.x >= deleteButtonArea.x && mouse.x <= deleteButtonArea.x + deleteButtonArea.width
                    && mouse.y >= deleteButtonArea.y && mouse.y <= deleteButtonArea.y + deleteButtonArea.height
            if (inDeleteButton && root._showDeleteButton) {
                mouse.accepted = false
            }
        }

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton && root.contextMenuEnabled) {
                var globalPos = mapToGlobal(mouse.x, mouse.y)
                root.contextMenuRequested(globalPos.x, globalPos.y, root.index, root.status)
                return
            }

            if (!root.messageMode || root.isSuccess) {
                root.activated(root.origIndex)
            }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 4
        width: 20
        height: 20
        radius: 10
        color: deleteMouse.containsMouse ? Theme.colors.destructive : Qt.rgba(0, 0, 0, 0.7)
        visible: root._showDeleteButton
        z: 10
        opacity: visible ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 100 } }
        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }

        Text {
            anchors.centerIn: parent
            text: "\u00D7"
            color: Theme.colors.textOnPrimary
            font.pixelSize: 14
            font.weight: Font.Bold
        }

        MouseArea {
            id: deleteMouse
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            cursorShape: root._deleteInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor

            onClicked: {
                if (root._deleteInProgress) {
                    return
                }
                root._deleteInProgress = true
                root._deleteOpacity = 0.3
                deleteDebounceTimer.start()
                root.deleteRequested(root.origIndex)
            }
        }
    }
}
