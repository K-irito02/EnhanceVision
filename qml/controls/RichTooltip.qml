import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

/**
 * @brief 富文本悬停提示组件
 *
 * 支持多行内容显示，用于模型性能提示等场景。
 * 带箭头指示器，自动边界检测和位置调整。
 */
Popup {
    id: root

    property var contentModel: []   // [{label: "CPU 图片:", value: "120.50s"}, ...]
    property string title: ""
    property string footerNote: ""  // 底部说明文字

    property int arrowSize: 6
    property int tooltipOffset: 8
    property int tooltipPadding: 10

    property color backgroundColor: Theme.colors.tooltip
    property color textColor: Theme.colors.tooltipForeground
    property color borderColor: Theme.colors.tooltipBorder
    property color labelColor: Theme.colors.mutedForeground
    property color valueColor: Theme.colors.tooltipForeground

    parent: Overlay.overlay

    padding: 0
    margins: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    modal: false
    dim: false

    implicitWidth: contentColumn.implicitWidth + tooltipPadding * 2
    implicitHeight: contentColumn.implicitHeight + tooltipPadding * 2

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
            ctx.roundedRect(boxX, boxY, boxWidth, boxHeight, 6, 6)
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

    contentItem: ColumnLayout {
        id: contentColumn
        width: root.implicitWidth
        anchors.centerIn: parent
        anchors.verticalCenterOffset: arrowCanvas.arrowDirection === "up" ? root.arrowSize / 2 : -root.arrowSize / 2
        spacing: 4

        // 标题
        Text {
            visible: root.title !== ""
            text: root.title
            color: root.textColor
            font.pixelSize: 11
            font.weight: Font.DemiBold
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
        }

        // 分隔线
        Rectangle {
            visible: root.title !== "" && repeater.count > 0
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            height: 1
            color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.2)
        }

        // 内容行
        Repeater {
            id: repeater
            model: root.contentModel

            RowLayout {
                spacing: 16
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6

                Text {
                    text: modelData.label || ""
                    color: root.labelColor
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    Layout.alignment: Qt.AlignLeft
                }

                Item { Layout.fillWidth: true; Layout.minimumWidth: 12 }

                Text {
                    text: modelData.value || ""
                    // 统一使用蓝色系
                    color: "#4a9eff"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    Layout.alignment: Qt.AlignRight
                }
            }
        }

        // 底部说明文字
        Rectangle {
            visible: root.footerNote !== ""
            Layout.fillWidth: true
            Layout.topMargin: 4
            height: 1
            color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.15)
        }

        Text {
            visible: root.footerNote !== ""
            text: root.footerNote
            color: Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.6)
            font.pixelSize: 9
            font.italic: true
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 6
            Layout.topMargin: -2
            Layout.maximumWidth: 200
            horizontalAlignment: Text.AlignLeft
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 120 }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 80 }
    }

    function show(targetItem) {
        if (!targetItem || root.contentModel.length === 0) return

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
