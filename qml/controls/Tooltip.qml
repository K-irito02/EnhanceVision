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
    property int minWidth: 0
    property int maxWidth: Theme.tooltip.maxWidth
    property int textMaximumLineCount: 3
    property int textElideMode: Text.ElideRight
    property int textHorizontalAlignment: Text.AlignHCenter

    property color backgroundColor: Theme.colors.tooltip
    property color textColor: Theme.colors.tooltipForeground
    property color borderColor: Theme.colors.tooltipBorder

    parent: Overlay.overlay

    padding: 0
    margins: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    modal: false
    dim: false

    readonly property int availableTooltipWidth: Overlay.overlay ? Math.max(120, Overlay.overlay.width - 20) : maxWidth
    implicitWidth: Math.max(minWidth,
                            Math.min(textLabel.implicitWidth + tooltipPadding * 2,
                                     Math.min(maxWidth, availableTooltipWidth)))
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
        wrapMode: Text.WordWrap
        maximumLineCount: root.textMaximumLineCount
        elide: root.textElideMode
        horizontalAlignment: root.textHorizontalAlignment
        verticalAlignment: Text.AlignVCenter
        width: Math.max(0, root.implicitWidth - root.tooltipPadding * 2)
        anchors.centerIn: parent
        anchors.verticalCenterOffset: arrowCanvas.arrowDirection === "up" ? root.arrowSize / 2 + 2 : -root.arrowSize / 2 + 2
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100 }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 80 }
    }

    function _overlayItem() {
        return root.parent ? root.parent : Overlay.overlay
    }

    function show(targetItem) {
        if (!targetItem || !root.text) return

        var overlayItem = _overlayItem()
        if (!overlayItem) return

        var tooltipW = root.implicitWidth
        var tooltipH = root.implicitHeight + root.arrowSize
        var margin = 10
        var bottomAnchor = targetItem.mapToItem(overlayItem, targetItem.width / 2, targetItem.height)
        var topAnchor = targetItem.mapToItem(overlayItem, targetItem.width / 2, 0)

        var posX = bottomAnchor.x - tooltipW / 2
        var posY = bottomAnchor.y + tooltipOffset
        var arrowDirection = "up"

        if (posY + tooltipH > overlayItem.height - margin) {
            posY = topAnchor.y - tooltipH - tooltipOffset
            arrowDirection = "down"
        }

        posX = Math.max(margin, Math.min(posX, overlayItem.width - tooltipW - margin))
        posY = Math.max(margin, Math.min(posY, overlayItem.height - tooltipH - margin))

        var anchorX = arrowDirection === "up" ? bottomAnchor.x : topAnchor.x
        var minArrowX = root.arrowSize + 6
        var maxArrowX = tooltipW - root.arrowSize - 6
        var arrowOffset = Math.max(minArrowX, Math.min(anchorX - posX, maxArrowX)) - tooltipW / 2

        arrowCanvas.arrowDirection = arrowDirection
        arrowCanvas.arrowOffset = arrowOffset
        root.x = posX
        root.y = posY
        root.open()
        arrowCanvas.requestPaint()
    }
}
