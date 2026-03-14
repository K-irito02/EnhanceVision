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
 * 支持收缩/展开功能
 */
Rectangle {
    id: root

    // ========== 属性定义 ==========
    property int processingMode: 0
    property int displayMode: 0
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real sharpness: 0.0
    property real denoise: 0.0
    property int aiModelIndex: 0
    property bool collapsed: false

    // ========== 信号 ==========
    signal collapseToggleRequested()

    // ========== 属性监听 ==========
    onProcessingModeChanged: {
        displayMode = processingMode
    }

    Component.onCompleted: {
        displayMode = processingMode
    }

    // ========== 视觉属性 ==========
    color: Theme.colors.card

    // 左侧分隔线
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 1
        color: Theme.colors.cardBorder
    }

    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: collapsed ? 8 : 12
        anchors.topMargin: collapsed ? 8 : 12
        anchors.bottomMargin: collapsed ? 8 : 12
        spacing: collapsed ? 0 : 12

        // ========== 顶部：模式选择 + 收缩按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            // 收缩状态：只显示展开按钮
            IconButton {
                visible: root.collapsed
                iconName: "chevron-left"
                iconSize: 16
                btnSize: 28
                tooltip: qsTr("展开控制面板")
                onClicked: root.collapseToggleRequested()
            }

            // 展开状态：显示模式按钮
            RowLayout {
                visible: !root.collapsed
                spacing: 4
                Layout.fillWidth: true

                // Shader 模式按钮
                Rectangle {
                    Layout.preferredWidth: shaderRow.implicitWidth + 16
                    height: 28
                    radius: Theme.radius.md
                    color: displayMode === 0 ? Theme.colors.primary : "transparent"
                    border.width: displayMode === 0 ? 0 : 1
                    border.color: Theme.colors.border

                    Row {
                        id: shaderRow
                        anchors.centerIn: parent
                        spacing: 5

                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("sliders")
                            iconSize: 13
                            color: displayMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Shader")
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: displayMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: displayMode = 0
                    }

                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                }

                // AI 模式按钮
                Rectangle {
                    Layout.preferredWidth: aiRow.implicitWidth + 16
                    height: 28
                    radius: Theme.radius.md
                    color: displayMode === 1 ? Theme.colors.primary : "transparent"
                    border.width: displayMode === 1 ? 0 : 1
                    border.color: Theme.colors.border

                    Row {
                        id: aiRow
                        anchors.centerIn: parent
                        spacing: 5

                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("sparkles")
                            iconSize: 13
                            color: displayMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("AI")
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: displayMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: displayMode = 1
                    }

                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                }

                Item { Layout.fillWidth: true }

                // 收缩按钮
                IconButton {
                    iconName: "chevron-right"
                    iconSize: 14
                    btnSize: 28
                    tooltip: qsTr("收缩控制面板")
                    onClicked: root.collapseToggleRequested()
                }
            }
        }

        // ========== 收缩状态：垂直图标按钮 ==========
        ColumnLayout {
            visible: root.collapsed
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            Item { Layout.fillHeight: true }

            // 处理按钮（收缩状态）
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 36
                height: 36
                radius: Theme.radius.md
                color: processBtnMouse.containsMouse ? Theme.colors.primary : Theme.colors.primarySubtle
                border.width: 1
                border.color: Theme.colors.primary

                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon("send")
                    iconSize: 16
                    color: processBtnMouse.containsMouse ? "#FFFFFF" : Theme.colors.primary
                }

                MouseArea {
                    id: processBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: sendToProcessing()
                }

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
            }

            // 重置按钮（收缩状态）
            IconButton {
                Layout.alignment: Qt.AlignHCenter
                iconName: "rotate-ccw"
                iconSize: 16
                btnSize: 36
                tooltip: qsTr("重置参数")
                onClicked: {
                    root.brightness = 0.0;
                    root.contrast = 1.0;
                    root.saturation = 1.0;
                    root.sharpness = 0.0;
                    root.denoise = 0.0;
                }
            }

            Item { Layout.fillHeight: true }
        }

        // ========== 展开状态：完整内容 ==========
        ColumnLayout {
            visible: !root.collapsed
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // ========== 分隔线 ==========
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

            // ========== 参数区域 ==========
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: displayMode

                // ===== Shader 模式参数 =====
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        spacing: 8
                        width: parent.width

                        // 亮度
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                Text { text: qsTr("亮度"); color: Theme.colors.foreground; font.pixelSize: 11; font.weight: Font.Medium }
                                Item { Layout.fillWidth: true }
                                Text { text: brightnessSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 10; font.family: "Consolas" }
                            }
                            Slider { id: brightnessSlider; Layout.fillWidth: true; from: -1.0; to: 1.0; value: root.brightness; onValueChanged: root.brightness = value }
                        }

                        // 对比度
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                Text { text: qsTr("对比度"); color: Theme.colors.foreground; font.pixelSize: 11; font.weight: Font.Medium }
                                Item { Layout.fillWidth: true }
                                Text { text: contrastSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 10; font.family: "Consolas" }
                            }
                            Slider { id: contrastSlider; Layout.fillWidth: true; from: 0.0; to: 2.0; value: root.contrast; onValueChanged: root.contrast = value }
                        }

                        // 饱和度
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                Text { text: qsTr("饱和度"); color: Theme.colors.foreground; font.pixelSize: 11; font.weight: Font.Medium }
                                Item { Layout.fillWidth: true }
                                Text { text: saturationSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 10; font.family: "Consolas" }
                            }
                            Slider { id: saturationSlider; Layout.fillWidth: true; from: 0.0; to: 2.0; value: root.saturation; onValueChanged: root.saturation = value }
                        }

                        // 锐度
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                Text { text: qsTr("锐度"); color: Theme.colors.foreground; font.pixelSize: 11; font.weight: Font.Medium }
                                Item { Layout.fillWidth: true }
                                Text { text: sharpnessSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 10; font.family: "Consolas" }
                            }
                            Slider { id: sharpnessSlider; Layout.fillWidth: true; from: 0.0; to: 2.0; value: root.sharpness; onValueChanged: root.sharpness = value }
                        }

                        // 降噪
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            RowLayout {
                                Text { text: qsTr("降噪"); color: Theme.colors.foreground; font.pixelSize: 11; font.weight: Font.Medium }
                                Item { Layout.fillWidth: true }
                                Text { text: denoiseSlider.value.toFixed(2); color: Theme.colors.mutedForeground; font.pixelSize: 10; font.family: "Consolas" }
                            }
                            Slider { id: denoiseSlider; Layout.fillWidth: true; from: 0.0; to: 1.0; value: root.denoise; onValueChanged: root.denoise = value }
                        }

                        // 预设
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: qsTr("预设"); color: Theme.colors.foreground; font.pixelSize: 11; font.weight: Font.Medium }
                            ComboBox { Layout.fillWidth: true; model: [qsTr("自定义"), qsTr("鲜艳"), qsTr("柔和"), qsTr("黑白")] }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }

                // ===== AI 推理模式参数 =====
                ColumnLayout {
                    spacing: 10

                    // AI 模型选择
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ColoredIcon {
                            source: Theme.icon("cpu")
                            iconSize: 14
                            color: Theme.colors.foreground
                        }

                        Text {
                            text: qsTr("AI 模型")
                            color: Theme.colors.foreground
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }
                    }

                    ComboBox {
                        id: aiModelComboBox
                        Layout.fillWidth: true
                        model: [qsTr("Real-ESRGAN (4x)"), qsTr("Real-ESRGAN Anime"), qsTr("CBDNet (去噪)")]
                        currentIndex: root.aiModelIndex
                        onCurrentIndexChanged: root.aiModelIndex = currentIndex
                    }

                    // 模型说明卡片
                    Rectangle {
                        Layout.fillWidth: true
                        height: modelDescCol.implicitHeight + 16
                        radius: Theme.radius.md
                        color: Theme.colors.primarySubtle
                        border.width: 1
                        border.color: Theme.colors.border

                        ColumnLayout {
                            id: modelDescCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 3

                            Text {
                                text: qsTr("模型说明")
                                color: Theme.colors.foreground
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
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
                spacing: 6

                // 状态指示灯
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: processingController.queueStatus === ProcessingController.Running ? Theme.colors.success : Theme.colors.warning
                }

                Text {
                    text: processingController.queueStatus === ProcessingController.Running ? qsTr("运行中") : qsTr("已暂停")
                    color: Theme.colors.foreground
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }

                Text {
                    text: qsTr("排队: %1").arg(processingController.queueSize)
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                // 暂停/恢复按钮
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

                // 取消按钮
                Button {
                    text: qsTr("取消")
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
                spacing: 6

                // 重置按钮
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

                // 导出按钮
                Button {
                    text: qsTr("导出")
                    iconName: "save"
                    variant: "secondary"
                    size: "sm"
                }

                // 处理按钮
                Button {
                    id: sendButton
                    text: qsTr("处理")
                    iconName: "send"
                    variant: "primary"
                    size: "sm"
                    enabled: fileModel.count > 0
                    onClicked: sendToProcessing()
                }
            }
        }
    }

    // ========== 函数 ==========
    function sendToProcessing() {
        console.log("发送到处理队列");

        if (fileModel.count === 0) {
            console.warn("请先添加文件");
            return;
        }

        var params = {};
        if (displayMode === 0) {
            params.brightness = brightness;
            params.contrast = contrast;
            params.saturation = saturation;
            params.sharpness = sharpness;
            params.denoise = denoise;
        } else {
            params.modelIndex = aiModelIndex;
        }

        console.log("处理模式:", displayMode, "参数:", params);

        var messageId = processingController.sendToProcessing(displayMode, params);
        if (messageId) {
            console.log("任务已添加到队列，消息ID:", messageId);
        } else {
            console.warn("添加任务失败");
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
