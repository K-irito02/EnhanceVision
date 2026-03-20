import QtQuick
import QtQuick.Controls
import "../styles"

Popup {
    id: root

    property string text: ""
    property var target: null

    property int arrowSize: 5
    property int tooltipOffset: 8
    property int tooltipPadding: 4

    property color backgroundColor: Theme.colors.tooltip
    property color textColor: Theme.colors.tooltipForeground
    property color borderColor: Theme.colors.tooltipBorder

    parent: Overlay.overlay

    padding: 0
    margins: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    modal: false
    dim: false

    implicitWidth: textLabel.implicitWidth + tooltipPadding * 2
    implicitHeight: textLabel.implicitHeight + tooltipPadding * 2

    background: Canvas {
        id: arrowCanvas
        width: root.implicitWidth
        height: root.implicitHeight + root.arrowSize

        property string arrowDirection: "up"
        property int arrowOffset: 0

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            ctx.fillStyle = root.backgroundColor.toString()
            ctx.strokeStyle = root.borderColor.toString()
            ctx.lineWidth = 1

            var size = root.arrowSize
            var boxX = 0
            var boxY = arrowDirection === "up" ? size : 0
            var boxWidth = root.implicitWidth
            var boxHeight = root.implicitHeight

            ctx.beginPath()
            ctx.roundedRect(boxX, boxY, boxWidth, boxHeight, 4, 4)
            ctx.fill()
            ctx.stroke()

            var arrowX = root.implicitWidth / 2 + arrowOffset

            ctx.fillStyle = root.backgroundColor.toString()
            ctx.beginPath()

            if (arrowDirection === "up") {
                ctx.moveTo(arrowX - size, size)
                ctx.lineTo(arrowX, 0)
                ctx.lineTo(arrowX + size, size)
            } else {
                ctx.moveTo(arrowX - size, root.implicitHeight)
                ctx.lineTo(arrowX, root.implicitHeight + size)
                ctx.lineTo(arrowX + size, root.implicitHeight)
            }

            ctx.closePath()
            ctx.fill()
        }

        Connections {
            target: root
            function onBackgroundColorChanged() { arrowCanvas.requestPaint() }
            function onBorderColorChanged() { arrowCanvas.requestPaint() }
        }
    }

    contentItem: Text {
        id: textLabel
        text: root.text
        color: root.textColor
        font.pixelSize: 11
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        width: root.implicitWidth
        height: root.implicitHeight
        anchors.centerIn: parent
        anchors.verticalCenterOffset: arrowCanvas.arrowDirection === "up" ? root.arrowSize / 2 + 2 : -root.arrowSize / 2 + 2
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100 }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 80 }
    }

    function show(targetItem) {
        if (!targetItem || !root.text) return

        var globalPos = targetItem.mapToGlobal(targetItem.width / 2, targetItem.height)
        var localPos = Overlay.overlay.mapFromGlobal(globalPos.x, globalPos.y)

        var tooltipW = root.implicitWidth
        var tooltipH = root.implicitHeight + root.arrowSize

        var posX = localPos.x - tooltipW / 2
        var posY = localPos.y + tooltipOffset
        var arrowOffset = 0
        var arrowDirection = "up"

        var overlayW = Overlay.overlay.width
        var overlayH = Overlay.overlay.height

        if (posY + tooltipH > overlayH - 10) {
            var globalPosTop = targetItem.mapToGlobal(targetItem.width / 2, 0)
            var localPosTop = Overlay.overlay.mapFromGlobal(globalPosTop.x, globalPosTop.y)
            posY = localPosTop.y - tooltipH - tooltipOffset
            arrowDirection = "down"
        }

        if (posX < 10) {
            arrowOffset = posX - 10 + tooltipW / 2
            posX = 10
        } else if (posX + tooltipW > overlayW - 10) {
            arrowOffset = posX + tooltipW - (overlayW - 10) - tooltipW / 2
            posX = overlayW - tooltipW - 10
        }

        if (posY < 10) posY = 10

        arrowCanvas.arrowDirection = arrowDirection
        arrowCanvas.arrowOffset = -arrowOffset
        root.x = posX
        root.y = posY
        root.open()
        arrowCanvas.requestPaint()
    }
}
