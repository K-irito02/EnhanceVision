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
    /**
     * @brief 当前处理模式
     * 0: Shader, 1: AI Inference
     */
    property int processingMode: 0

    // Shader 参数
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real sharpness: 0.0
    property real denoise: 0.0

    // AI 模型选择
    property int aiModelIndex: 0

    // ========== 视觉属性 ==========
    color: Theme.colors.card
    border {
        width: 1
        color: Theme.colors.border
    }
    radius: Theme.radius.md

    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing._3
        spacing: Theme.spacing._3

        // ========== 模式选择 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2

            Text {
                text: qsTr("处理模式:")
                color: Theme.colors.foreground
                font.pixelSize: 13
            }

            // Shader 模式按钮
            Button {
                text: qsTr("Shader 滤镜")
                flat: processingMode !== 0
                checked: processingMode === 0
                onClicked: processingMode = 0
            }

            // AI 推理模式按钮
            Button {
                text: qsTr("AI 增强")
                flat: processingMode !== 1
                checked: processingMode === 1
                onClicked: processingMode = 1
            }

            Item {
                Layout.fillWidth: true
            }
        }

        // ========== 分隔线 ==========
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }

        // ========== 参数区域（根据模式显示） ==========
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: processingMode

            // Shader 模式参数
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    spacing: Theme.spacing._3
                    anchors.fill: parent
                    anchors.margins: Theme.spacing._1

                    Text {
                        text: qsTr("Shader 参数调节")
                        color: Theme.colors.foreground
                        font.pixelSize: 13
                        font.bold: true
                    }

                    // 亮度滑块
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing._2

                        Text {
                            text: qsTr("亮度")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }

                        Slider {
                            id: brightnessSlider
                            Layout.fillWidth: true
                            from: -1.0
                            to: 1.0
                            value: root.brightness
                            onValueChanged: root.brightness = value
                        }

                        Text {
                            text: brightnessSlider.value.toFixed(2)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // 对比度滑块
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing._2

                        Text {
                            text: qsTr("对比度")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }

                        Slider {
                            id: contrastSlider
                            Layout.fillWidth: true
                            from: 0.0
                            to: 2.0
                            value: root.contrast
                            onValueChanged: root.contrast = value
                        }

                        Text {
                            text: contrastSlider.value.toFixed(2)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // 饱和度滑块
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing._2

                        Text {
                            text: qsTr("饱和度")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }

                        Slider {
                            id: saturationSlider
                            Layout.fillWidth: true
                            from: 0.0
                            to: 2.0
                            value: root.saturation
                            onValueChanged: root.saturation = value
                        }

                        Text {
                            text: saturationSlider.value.toFixed(2)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // 锐度滑块
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing._2

                        Text {
                            text: qsTr("锐度")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }

                        Slider {
                            id: sharpnessSlider
                            Layout.fillWidth: true
                            from: 0.0
                            to: 2.0
                            value: root.sharpness
                            onValueChanged: root.sharpness = value
                        }

                        Text {
                            text: sharpnessSlider.value.toFixed(2)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // 降噪滑块
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing._2

                        Text {
                            text: qsTr("降噪")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                            Layout.preferredWidth: 60
                        }

                        Slider {
                            id: denoiseSlider
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: root.denoise
                            onValueChanged: root.denoise = value
                        }

                        Text {
                            text: denoiseSlider.value.toFixed(2)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            Layout.preferredWidth: 40
                        }
                    }

                    // 预设下拉框（可选）
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing._2

                        Text {
                            text: qsTr("预设:")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("自定义"), qsTr("鲜艳"), qsTr("柔和"), qsTr("黑白")]
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // AI 推理模式参数
            ColumnLayout {
                spacing: Theme.spacing._3

                Text {
                    text: qsTr("AI 模型选择")
                    color: Theme.colors.foreground
                    font.pixelSize: 13
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing._2

                    Text {
                        text: qsTr("模型:")
                        color: Theme.colors.foreground
                        font.pixelSize: 12
                        Layout.preferredWidth: 60
                    }

                    ComboBox {
                        id: aiModelComboBox
                        Layout.fillWidth: true
                        model: [qsTr("Real-ESRGAN (4x 超分)"), qsTr("Real-ESRGAN Anime"), qsTr("CBDNet (去噪)")]
                        currentIndex: root.aiModelIndex
                        onCurrentIndexChanged: root.aiModelIndex = currentIndex
                    }
                }

                Text {
                    text: qsTr("模型说明:")
                    color: Theme.colors.foreground
                    font.pixelSize: 12
                }

                Text {
                    text: {
                        switch (aiModelComboBox.currentIndex) {
                            case 0: return qsTr("通用场景超分辨率，4倍放大");
                            case 1: return qsTr("动漫插画专用优化");
                            case 2: return qsTr("通用图像降噪");
                            default: return "";
                        }
                    }
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // ========== 分隔线 ==========
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }

        // ========== 队列管理区域 ==========
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing._2

                Text {
                    text: qsTr("队列状态:")
                    color: Theme.colors.foreground
                    font.pixelSize: 13
                }

                Text {
                    text: {
                        if (processingController.queueStatus === ProcessingController.Running) {
                            return qsTr("运行中");
                        } else {
                            return qsTr("已暂停");
                        }
                    }
                    color: processingController.queueStatus === ProcessingController.Running ? Theme.colors.primary : Theme.colors.destructive
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    text: qsTr("排队: %1").arg(processingController.queueSize)
                    color: Theme.colors.foreground
                    font.pixelSize: 13
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing._2

                // 暂停/恢复按钮
                Button {
                    text: processingController.queueStatus === ProcessingController.Running ? qsTr("暂停") : qsTr("恢复")
                    Layout.preferredWidth: 80
                    onClicked: {
                        if (processingController.queueStatus === ProcessingController.Running) {
                            processingController.pauseQueue();
                        } else {
                            processingController.resumeQueue();
                        }
                    }
                }

                // 取消全部按钮
                Button {
                    text: qsTr("取消全部")
                    Layout.preferredWidth: 80
                    flat: true
                    onClicked: processingController.cancelAllTasks();
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        // ========== 分隔线 ==========
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }

        // ========== 操作按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2

            Button {
                text: qsTr("重置参数")
                Layout.preferredWidth: 100
                flat: true
                onClicked: {
                    root.brightness = 0.0;
                    root.contrast = 1.0;
                    root.saturation = 1.0;
                    root.sharpness = 0.0;
                    root.denoise = 0.0;
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("导出")
                Layout.preferredWidth: 100
                flat: true
            }

            Button {
                id: sendButton
                text: qsTr("发送")
                Layout.preferredWidth: 100
                highlighted: true
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
            // Shader 模式
            params.brightness = brightness;
            params.contrast = contrast;
            params.saturation = saturation;
            params.sharpness = sharpness;
            params.denoise = denoise;
        } else {
            // AI 模式
            params.modelIndex = aiModelIndex;
        }

        console.log("处理模式:", processingMode, "参数:", params);

        // 调用 ProcessingController 的发送方法
        var messageId = processingController.sendToProcessing(processingMode, params);
        if (messageId) {
            console.log("任务已添加到队列，消息ID:", messageId);
        } else {
            console.warn("添加任务失败");
        }
    }
}
