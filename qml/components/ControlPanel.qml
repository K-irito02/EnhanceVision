import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"
import "./" as Components
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

    // Shader 参数
    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real hue: 0.0
    property real sharpness: 0.0
    property real blur: 0.0
    property real denoise: 0.0
    property real exposure: 0.0
    property real gamma: 1.0
    property real temperature: 0.0
    property real tint: 0.0
    property real vignette: 0.0
    property real highlights: 0.0
    property real shadows: 0.0

    property int aiModelIndex: 0
    property bool collapsed: false
    
    readonly property bool hasShaderModifications: {
        return Math.abs(brightness) > 0.001 ||
               Math.abs(contrast - 1.0) > 0.001 ||
               Math.abs(saturation - 1.0) > 0.001 ||
               Math.abs(hue) > 0.001 ||
               Math.abs(sharpness) > 0.001 ||
               Math.abs(blur) > 0.001 ||
               Math.abs(denoise) > 0.001 ||
               Math.abs(exposure) > 0.001 ||
               Math.abs(gamma - 1.0) > 0.001 ||
               Math.abs(temperature) > 0.001 ||
               Math.abs(tint) > 0.001 ||
               Math.abs(vignette) > 0.001 ||
               Math.abs(highlights) > 0.001 ||
               Math.abs(shadows) > 0.001
    }

    // ========== 信号 ==========
    signal collapseToggleRequested()

    // ========== 属性监听 ==========
    onProcessingModeChanged: {
        displayMode = processingMode
    }

    Component.onCompleted: {
        displayMode = processingMode
    }

    // ========== 属性定义 ==========
    property bool isAnyEditing: false

    // ========== 视觉属性 ==========
    color: Theme.colors.card

    // 全局点击处理器 - 用于退出编辑模式
    MouseArea {
        id: globalClickHandler
        anchors.fill: parent
        enabled: root.isAnyEditing
        z: 999
        onClicked: {
            root.isAnyEditing = false
        }
    }

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
                id: paramsStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: displayMode

                // ===== Shader 模式参数 =====
                ScrollView {
                    id: shaderScrollView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    Components.ShaderParamsPanel {
                        id: shaderParams
                        width: shaderScrollView.availableWidth - 4

                        brightness: root.brightness
                        contrast: root.contrast
                        saturation: root.saturation
                        hue: root.hue
                        sharpness: root.sharpness
                        blur: root.blur
                        denoise: root.denoise
                        exposure: root.exposure
                        gamma: root.gamma
                        temperature: root.temperature
                        tint: root.tint
                        vignette: root.vignette
                        highlights: root.highlights
                        shadows: root.shadows

                        onParamChanged: function(name, value) {
                            root[name] = value
                        }
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
        }
    }

    // ========== 函数 ==========
    function sendToProcessing() {

        if (fileModel.count === 0) {
            return;
        }

        var params = {};
        if (displayMode === 0) {
            params.brightness = brightness;
            params.contrast = contrast;
            params.saturation = saturation;
            params.hue = hue;
            params.sharpness = sharpness;
            params.blur = blur;
            params.denoise = denoise;
            params.exposure = exposure;
            params.gamma = gamma;
            params.temperature = temperature;
            params.tint = tint;
            params.vignette = vignette;
            params.highlights = highlights;
            params.shadows = shadows;
        } else {
            params.modelIndex = aiModelIndex;
        }

        var messageId = processingController.sendToProcessing(displayMode, params);
    }

    function resetAllParams() {
        brightness = 0.0
        contrast = 1.0
        saturation = 1.0
        hue = 0.0
        sharpness = 0.0
        blur = 0.0
        denoise = 0.0
        exposure = 0.0
        gamma = 1.0
        temperature = 0.0
        tint = 0.0
        vignette = 0.0
        highlights = 0.0
        shadows = 0.0
    }

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
