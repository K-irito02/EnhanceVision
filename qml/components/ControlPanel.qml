import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"
import EnhanceVision.Controllers
import EnhanceVision.Models

/**
 * @brief 控制面板组件
 *
 * 提供处理模式选择、参数调节、队列管理和操作按钮
 * 参考功能设计文档 0.2 节中的 3.4 和 3.11 节
 */
Rectangle {
    id: root

    // ========== 属性定义 ==========
    property int processingMode: 0
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real sharpness: 0.0
    property real denoise: 0.0
    property int aiModelIndex: 0

    // ========== 视觉属性 ==========
    color: Theme.colors.card
    border.width: 1
    border.color: Theme.colors.cardBorder
    radius: Theme.radius.lg

    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // ========== 模式选择标签页 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            // Shader 模式按钮
            Rectangle {
                Layout.preferredWidth: shaderRow.implicitWidth + 20
                height: 32
                radius: Theme.radius.md
                color: processingMode === 0 ? Theme.colors.primary : "transparent"
                border.width: processingMode === 0 ? 0 : 1
                border.color: Theme.colors.border

                Row {
                    id: shaderRow
                    anchors.centerIn: parent
                    spacing: 6

                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14; height: 14
                        source: Theme.icon("sliders")
                        sourceSize: Qt.size(14, 14)
                        smooth: true
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Shader 滤镜")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: processingMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: processingMode = 0
                }

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
            }

            // AI 模式按钮
            Rectangle {
                Layout.preferredWidth: aiRow.implicitWidth + 20
                height: 32
                radius: Theme.radius.md
                color: processingMode === 1 ? Theme.colors.primary : "transparent"
                border.width: processingMode === 1 ? 0 : 1
                border.color: Theme.colors.border

                Row {
                    id: aiRow
                    anchors.centerIn: parent
                    spacing: 6

                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14; height: 14
                        source: Theme.icon("sparkles")
                        sourceSize: Qt.size(14, 14)
                        smooth: true
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("AI 增强")
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: processingMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: processingMode = 1
                }

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
            }

            Item { Layout.fillWidth: true }
        }

        // ========== 分隔线 ==========
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

        // ========== 参数区域 ==========
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: processingMode

            // ===== Shader 模式参数 =====
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    spacing: 10
                    width: parent.width

                    // 亮度
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Text { text: qsTr("亮度"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
                            Item { Layout.fillWidth: true }
                            Text { text: brightnessSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 11; font.family: "Consolas" }
                        }
                        Slider { id: brightnessSlider; Layout.fillWidth: true; from: -1.0; to: 1.0; value: root.brightness; onValueChanged: root.brightness = value }
                    }

                    // 对比度
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Text { text: qsTr("对比度"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
                            Item { Layout.fillWidth: true }
                            Text { text: contrastSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 11; font.family: "Consolas" }
                        }
                        Slider { id: contrastSlider; Layout.fillWidth: true; from: 0.0; to: 2.0; value: root.contrast; onValueChanged: root.contrast = value }
                    }

                    // 饱和度
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Text { text: qsTr("饱和度"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
                            Item { Layout.fillWidth: true }
                            Text { text: saturationSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 11; font.family: "Consolas" }
                        }
                        Slider { id: saturationSlider; Layout.fillWidth: true; from: 0.0; to: 2.0; value: root.saturation; onValueChanged: root.saturation = value }
                    }

                    // 锐度
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Text { text: qsTr("锐度"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
                            Item { Layout.fillWidth: true }
                            Text { text: sharpnessSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 11; font.family: "Consolas" }
                        }
                        Slider { id: sharpnessSlider; Layout.fillWidth: true; from: 0.0; to: 2.0; value: root.sharpness; onValueChanged: root.sharpness = value }
                    }

                    // 降噪
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        RowLayout {
                            Text { text: qsTr("降噪"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
                            Item { Layout.fillWidth: true }
                            Text { text: denoiseSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 11; font.family: "Consolas" }
                        }
                        Slider { id: denoiseSlider; Layout.fillWidth: true; from: 0.0; to: 1.0; value: root.denoise; onValueChanged: root.denoise = value }
                    }

                    // 预设
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text { text: qsTr("预设"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
                        ComboBox { Layout.fillWidth: true; model: [qsTr("自定义"), qsTr("鲜艳"), qsTr("柔和"), qsTr("黑白")] }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ===== AI 推理模式参数 =====
            ColumnLayout {
                spacing: 12

                // AI 模型选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Image {
                        width: 16; height: 16
                        source: Theme.icon("cpu")
                        sourceSize: Qt.size(16, 16)
                        smooth: true
                    }

                    Text {
                        text: qsTr("AI 模型选择")
                        color: Theme.colors.foreground
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                ComboBox {
                    id: aiModelComboBox
                    Layout.fillWidth: true
                    model: [qsTr("Real-ESRGAN (4x 超分)"), qsTr("Real-ESRGAN Anime"), qsTr("CBDNet (去噪)")]
                    currentIndex: root.aiModelIndex
                    onCurrentIndexChanged: root.aiModelIndex = currentIndex
                }

                // 模型说明卡片
                Rectangle {
                    Layout.fillWidth: true
                    height: modelDescCol.implicitHeight + 20
                    radius: Theme.radius.md
                    color: Theme.colors.primarySubtle
                    border.width: 1
                    border.color: Theme.colors.border

                    ColumnLayout {
                        id: modelDescCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            text: qsTr("模型说明")
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: {
                                switch (aiModelComboBox.currentIndex) {
                                    case 0: return qsTr("通用场景超分辨率，4倍放大，适合照片增强");
                                    case 1: return qsTr("动漫插画专用优化，线条清晰，色彩还原");
                                    case 2: return qsTr("通用图像降噪，去除高ISO噪点");
                                    default: return "";
                                }
                            }
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ========== 分隔线 ==========
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

        // ========== 队列管理区域 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // 状态指示灯
            Rectangle {
                width: 8; height: 8; radius: 4
                color: processingController.queueStatus === ProcessingController.Running ? Theme.colors.success : Theme.colors.warning
            }

            Text {
                text: processingController.queueStatus === ProcessingController.Running ? qsTr("运行中") : qsTr("已暂停")
                color: Theme.colors.foreground
                font.pixelSize: 12
                font.weight: Font.Medium
            }

            Rectangle { width: 1; height: 14; color: Theme.colors.border }

            Text {
                text: qsTr("排队: %1").arg(processingController.queueSize)
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }

            Button {
                text: processingController.queueStatus === ProcessingController.Running ? qsTr("暂停") : qsTr("恢复")
                variant: "secondary"
                size: "sm"
                onClicked: {
                    if (processingController.queueStatus === ProcessingController.Running) {
                        processingController.pauseQueue();
                    } else {
                        processingController.resumeQueue();
                    }
                }
            }

            Button {
                text: qsTr("取消全部")
                variant: "ghost"
                size: "sm"
                onClicked: processingController.cancelAllTasks()
            }
        }

        // ========== 分隔线 ==========
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

        // ========== 操作按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("重置")
                iconName: "rotate-ccw"
                variant: "ghost"
                size: "sm"
                onClicked: {
                    root.brightness = 0.0;
                    root.contrast = 1.0;
                    root.saturation = 1.0;
                    root.sharpness = 0.0;
                    root.denoise = 0.0;
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("导出")
                iconName: "save"
                variant: "secondary"
                size: "md"
            }

            Button {
                id: sendButton
                text: qsTr("处理")
                iconName: "send"
                variant: "primary"
                size: "md"
                enabled: fileModel.count > 0
                onClicked: sendToProcessing()
            }
        }
    }

    // ========== 函数 ==========
    /**
     * @brief 发送到处理队列
     */
    function sendToProcessing() {
        console.log("发送到处理队列");

        // 验证文件列表
        if (fileModel.count === 0) {
            console.warn("请先添加文件");
            return;
        }

        // 构建参数
        var params = {};
        if (processingMode === 0) {
            params.brightness = brightness;
            params.contrast = contrast;
            params.saturation = saturation;
            params.sharpness = sharpness;
            params.denoise = denoise;
        } else {
            params.modelIndex = aiModelIndex;
        }

        console.log("处理模式:", processingMode, "参数:", params);

        var messageId = processingController.sendToProcessing(processingMode, params);
        if (messageId) {
            console.log("任务已添加到队列，消息ID:", messageId);
        } else {
            console.warn("添加任务失败");
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
