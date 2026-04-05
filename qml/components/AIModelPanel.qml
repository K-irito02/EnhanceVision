import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision.Controllers
import "../stores"
import "../styles"
import "../controls"

/**
 * @brief AI 模型选择面板
 *
 * 显示模型类别列表和每个类别下的可用模型，
 * 从 ModelRegistry 动态加载数据。
 * 使用 AIModelConfigStore 管理选中的模型状态。
 */
ColumnLayout {
    id: root

    property string selectedModelId: AIModelConfigStore.currentModelId
    property string selectedCategory: ""
    property var registry: processingController ? processingController.modelRegistry : null

    signal modelSelected(string modelId, string category)

    spacing: 8

    // ========== 类别标签 ==========
    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        ColoredIcon {
            source: Theme.icon("sparkles")
            iconSize: 14
            color: Theme.colors.foreground
        }

        Text {
            text: qsTr("AI 模型类别")
            color: Theme.colors.foreground
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }

    // ========== 类别网格（不参与滚动）==========
    Flow {
        id: categoryFlow
        Layout.fillWidth: true
        spacing: 6

        Repeater {
            id: categoryRepeater
            model: registry ? registry.getCategories() : []

            Rectangle {
                width: catLabel.implicitWidth + 20
                height: 28
                radius: Theme.radius.md
                color: root.selectedCategory === modelData.id
                       ? Theme.colors.primary
                       : Theme.colors.muted
                border.width: root.selectedCategory === modelData.id ? 0 : 1
                border.color: Theme.colors.border

                Text {
                    id: catLabel
                    anchors.centerIn: parent
                    text: modelData.name + " (" + modelData.count + ")"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: root.selectedCategory === modelData.id
                           ? "#FFFFFF"
                           : Theme.colors.foreground
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.selectedCategory = modelData.id
                        modelListView.model = registry.getModelsByCategoryStr(modelData.id)
                    }
                }

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
            }
        }
    }

    // ========== 分隔线 ==========
    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Theme.colors.border
        visible: root.selectedCategory !== ""
    }

    // ========== 模型列表（独立滚动区域）==========
    ListView {
        id: modelListView
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: []
        spacing: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: true

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            policy: ScrollBar.AsNeeded
            interactive: true
        }

        delegate: Rectangle {
            width: modelListView.width - (verticalScrollBar.visible ? verticalScrollBar.width + 4 : 0)
            height: modelItemContent.implicitHeight + 16
            radius: Theme.radius.md
            color: root.selectedModelId === modelData.id
                   ? Theme.colors.primarySubtle
                   : (modelHover.hovered ? Theme.colors.primarySubtle : "transparent")
            border.width: root.selectedModelId === modelData.id ? 2 : 1
            border.color: root.selectedModelId === modelData.id
                          ? Theme.colors.primary
                          : Theme.colors.border

            ColumnLayout {
                id: modelItemContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        id: modelNameText
                        text: modelData.name || ""
                        color: Theme.colors.foreground
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }

                    Rectangle {
                        id: sizeTag
                        width: sizeLabel.implicitWidth + 12
                        height: 18
                        radius: Theme.radius.sm
                        color: Theme.colors.muted
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            id: sizeLabel
                            anchors.centerIn: parent
                            text: modelData.sizeStr || ""
                            font.pixelSize: 9
                            color: Theme.colors.mutedForeground
                        }
                    }
                }

                Text {
                    id: modelDescText
                    text: modelData.description || ""
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    visible: text !== ""
                }
            }

            HoverHandler {
                id: modelHover
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: {
                    AIModelConfigStore.selectModel(modelData.id)
                    root.modelSelected(modelData.id, root.selectedCategory)
                }
                onEntered: {
                    perfTooltipTimer.targetItem = parent
                    perfTooltipTimer.targetModelId = modelData.id || ""
                    perfTooltipTimer.restart()
                }
                onExited: {
                    perfTooltipTimer.stop()
                    perfTooltip.close()
                }
            }
        }

        // 未选择类别的提示
        Text {
            visible: root.selectedCategory === ""
            text: qsTr("请选择一个模型类别")
            color: Theme.colors.mutedForeground
            font.pixelSize: 12
            anchors.centerIn: parent
        }
    }

    // ========== 模型性能提示 ==========
    RichTooltip {
        id: perfTooltip
    }

    Timer {
        id: perfTooltipTimer
        interval: 600
        repeat: false

        property var targetItem: null
        property string targetModelId: ""

        onTriggered: {
            if (!targetItem || targetModelId === "") return
            if (typeof taskTimeEstimator === "undefined") return

            // 模拟处理标准文件，使用 estimateMessageTotalTime 预测时间
            var standardImage = [{
                width: 1920,
                height: 1080,
                isVideo: false,
                durationMs: 0,
                fps: 30.0
            }]
            var standardVideo = [{
                width: 1920,
                height: 1080,
                isVideo: true,
                durationMs: 10000,  // 10秒
                fps: 30.0
            }]

            // mode=1 表示 AI 处理模式
            var gpuImageSec = taskTimeEstimator.estimateMessageTotalTime(1, true, targetModelId, standardImage)
            var gpuVideoSec = taskTimeEstimator.estimateMessageTotalTime(1, true, targetModelId, standardVideo)
            var cpuImageSec = taskTimeEstimator.estimateMessageTotalTime(1, false, targetModelId, standardImage)
            var cpuVideoSec = taskTimeEstimator.estimateMessageTotalTime(1, false, targetModelId, standardVideo)

            function _fmtTime(sec) {
                if (sec < 60) return sec.toFixed(1) + "s"
                if (sec < 3600) return (sec / 60).toFixed(1) + "min"
                return (sec / 3600).toFixed(1) + "h"
            }

            perfTooltip.title = qsTr("性能预估 (1920×1080)")
            perfTooltip.contentModel = [
                {
                    label: qsTr("GPU 图片:"),
                    value: _fmtTime(gpuImageSec),
                    rawValue: String(gpuImageSec)
                },
                {
                    label: qsTr("GPU 视频 (10秒):"),
                    value: _fmtTime(gpuVideoSec),
                    rawValue: String(gpuVideoSec)
                },
                {
                    label: qsTr("CPU 图片:"),
                    value: _fmtTime(cpuImageSec),
                    rawValue: String(cpuImageSec)
                },
                {
                    label: qsTr("CPU 视频 (10秒):"),
                    value: _fmtTime(cpuVideoSec),
                    rawValue: String(cpuVideoSec)
                }
            ]
            perfTooltip.footerNote = qsTr("实际耗时与文件尺寸、分辨率、帧数等因素有关")
            perfTooltip.show(targetItem)
        }
    }

    // ========== 初始化和语言切换监听 ==========
    Connections {
        target: SettingsController
        function onLanguageChanged() {
            refreshData()
        }
    }

    function refreshData() {
        if (registry) {
            categoryRepeater.model = null
            categoryRepeater.model = registry.getCategories()
            if (root.selectedCategory !== "") {
                modelListView.model = null
                modelListView.model = registry.getModelsByCategoryStr(root.selectedCategory)
            }
        }
    }

    Component.onCompleted: {
        if (registry) {
            var categories = registry.getCategories()
            if (categories.length > 0) {
                selectedCategory = categories[0].id
                var models = registry.getModelsByCategoryStr(categories[0].id)
                modelListView.model = models
            }
        }
    }
}
