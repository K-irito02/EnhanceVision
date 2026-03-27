import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision.Controllers
import "../styles"
import "../controls" as Controls

/**
 * @brief AI 参数配置面板
 *
 * 设计原则：
 * 1. UI 始终展示模型的「默认值」，不因后台自动计算而跳动
 * 2. 后台静默计算最优参数（_autoParams），推理时透明注入
 * 3. 用户手动调节的值具有最高优先级，会覆盖自动推荐值
 * 4. 分块大小提供自动/手动双模式切换
 */
ColumnLayout {
    id: root

    // ── 外部绑定属性
    property string modelId: ""
    property bool useGpu: true
    property int tileSize: 0
    property var modelParams: ({})
    property var registry: processingController ? processingController.modelRegistry : null
    property var engine:   processingController ? processingController.aiEngine   : null

    // ── 媒体信息（由外部绑定，驱动后台自动参数计算）
    property size currentMediaSize: Qt.size(0, 0)
    property bool currentMediaIsVideo: false

    signal paramsChanged()

    spacing: 8

    // ── 内部状态
    property var  currentModelInfo:    null
    property bool _isModelSwitching:   false
    property var  _pendingParams:      ({})
    property bool autoTileMode: true
    property int  computedAutoTileSize: 0
    property var  _autoParams: ({})

    // ── 自动优化就绪状态
    readonly property bool _autoReady: {
        return currentMediaSize.width > 0
            && currentModelInfo !== null
            && Object.keys(_autoParams).length > 0
    }

    onRegistryChanged: _updateModelInfo()
    onModelIdChanged:  _updateModelInfo()

    function _updateModelInfo() {
        if (registry && modelId !== "") {
            _isModelSwitching = true
            currentModelInfo  = registry.getModelInfoMap(modelId)
            autoTileMode      = true
            tileSize          = 0
            computedAutoTileSize = 0
            _autoParams       = {}
            _pendingParams    = {}
            modelParams       = {}
            _recomputeAutoParams()
            _isModelSwitching = false
        } else {
            currentModelInfo = null
        }
    }

    function _recomputeAutoParams() {
        if (engine && currentModelInfo && currentMediaSize.width > 0) {
            _autoParams = engine.computeAutoParams(currentMediaSize, currentMediaIsVideo)
            computedAutoTileSize = (_autoParams["tileSize"] !== undefined)
                                   ? _autoParams["tileSize"] : 0
        } else {
            _autoParams          = {}
            computedAutoTileSize = 0
        }
    }

    onCurrentMediaSizeChanged:    _recomputeAutoParams()
    onCurrentMediaIsVideoChanged: _recomputeAutoParams()

    function _localText(param, zhKey, enKey) {
        if (!param) return ""
        var useEn = (Theme.language === "en_US")
        if (useEn && param[enKey]) return param[enKey]
        return param[zhKey] || ""
    }
    function getParamLabel(param)       { return _localText(param, "label", "label_en") || param.key || "" }
    function getParamDescription(param) { return _localText(param, "description", "description_en") }

    function getParams() {
        var params = {}
        params.modelId      = root.modelId
        params.useGpu       = root.useGpu
        params.tileSize     = root.autoTileMode ? 0 : root.tileSize
        params.autoTileSize = root.autoTileMode
        params.mediaIsVideo = root.currentMediaIsVideo
        if (currentModelInfo) {
            params.category    = currentModelInfo.category    || ""
            params.scaleFactor = currentModelInfo.scaleFactor || 1
        }
        var autoKeys = Object.keys(root._autoParams)
        for (var a = 0; a < autoKeys.length; a++) {
            var ak = autoKeys[a]
            if (ak !== "tileSize") params[ak] = root._autoParams[ak]
        }
        var keys = Object.keys(root.modelParams)
        for (var i = 0; i < keys.length; i++) {
            params[keys[i]] = root.modelParams[keys[i]]
        }
        return params
    }

    // ===== 模型信息卡片 =====
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: _infoCol.implicitHeight + 16
        radius: Theme.radius.md
        color: Theme.colors.primarySubtle
        border.width: 1
        border.color: Theme.colors.primary
        visible: currentModelInfo !== null

        ColumnLayout {
            id: _infoCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                ColoredIcon { source: Theme.icon("cpu"); iconSize: 14; color: Theme.colors.primary }
                Text { text: qsTr("选中模型"); color: Theme.colors.mutedForeground; font.pixelSize: 11 }
                Rectangle { width: 1; height: 14; color: Theme.colors.border }
                Text {
                    text: currentModelInfo ? currentModelInfo.name : ""
                    color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.DemiBold
                    Layout.fillWidth: true; elide: Text.ElideRight
                }
            }

            Text {
                text: {
                    if (!currentModelInfo) return ""
                    var isEn = (Theme.language === "en_US")
                    return (isEn && currentModelInfo.descriptionEn)
                           ? currentModelInfo.descriptionEn : (currentModelInfo.description || "")
                }
                color: Theme.colors.mutedForeground; font.pixelSize: 11
                wrapMode: Text.WordWrap; Layout.fillWidth: true; visible: text !== ""
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 6; visible: currentModelInfo !== null
                Tag { text: currentModelInfo ? (currentModelInfo.categoryName || "") : ""; visible: text !== "" }
                Tag { text: currentModelInfo ? (currentModelInfo.sizeStr || "") : ""; visible: text !== "" }
                Tag {
                    text: (currentModelInfo && currentModelInfo.scaleFactor > 1) ? currentModelInfo.scaleFactor + "x" : ""
                    visible: text !== ""; accent: true
                }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ===== 自动优化状态栏 =====
    Rectangle {
        Layout.fillWidth: true
        height: _autoStatusRow.implicitHeight + 10
        radius: Theme.radius.sm
        color: root._autoReady ? Theme.colors.successSubtle : Theme.colors.muted
        border.width: 1
        border.color: root._autoReady ? Theme.colors.success : Theme.colors.border
        visible: currentModelInfo !== null
        Behavior on color        { ColorAnimation { duration: 200 } }
        Behavior on border.color { ColorAnimation { duration: 200 } }

        RowLayout {
            id: _autoStatusRow
            anchors {
                left: parent.left; right: parent.right
                verticalCenter: parent.verticalCenter
                leftMargin: 8; rightMargin: 8
            }
            spacing: 6

            ColoredIcon {
                source: Theme.icon(root._autoReady ? "check-circle" : "loader")
                iconSize: 12
                color: root._autoReady ? Theme.colors.success : Theme.colors.mutedForeground
            }

            Text {
                text: {
                    if (!root._autoReady)
                        return currentMediaSize.width > 0 ? qsTr("正在计算最优参数…") : qsTr("上传文件后将自动优化参数")
                    var wh   = currentMediaSize.width + " × " + currentMediaSize.height
                    var kind = root.currentMediaIsVideo ? qsTr("视频") : qsTr("图像")
                    return kind + " · " + wh + " · " + qsTr("自动优化已就绪")
                }
                color: root._autoReady ? Theme.colors.success : Theme.colors.mutedForeground
                font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight
                Behavior on color { ColorAnimation { duration: 200 } }
            }

            Item {
                width: 16; height: 16; visible: root._autoReady
                ColoredIcon {
                    anchors.centerIn: parent
                    source: root.currentMediaIsVideo ? Theme.icon("video") : Theme.icon("image")
                    iconSize: 12; color: Theme.colors.mutedForeground
                }
                ToolTip.visible: _strategyHover.containsMouse
                ToolTip.delay: 400
                ToolTip.text: root.currentMediaIsVideo
                    ? qsTr("视频策略：优先稳定性（抑制帧间闪烁/拖影），适度降低质量参数")
                    : qsTr("图像策略：优先质量（TTA、锐度、人像增强等均按最优值配置）")
                MouseArea { id: _strategyHover; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.WhatsThisCursor }
            }
        }
    }

    Divider { visible: currentModelInfo !== null }

    // ===== GPU 加速开关 =====
    RowLayout {
        Layout.fillWidth: true; spacing: 8; visible: engine !== null

        ColoredIcon {
            source: Theme.icon("zap"); iconSize: 14
            color: (root.useGpu && engine && engine.gpuAvailable) ? Theme.colors.primary : Theme.colors.mutedForeground
        }

        Text { text: qsTr("GPU 加速"); color: Theme.colors.foreground; font.pixelSize: 12; Layout.fillWidth: true }

        Badge {
            text: {
                if (!engine) return ""
                if (!engine.gpuAvailable) return qsTr("不可用")
                return root.useGpu ? qsTr("Vulkan") : qsTr("CPU 模式")
            }
            color: (!engine || !engine.gpuAvailable) ? Theme.colors.destructive
                   : root.useGpu ? Theme.colors.primary : Theme.colors.mutedForeground
        }

        Controls.Switch {
            id: _gpuSwitch
            checked: root.useGpu && engine && engine.gpuAvailable
            enabled: engine && engine.gpuAvailable
            opacity: enabled ? 1.0 : 0.5
            onToggled: { root.useGpu = checked; root.paramsChanged() }
        }
    }

    InfoBanner {
        Layout.fillWidth: true
        visible: engine !== null && !(engine && engine.gpuAvailable)
        text: qsTr("当前设备不支持 Vulkan GPU 加速，将使用 CPU 模式运行（速度较慢）。")
        isWarning: true
    }

    Connections {
        target: engine
        function onGpuAvailableChanged(available) {
            if (!available) { root.useGpu = false; root.paramsChanged() }
        }
        function onAutoParamsComputed(autoTileSize) { root.computedAutoTileSize = autoTileSize }
        function onAllAutoParamsComputed(autoParams) {
            root._autoParams = autoParams
            if (autoParams["tileSize"] !== undefined) root.computedAutoTileSize = autoParams["tileSize"]
        }
    }

    Component.onCompleted: { if (engine && !engine.gpuAvailable) root.useGpu = false }

    // ===== 分块大小（自动/手动双模式）=====
    ColumnLayout {
        Layout.fillWidth: true; spacing: 6
        visible: currentModelInfo !== null && currentModelInfo.tileSize > 0

        Divider {}

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            ColoredIcon {
                source: Theme.icon("layout-grid"); iconSize: 13
                color: root.autoTileMode ? Theme.colors.primary : Theme.colors.mutedForeground
            }
            Text { text: qsTr("分块大小"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
            Item { Layout.fillWidth: true }
            Text {
                visible: root.autoTileMode
                text: root.computedAutoTileSize > 0
                      ? (qsTr("自动推荐") + " · " + root.computedAutoTileSize + " px")
                      : qsTr("自动（整图推理）")
                color: Theme.colors.primary; font.pixelSize: 10; font.weight: Font.Medium
            }
            Text {
                visible: !root.autoTileMode
                text: root.tileSize > 0 ? (root.tileSize + " px") : qsTr("不分块")
                color: Theme.colors.mutedForeground; font.pixelSize: 10
            }
            Rectangle {
                width: _tileModeLbl.implicitWidth + 12; height: 20; radius: Theme.radius.sm
                color: root.autoTileMode ? Theme.colors.primary : Theme.colors.muted
                border.width: root.autoTileMode ? 0 : 1; border.color: Theme.colors.border
                Text {
                    id: _tileModeLbl; anchors.centerIn: parent
                    text: root.autoTileMode ? qsTr("自动") : qsTr("手动")
                    color: root.autoTileMode ? Theme.colors.textOnPrimary : Theme.colors.foreground; font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.autoTileMode = !root.autoTileMode
                        if (root.autoTileMode) root.tileSize = 0
                        root.paramsChanged()
                    }
                }
            }
        }

        Controls.Slider {
            Layout.fillWidth: true; visible: !root.autoTileMode
            from: 64; to: 1024; stepSize: 64
            value: root.tileSize > 0 ? root.tileSize : 512
            onMoved: { root.tileSize = value; root.paramsChanged() }
        }

        Text {
            Layout.fillWidth: true
            text: root.autoTileMode
                  ? qsTr("自动模式：根据图像分辨率与显存自动选择最优分块大小，推理时静默应用。")
                  : qsTr("手动模式：较小分块占用更少显存，但处理更慢。")
            color: Theme.colors.mutedForeground; font.pixelSize: 10; wrapMode: Text.WordWrap
        }
    }

    // ===== 模型特定参数 =====
    ColumnLayout {
        id: _modelParamsSection
        Layout.fillWidth: true; spacing: 6
        visible: {
            if (!currentModelInfo) return false
            var p = currentModelInfo.supportedParams
            if (!p || typeof p !== 'object') return false
            try { return Object.keys(p).length > 0 } catch(e) { return false }
        }

        Divider {}

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            ColoredIcon { source: Theme.icon("sliders"); iconSize: 13; color: Theme.colors.mutedForeground }
            Text { text: qsTr("模型参数"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Text {
                text: qsTr("重置"); color: Theme.colors.primary; font.pixelSize: 10
                visible: Object.keys(root.modelParams).length > 0
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: { root.modelParams = {}; root.paramsChanged() }
                }
            }
        }

        Repeater {
            id: _paramsRepeater
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
                        var copy = JSON.parse(JSON.stringify(param))
                        copy.key = key
                        items.push(copy)
                    }
                    return items
                } catch(e) { return [] }
            }

            delegate: ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                opacity: root._isModelSwitching ? 0.5 : 1.0
                Behavior on opacity { NumberAnimation { duration: 150 } }

                property string paramKey:     modelData.key
                property string paramType:    modelData.type || "float"
                property var    paramDefault: modelData["default"]

                RowLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { text: root.getParamLabel(modelData); color: Theme.colors.foreground; font.pixelSize: 11; Layout.fillWidth: true }
                    Rectangle {
                        height: 14; radius: Theme.radius.sm
                        color: Theme.colors.primarySubtle; border.width: 1; border.color: Theme.colors.primary
                        visible: root._autoReady && root._autoParams[paramKey] !== undefined && root.modelParams[paramKey] === undefined
                        implicitWidth: _autoBadgeTxt.implicitWidth + 10
                        Text { id: _autoBadgeTxt; anchors.centerIn: parent; text: qsTr("自动"); color: Theme.colors.primary; font.pixelSize: 9 }
                    }
                    Text {
                        text: {
                            var val = root.modelParams[paramKey]
                            var dv  = (val !== undefined) ? val : paramDefault
                            if (paramType === "bool") return dv ? qsTr("开启") : qsTr("关闭")
                            return dv !== undefined ? dv.toString() : ""
                        }
                        color: Theme.colors.mutedForeground; font.pixelSize: 11
                    }
                }

                Controls.Slider {
                    id: _sliderCtrl
                    Layout.fillWidth: true
                    visible: paramType === "int" || paramType === "float"
                    from: modelData.min !== undefined ? modelData.min : 0
                    to:   modelData.max !== undefined ? modelData.max : 100
                    stepSize: paramType === "int" ? 1 : (modelData.step || 0.1)
                    value: { var v = root.modelParams[paramKey]; return v !== undefined ? v : (paramDefault !== undefined ? paramDefault : from) }
                    Behavior on value { enabled: !_sliderCtrl.pressed; NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    onMoved: {
                        var np = JSON.parse(JSON.stringify(root.modelParams))
                        np[paramKey] = value; root.modelParams = np; root.paramsChanged()
                    }
                }

                ComboBox {
                    Layout.fillWidth: true; visible: paramType === "enum"
                    model: modelData.values || []
                    currentIndex: {
                        var v = root.modelParams[paramKey]
                        if (v === undefined) v = paramDefault
                        return Math.max(0, (modelData.values || []).indexOf(v))
                    }
                    onCurrentIndexChanged: {
                        if (visible && !root._isModelSwitching) {
                            var vals = modelData.values || []
                            if (currentIndex >= 0 && currentIndex < vals.length) {
                                var np = JSON.parse(JSON.stringify(root.modelParams))
                                np[paramKey] = vals[currentIndex]; root.modelParams = np; root.paramsChanged()
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true; visible: paramType === "bool"; spacing: 8
                    Controls.Switch {
                        id: _boolSwitch
                        checked: { var v = root.modelParams[paramKey]; return v !== undefined ? v : (paramDefault !== undefined ? paramDefault : false) }
                        onToggled: {
                            var np = JSON.parse(JSON.stringify(root.modelParams))
                            np[paramKey] = checked; root.modelParams = np; root.paramsChanged()
                        }
                    }
                    Text { text: _boolSwitch.checked ? qsTr("开启") : qsTr("关闭"); color: Theme.colors.mutedForeground; font.pixelSize: 11 }
                    Item { Layout.fillWidth: true }
                }

                TextField {
                    Layout.fillWidth: true; visible: paramType === "string"
                    placeholderText: modelData.placeholder || ""
                    text: { var v = root.modelParams[paramKey]; return v !== undefined ? v : (paramDefault !== undefined ? paramDefault : "") }
                    onEditingFinished: {
                        var np = JSON.parse(JSON.stringify(root.modelParams))
                        np[paramKey] = text; root.modelParams = np; root.paramsChanged()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.getParamDescription(modelData)
                    color: Theme.colors.mutedForeground; font.pixelSize: 10; wrapMode: Text.WordWrap; visible: text !== ""
                }
            }
        }
    }

    // ===== 语言切换监听 =====
    Connections {
        target: SettingsController
        function onLanguageChanged() {
            if (registry && modelId !== "") currentModelInfo = registry.getModelInfoMap(modelId)
            _paramsRepeater.model = _paramsRepeater.model
        }
    }
    Connections {
        target: Theme
        function onLanguageChanged() { _paramsRepeater.model = _paramsRepeater.model }
    }

    // ===== 内部辅助组件 =====
    component Divider: Rectangle {
        Layout.fillWidth: true; height: 1; color: Theme.colors.border
    }

    component Tag: Rectangle {
        property string text: ""
        property bool   accent: false
        height: 18; radius: Theme.radius.sm
        color: accent ? Theme.colors.primarySubtle : Theme.colors.muted
        border.width: accent ? 1 : 0; border.color: Theme.colors.primary
        implicitWidth: _tagTxt.implicitWidth + 12
        Text {
            id: _tagTxt; anchors.centerIn: parent; text: parent.text
            font.pixelSize: 9
            color: parent.accent ? Theme.colors.primary : Theme.colors.mutedForeground
        }
    }

    component Badge: Rectangle {
        property string text: ""
        property color  color: Theme.colors.primary
        height: 18; radius: Theme.radius.sm
        implicitWidth: _badgeTxt.implicitWidth + 12
        Rectangle { anchors.fill: parent; radius: parent.radius; color: parent.color; opacity: 0.12 }
        Text { id: _badgeTxt; anchors.centerIn: parent; text: parent.text; font.pixelSize: 9; color: parent.color }
    }

    component InfoBanner: Rectangle {
        property string text: ""
        property bool   isWarning: false
        Layout.fillWidth: true
        height: _bannerTxt.implicitHeight + 12; radius: Theme.radius.sm
        color: isWarning ? Theme.colors.warningSubtleBg : Theme.colors.primarySubtle
        border.width: 1; border.color: isWarning ? Theme.colors.warning : Theme.colors.primary
        Text {
            id: _bannerTxt
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
            text: parent.text; wrapMode: Text.WordWrap; font.pixelSize: 10
            color: parent.isWarning ? Theme.colors.warning : Theme.colors.primary
        }
    }
} 
