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
            case Dialog.DialogType.Warning: return Theme.colors.warningSubtle
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

    property point dialogPos: Qt.point(0, 0)
    property bool isDragging: false

    function showDialog(dialogTitle, dialogMessage, dialogType, primaryBtnText) {
        title = dialogTitle || ""
        message = dialogMessage || ""
        type = dialogType !== undefined ? dialogType : Dialog.DialogType.Info
        if (primaryBtnText) primaryButtonText = primaryBtnText
        
        dialogPos = Qt.point(
            (parent.width - dialogRect.width) / 2,
            (parent.height - dialogRect.height) / 2
        )
        
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
        x: dialogPos.x
        y: dialogPos.y
        width: Math.min(440, parent.width - 48)
        implicitHeight: contentColumn.implicitHeight + 48
        radius: Theme.radius.xxl
        color: Theme.colors.card
        border.color: Theme.colors.cardBorder
        border.width: 1

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            RowLayout {
                id: titleRow
                Layout.fillWidth: true
                spacing: 14

                Rectangle {
                    width: 44; height: 44
                    radius: Theme.radius.xl
                    color: dialogIconBgColor

                    Image {
                        anchors.centerIn: parent
                        width: 22; height: 22
                        source: Theme.icon(dialogIconName)
                        sourceSize: Qt.size(22, 22)
                        smooth: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: title
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: Theme.colors.foreground
                    elide: Text.ElideRight
                }

                IconButton {
                    iconName: "x"
                    iconSize: 14
                    btnSize: 28
                    onClicked: root.hide()
                }
            }
            
            MouseArea {
                Layout.fillWidth: true
                Layout.preferredHeight: titleRow.height
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
                        var newX = dialogPos.x + mouse.x - lastMousePos.x
                        var newY = dialogPos.y + mouse.y - lastMousePos.y
                        
                        newX = Math.max(0, Math.min(newX, root.parent.width - dialogRect.width))
                        newY = Math.max(0, Math.min(newY, root.parent.height - dialogRect.height))
                        
                        dialogPos = Qt.point(newX, newY)
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: message
                font.pixelSize: 14
                color: Theme.colors.mutedForeground
                wrapMode: Text.Wrap
                lineHeight: 1.6
                leftPadding: 58
            }

            Item { height: 4 }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                Button {
                    visible: showSecondaryButton
                    variant: "secondary"
                    text: secondaryButtonText
                    size: "md"
                    onClicked: { root.secondaryButtonClicked(); root.hide() }
                }

                Button {
                    variant: type === Dialog.DialogType.Error ? "destructive" : "primary"
                    text: primaryButtonText
                    size: "md"
                    onClicked: { root.primaryButtonClicked(); root.hide() }
                }
            }
        }
    }
}
