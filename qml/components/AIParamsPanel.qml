import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision.Controllers
import "../styles"
import "../controls" as Controls

/**
 * @brief AI 参数配置面板
 *
 * 根据所选模型动态显示可调参数，如 GPU 加速、分块大小等。
 */
ColumnLayout {
    id: root

    property string modelId: ""
    property bool useGpu: true
    property int tileSize: 0
    property var modelParams: ({})
    property var registry: processingController ? processingController.modelRegistry : null
    property var engine: processingController ? processingController.aiEngine : null

    signal paramsChanged()

    spacing: 8

    // 内部：当前模型详情
    property var currentModelInfo: registry && modelId !== "" ? registry.getModelInfoMap(modelId) : null

    onModelIdChanged: {
        if (registry && modelId !== "") {
            currentModelInfo = registry.getModelInfoMap(modelId)
            // 重置为模型默认分块
            if (currentModelInfo && currentModelInfo.tileSize) {
                tileSize = currentModelInfo.tileSize
            }
            modelParams = {}
        }
    }

    // ========== 模型信息卡片 ==========
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: infoCol.implicitHeight + 16
        radius: Theme.radius.md
        color: Theme.colors.primarySubtle
        border.width: 1
        border.color: Theme.colors.primary
        visible: currentModelInfo !== null

        ColumnLayout {
            id: infoCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                ColoredIcon {
                    source: Theme.icon("cpu")
                    iconSize: 14
                    color: Theme.colors.primary
                }

                Text {
                    text: qsTr("选中模型")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                }

                Rectangle {
                    width: 1
                    height: 14
                    color: Theme.colors.border
                }

                Text {
                    id: modelNameText
                    text: currentModelInfo ? currentModelInfo.name : ""
                    color: Theme.colors.foreground
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }
            }

            Text {
                text: currentModelInfo ? currentModelInfo.description : ""
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: text !== ""
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: currentModelInfo !== null

                Rectangle {
                    height: 18
                    radius: Theme.radius.sm
                    color: Theme.colors.muted
                    Layout.minimumWidth: catNameLabel.implicitWidth + 12
                    Layout.preferredWidth: catNameLabel.implicitWidth + 12

                    Text {
                        id: catNameLabel
                        anchors.centerIn: parent
                        text: currentModelInfo ? (currentModelInfo.categoryName || "") : ""
                        font.pixelSize: 9
                        color: Theme.colors.mutedForeground
                    }
                }

                Rectangle {
                    height: 18
                    radius: Theme.radius.sm
                    color: Theme.colors.muted
                    Layout.minimumWidth: sizeInfoLabel.implicitWidth + 12
                    Layout.preferredWidth: sizeInfoLabel.implicitWidth + 12

                    Text {
                        id: sizeInfoLabel
                        anchors.centerIn: parent
                        text: currentModelInfo ? (currentModelInfo.sizeStr || "") : ""
                        font.pixelSize: 9
                        color: Theme.colors.mutedForeground
                    }
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    // ========== 分隔线 ==========
    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Theme.colors.border
        visible: currentModelInfo !== null
    }

    // ========== GPU 加速开关 ==========
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: engine !== null

        ColoredIcon {
            source: Theme.icon("zap")
            iconSize: 14
            color: root.useGpu ? Theme.colors.primary : Theme.colors.mutedForeground
        }

        Text {
            text: qsTr("GPU 加速")
            color: Theme.colors.foreground
            font.pixelSize: 12
            Layout.fillWidth: true
        }

        Controls.Switch {
            checked: root.useGpu
            onToggled: {
                root.useGpu = checked
                root.paramsChanged()
            }
        }

        // GPU 状态指示
        Rectangle {
            height: 18
            radius: Theme.radius.sm
            color: engine && engine.gpuAvailable ? "#1a7c3d1e" : "#1ae74c3c"
            Layout.minimumWidth: gpuStatusText.implicitWidth + 16
            Layout.preferredWidth: gpuStatusText.implicitWidth + 16

            Text {
                id: gpuStatusText
                anchors.centerIn: parent
                text: engine && engine.gpuAvailable ? qsTr("可用") : qsTr("不可用")
                font.pixelSize: 9
                color: engine && engine.gpuAvailable ? "#7c3d1e" : "#e74c3c"
            }
        }
    }

    // ========== 分块大小 ==========
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4
        visible: currentModelInfo !== null && currentModelInfo.tileSize > 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("分块大小")
                color: Theme.colors.foreground
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.tileSize > 0 ? (root.tileSize + " px") : qsTr("自动")
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }
        }

        Controls.Slider {
            Layout.fillWidth: true
            from: 0
            to: 1024
            stepSize: 64
            value: root.tileSize
            onMoved: {
                root.tileSize = value
                root.paramsChanged()
            }
        }

        Text {
            text: qsTr("较小的分块占用更少显存，但处理更慢。0 = 自动。")
            color: Theme.colors.mutedForeground
            font.pixelSize: 10
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    // ========== 模型特定参数 ==========
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        visible: currentModelInfo !== null && Object.keys(currentModelInfo.supportedParams || {}).length > 0

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }

        Text {
            text: qsTr("模型参数")
            color: Theme.colors.foreground
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        Repeater {
            model: {
                if (!currentModelInfo || !currentModelInfo.supportedParams) return []
                var params = currentModelInfo.supportedParams
                var keys = Object.keys(params)
                var items = []
                for (var i = 0; i < keys.length; i++) {
                    var key = keys[i]
                    var param = params[key]
                    param.key = key
                    items.push(param)
                }
                return items
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: modelData.name || modelData.key
                        color: Theme.colors.foreground
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: {
                            var val = root.modelParams[modelData.key]
                            return val !== undefined ? val.toString() : (modelData["default"] !== undefined ? modelData["default"].toString() : "")
                        }
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 11
                    }
                }

                // 滑块参数
                Controls.Slider {
                    Layout.fillWidth: true
                    visible: modelData.type === "int" || modelData.type === "float"
                    from: modelData.min !== undefined ? modelData.min : 0
                    to: modelData.max !== undefined ? modelData.max : 100
                    stepSize: modelData.type === "int" ? 1 : 0.1
                    value: {
                        var val = root.modelParams[modelData.key]
                        return val !== undefined ? val : (modelData["default"] !== undefined ? modelData["default"] : from)
                    }
                    onMoved: {
                        var newParams = root.modelParams
                        newParams[modelData.key] = value
                        root.modelParams = newParams
                        root.paramsChanged()
                    }
                }

                // 下拉选择参数
                ComboBox {
                    Layout.fillWidth: true
                    visible: modelData.type === "enum"
                    model: modelData.values || []
                    currentIndex: {
                        var val = root.modelParams[modelData.key]
                        if (val === undefined) val = modelData["default"]
                        var values = modelData.values || []
                        return Math.max(0, values.indexOf(val))
                    }
                    onCurrentIndexChanged: {
                        var values = modelData.values || []
                        if (currentIndex >= 0 && currentIndex < values.length) {
                            var newParams = root.modelParams
                            newParams[modelData.key] = values[currentIndex]
                            root.modelParams = newParams
                            root.paramsChanged()
                        }
                    }
                }
            }
        }
    }

    // ========== 语言切换监听 ==========
    Connections {
        target: SettingsController
        function onLanguageChanged() {
            if (registry && modelId !== "") {
                currentModelInfo = registry.getModelInfoMap(modelId)
            }
        }
    }

    // ========== 汇总当前参数方法 ==========
    function getParams() {
        var params = {}
        params.modelId = root.modelId
        params.useGpu = root.useGpu
        params.tileSize = root.tileSize
        if (currentModelInfo) {
            params.category = currentModelInfo.category || ""
        }
        // 合并模型特定参数
        var keys = Object.keys(root.modelParams)
        for (var i = 0; i < keys.length; i++) {
            params[keys[i]] = root.modelParams[keys[i]]
        }
        return params
    }
}
