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

    // 内部：当前模型详情 - 使用Qt.binding确保响应式更新
    property var currentModelInfo: null
    
    // 监听registry和modelId变化，更新currentModelInfo
    onRegistryChanged: _updateModelInfo()
    onModelIdChanged: _updateModelInfo()
    
    function _updateModelInfo() {
        if (registry && modelId !== "") {
            _isModelSwitching = true
            currentModelInfo = registry.getModelInfoMap(modelId)
            // 重置为模型默认分块
            if (currentModelInfo && currentModelInfo.tileSize) {
                tileSize = currentModelInfo.tileSize
            }
            // 延迟重置参数，避免控件跳动
            _pendingParams = {}
            modelParams = {}
            _isModelSwitching = false
        } else {
            currentModelInfo = null
        }
    }

    // 内部：用于平滑过渡的参数缓存
    property var _pendingParams: ({})
    property bool _isModelSwitching: false

    // 本地化辅助函数：根据当前语言选择正确的文本
    function localizedText(param, zhField, enField) {
        if (!param) return ""
        var isEnglish = Theme.language === "en_US"
        if (isEnglish && param[enField] !== undefined && param[enField] !== "") {
            return param[enField]
        }
        return param[zhField] !== undefined ? param[zhField] : ""
    }

    // 获取参数标签（本地化）
    function getParamLabel(param) {
        return localizedText(param, "label", "label_en") || param.key || ""
    }

    // 获取参数描述（本地化）
    function getParamDescription(param) {
        return localizedText(param, "description", "description_en")
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
            color: root.useGpu && engine && engine.gpuAvailable ? Theme.colors.primary : Theme.colors.mutedForeground
        }

        Text {
            text: qsTr("GPU 加速")
            color: Theme.colors.foreground
            font.pixelSize: 12
            Layout.fillWidth: true
        }

        Controls.Switch {
            id: gpuSwitch
            checked: root.useGpu && engine && engine.gpuAvailable
            enabled: engine && engine.gpuAvailable
            opacity: enabled ? 1.0 : 0.5
            onToggled: {
                root.useGpu = checked
                root.paramsChanged()
            }
        }

        // 开关状态标签
        Rectangle {
            height: 18
            radius: Theme.radius.sm
            color: root.useGpu ? "#1a2563eb" : "#1a71717a"
            Layout.minimumWidth: switchStatusText.implicitWidth + 16
            Layout.preferredWidth: switchStatusText.implicitWidth + 16

            Text {
                id: switchStatusText
                anchors.centerIn: parent
                text: root.useGpu ? qsTr("已开启") : qsTr("已关闭")
                font.pixelSize: 9
                color: root.useGpu ? "#2563eb" : "#71717a"
            }
        }

        // GPU 可用性警告标签（仅当 GPU 不可用时显示）
        Rectangle {
            visible: !(engine && engine.gpuAvailable)
            height: 18
            radius: Theme.radius.sm
            color: "#1ae74c3c"
            Layout.minimumWidth: gpuWarningText.implicitWidth + 16
            Layout.preferredWidth: gpuWarningText.implicitWidth + 16

            Text {
                id: gpuWarningText
                anchors.centerIn: parent
                text: qsTr("GPU不可用")
                font.pixelSize: 9
                color: "#e74c3c"
            }
        }
    }

    // GPU 可用性变化时自动调整 useGpu
    Connections {
        target: engine
        function onGpuAvailableChanged(available) {
            if (!available) {
                root.useGpu = false
                root.paramsChanged()
            }
        }
    }

    // 初始化时检查 GPU 可用性
    Component.onCompleted: {
        if (engine && !engine.gpuAvailable) {
            root.useGpu = false
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

    // ========== 调试信息面板 ==========
    ColumnLayout {
        id: debugPanel
        Layout.fillWidth: true
        spacing: 6
        visible: currentModelInfo !== null

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }

        // 折叠标题
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("调试信息")
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }

            Item { Layout.fillWidth: true }

            Text {
                text: debugContent.visible ? qsTr("收起") : qsTr("展开")
                color: Theme.colors.primary
                font.pixelSize: 11
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: debugContent.visible = !debugContent.visible
                }
            }
        }

        // 调试内容
        ColumnLayout {
            id: debugContent
            Layout.fillWidth: true
            spacing: 4
            visible: false

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: debugInfoColumn.implicitHeight + 12
                radius: Theme.radius.sm
                color: Theme.colors.muted
                opacity: 0.3

                ColumnLayout {
                    id: debugInfoColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 2

                    // GPU 加速状态
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: qsTr("GPU 加速:")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                        Text {
                            text: {
                                if (!engine) return qsTr("未知")
                                if (!engine.gpuAvailable) return qsTr("不可用 (CPU模式)")
                                return root.useGpu ? qsTr("已开启 (Vulkan)") : qsTr("已关闭 (CPU模式)")
                            }
                            color: {
                                if (!engine || !engine.gpuAvailable) return "#e74c3c"
                                return root.useGpu ? "#22c55e" : Theme.colors.mutedForeground
                            }
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                    }

                    // 分块大小
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: qsTr("分块大小:")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                        Text {
                            text: root.tileSize > 0 ? (root.tileSize + " px") : qsTr("自动 (不分块)")
                            color: Theme.colors.foreground
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                    }

                    // 输出放大倍数
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: currentModelInfo !== null && currentModelInfo.scaleFactor > 1
                        Text {
                            text: qsTr("放大倍数:")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                        Text {
                            text: currentModelInfo ? (currentModelInfo.scaleFactor + "x") : "1x"
                            color: Theme.colors.foreground
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                    }

                    // 当前模型
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: currentModelInfo !== null
                        Text {
                            text: qsTr("当前模型:")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                        Text {
                            text: currentModelInfo ? currentModelInfo.name : ""
                            color: Theme.colors.foreground
                            font.pixelSize: 10
                            font.family: "monospace"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }

    // ========== 模型特定参数 ==========
    ColumnLayout {
        id: modelParamsSection
        Layout.fillWidth: true
        spacing: 6
        visible: {
            if (!currentModelInfo) return false
            var params = currentModelInfo.supportedParams
            if (!params) return false
            // 确保params是对象类型
            if (typeof params !== 'object') return false
            try {
                var keys = Object.keys(params)
                return keys.length > 0
            } catch (e) {
                return false
            }
        }

        Behavior on opacity {
            NumberAnimation { duration: Theme.animation.fast }
        }

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
            id: paramsRepeater
            model: {
                if (!currentModelInfo) return []
                var params = currentModelInfo.supportedParams
                if (!params || typeof params !== 'object') return []
                try {
                    var keys = Object.keys(params)
                    var items = []
                    for (var i = 0; i < keys.length; i++) {
                        var key = keys[i]
                        var param = params[key]
                        if (!param || typeof param !== 'object') continue
                        // 深拷贝参数对象，确保 key 被正确设置
                        var paramCopy = JSON.parse(JSON.stringify(param))
                        paramCopy.key = key
                        items.push(paramCopy)
                    }
                    return items
                } catch (e) {
                    return []
                }
            }

            delegate: ColumnLayout {
                id: paramDelegate
                Layout.fillWidth: true
                spacing: 4

                // 平滑过渡动画
                opacity: root._isModelSwitching ? 0.5 : 1.0
                Behavior on opacity {
                    NumberAnimation { duration: Theme.animation.fast }
                }

                property string paramKey: modelData.key
                property string paramType: modelData.type || "float"
                property var paramDefault: modelData["default"]

                // 参数标签行
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: root.getParamLabel(modelData)
                        color: Theme.colors.foreground
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: {
                            var val = root.modelParams[paramKey]
                            var displayVal = val !== undefined ? val : paramDefault
                            if (paramType === "bool") {
                                return displayVal ? qsTr("开启") : qsTr("关闭")
                            }
                            return displayVal !== undefined ? displayVal.toString() : ""
                        }
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 11
                    }
                }

                // 滑块参数
                Controls.Slider {
                    id: sliderControl
                    Layout.fillWidth: true
                    visible: paramType === "int" || paramType === "float"
                    from: modelData.min !== undefined ? modelData.min : 0
                    to: modelData.max !== undefined ? modelData.max : 100
                    stepSize: paramType === "int" ? 1 : (modelData.step || 0.1)

                    // 使用 Behavior 实现平滑过渡
                    value: {
                        var val = root.modelParams[paramKey]
                        return val !== undefined ? val : (paramDefault !== undefined ? paramDefault : from)
                    }
                    Behavior on value {
                        enabled: !sliderControl.pressed
                        NumberAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
                    }

                    onMoved: {
                        var newParams = JSON.parse(JSON.stringify(root.modelParams))
                        newParams[paramKey] = value
                        root.modelParams = newParams
                        root.paramsChanged()
                    }
                }

                // 下拉选择参数
                ComboBox {
                    id: comboBoxControl
                    Layout.fillWidth: true
                    visible: paramType === "enum"
                    model: modelData.values || []
                    textRole: ""

                    currentIndex: {
                        var val = root.modelParams[paramKey]
                        if (val === undefined) val = paramDefault
                        var values = modelData.values || []
                        return Math.max(0, values.indexOf(val))
                    }

                    onCurrentIndexChanged: {
                        if (visible && !root._isModelSwitching) {
                            var values = modelData.values || []
                            if (currentIndex >= 0 && currentIndex < values.length) {
                                var newParams = JSON.parse(JSON.stringify(root.modelParams))
                                newParams[paramKey] = values[currentIndex]
                                root.modelParams = newParams
                                root.paramsChanged()
                            }
                        }
                    }
                }

                // 布尔开关参数
                RowLayout {
                    Layout.fillWidth: true
                    visible: paramType === "bool"
                    spacing: 8

                    Controls.Switch {
                        id: boolSwitch
                        checked: {
                            var val = root.modelParams[paramKey]
                            return val !== undefined ? val : (paramDefault !== undefined ? paramDefault : false)
                        }

                        onToggled: {
                            var newParams = JSON.parse(JSON.stringify(root.modelParams))
                            newParams[paramKey] = checked
                            root.modelParams = newParams
                            root.paramsChanged()
                        }
                    }

                    Text {
                        text: boolSwitch.checked ? qsTr("开启") : qsTr("关闭")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }
                }

                // 文本输入参数
                TextField {
                    id: textFieldControl
                    Layout.fillWidth: true
                    visible: paramType === "string"
                    placeholderText: modelData.placeholder || ""

                    text: {
                        var val = root.modelParams[paramKey]
                        return val !== undefined ? val : (paramDefault !== undefined ? paramDefault : "")
                    }

                    onEditingFinished: {
                        var newParams = JSON.parse(JSON.stringify(root.modelParams))
                        newParams[paramKey] = text
                        root.modelParams = newParams
                        root.paramsChanged()
                    }
                }

                // 参数描述提示（本地化）
                Text {
                    Layout.fillWidth: true
                    text: root.getParamDescription(modelData)
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    visible: text !== ""
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
            // 强制刷新 Repeater 以更新本地化文本
            paramsRepeater.model = paramsRepeater.model
        }
    }

    // 监听 Theme.language 变化
    Connections {
        target: Theme
        function onLanguageChanged() {
            // 刷新 Repeater 以更新本地化文本
            paramsRepeater.model = paramsRepeater.model
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
