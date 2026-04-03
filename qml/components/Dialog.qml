import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

Item {
    id: root

    visible: false
    z: 9999

    enum DialogType {
        Info,
        Success,
        Warning,
        Error,
        Confirm
    }

    property string title: ""
    property string message: ""
    property int type: Dialog.DialogType.Info
    property string primaryButtonText: qsTr("确定")
    property string secondaryButtonText: qsTr("取消")
    property bool showSecondaryButton: true

    signal primaryButtonClicked()
    signal secondaryButtonClicked()
    signal closed()

    readonly property string dialogIconName: {
        switch (type) {
            case Dialog.DialogType.Success: return "check-circle"
            case Dialog.DialogType.Warning: return "alert-triangle"
            case Dialog.DialogType.Error:   return "x-circle"
            case Dialog.DialogType.Confirm: return "help-circle"
            default:                        return "info"
        }
    }

    readonly property color dialogIconBgColor: {
        switch (type) {
            case Dialog.DialogType.Success: return Theme.colors.successSubtle
            case Dialog.DialogType.Warning: return Theme.isDark ? Qt.rgba(245, 158, 11, 0.2) : Qt.rgba(245, 158, 11, 0.15)
            case Dialog.DialogType.Error:   return Theme.colors.destructiveSubtle
            default:                        return Theme.colors.primarySubtle
        }
    }

    readonly property color dialogIconColor: {
        switch (type) {
            case Dialog.DialogType.Success: return Theme.colors.success
            case Dialog.DialogType.Warning: return Theme.colors.warning
            case Dialog.DialogType.Error:   return Theme.colors.destructive
            default:                        return Theme.colors.primary
        }
    }

    property point dragOffset: Qt.point(0, 0)
    property bool isDragging: false

    function showDialog(dialogTitle, dialogMessage, dialogType, primaryBtnText) {
        title = dialogTitle || ""
        message = dialogMessage || ""
        type = dialogType !== undefined ? dialogType : Dialog.DialogType.Info
        if (primaryBtnText) primaryButtonText = primaryBtnText

        dragOffset = Qt.point(0, 0)
        visible = true
        opacity = 0
        dialogRect.scale = 0.92
        showAnimation.start()
    }

    function hide() {
        hideAnimation.start()
    }

    ParallelAnimation {
        id: showAnimation
        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { target: dialogRect; property: "scale"; from: 0.92; to: 1; duration: 250; easing.type: Easing.OutCubic }
    }

    ParallelAnimation {
        id: hideAnimation
        NumberAnimation { target: root; property: "opacity"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        NumberAnimation { target: dialogRect; property: "scale"; from: 1; to: 0.95; duration: 150; easing.type: Easing.InCubic }
        onFinished: { visible = false; closed() }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.7 : 0.5)
        
        MouseArea {
            anchors.fill: parent
            onClicked: root.hide()
        }
    }

    Rectangle {
        id: dialogRect
        x: __clampedCenterX + dragOffset.x
        y: __clampedCenterY + dragOffset.y
        width: Math.min(380, parent.width - 48)
        implicitHeight: Math.min(contentColumn.implicitHeight + 36, parent.height * 0.9)
        radius: Theme.radius.xl
        color: Theme.colors.card
        border.color: Theme.colors.cardBorder
        border.width: 1

        readonly property real __clampedCenterX: {
            var cx = (parent.width - width) / 2
            return Math.max(0, Math.min(cx, parent.width - width))
        }

        readonly property real __clampedCenterY: {
            var cy = (parent.height - implicitHeight) / 2
            return Math.max(0, Math.min(cy, parent.height - implicitHeight))
        }

        Flickable {
            id: contentFlickable
            anchors.fill: parent
            anchors.margins: 18
            contentHeight: contentColumn.implicitHeight
            contentWidth: width
            clip: true
            interactive: contentHeight > height

            ColumnLayout {
                id: contentColumn
                width: contentFlickable.contentWidth
                spacing: 12

                RowLayout {
                    id: titleRow
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        width: 36; height: 36
                        radius: Theme.radius.lg
                        color: dialogIconBgColor

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon(dialogIconName)
                            iconSize: 18
                            color: dialogIconColor
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: title
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: Theme.colors.foreground
                        elide: Text.ElideRight
                    }

                    IconButton {
                        iconName: "x"
                        iconSize: 12
                        btnSize: 24
                        onClicked: root.hide()
                    }
                }

                MouseArea {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    cursorShape: Qt.SizeAllCursor

                    property point lastMousePos: Qt.point(0, 0)

                    onPressed: function(mouse) {
                        lastMousePos = Qt.point(mouse.x, mouse.y)
                        isDragging = true
                    }

                    onReleased: {
                        isDragging = false
                    }

                    onPositionChanged: function(mouse) {
                        if (isDragging) {
                            var offsetX = dragOffset.x + mouse.x - lastMousePos.x
                            var offsetY = dragOffset.y + mouse.y - lastMousePos.y

                            var maxOffsetX = root.parent.width - dialogRect.width - dialogRect.__clampedCenterX
                            var maxOffsetY = root.parent.height - dialogRect.implicitHeight - dialogRect.__clampedCenterY

                            dragOffset = Qt.point(
                                Math.max(-dialogRect.__clampedCenterX, Math.min(offsetX, maxOffsetX)),
                                Math.max(-dialogRect.__clampedCenterY, Math.min(offsetY, maxOffsetY))
                            )
                        }
                    }
                }
                
                Text {
                    Layout.fillWidth: true
                    text: message
                    font.pixelSize: 13
                    color: Theme.colors.mutedForeground
                    wrapMode: Text.Wrap
                    lineHeight: 1.5
                    leftPadding: 48
                }

                Item { height: 2 }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Item { Layout.fillWidth: true }

                    Button {
                        visible: showSecondaryButton
                        variant: "secondary"
                        text: secondaryButtonText
                        size: "sm"
                        onClicked: { root.secondaryButtonClicked(); root.hide() }
                    }

                    Button {
                        variant: type === Dialog.DialogType.Error ? "destructive" : "primary"
                        text: primaryButtonText
                        size: "sm"
                        onClicked: { root.primaryButtonClicked(); root.hide() }
                    }
                }
            }
        }
    }
}
