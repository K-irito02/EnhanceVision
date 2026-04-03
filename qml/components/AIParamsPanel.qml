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
    property bool useGpu: false
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
    property var  currentModelInfo: (registry && modelId !== "") ? registry.getModelInfoMap(modelId) : null
    property bool _isModelSwitching:   false
    property var  _pendingParams:      ({})
    property bool autoTileMode: true
    property int  computedAutoTileSize: 0
    property var  _autoParams: ({})
    property int _paramsModelVersion: 0
    
    // ── 延迟处理定时器
    Timer {
        id: _recomputeAutoParamsTimer
        interval: 100
        onTriggered: _recomputeAutoParams()
    }

    // ── 自动优化就绪状态
    readonly property bool _autoReady: {
        return currentMediaSize.width > 0
            && currentModelInfo !== null
            && Object.keys(_autoParams).length > 0
    }

    onCurrentModelInfoChanged: function(info) {
        if (info) {
            _isModelSwitching = true
            autoTileMode      = true
            tileSize          = 0
            computedAutoTileSize = 0
            _autoParams       = {}
            _pendingParams    = {}
            modelParams       = {}
            _paramsModelVersion++
            _recomputeAutoParamsTimer.restart()
            _isModelSwitching = false
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
        return param[zhKey] || param.label || ""
    }
    function getParamLabel(param) { 
        return _localText(param, "label", "label_en") || param.label || param.key || "" 
    }
    function getParamDescription(param) { 
        return _localText(param, "description", "description_en") || param.description || ""
    }

    function hasParamModifications() {
        if (!currentModelInfo || !currentModelInfo.supportedParams) return false
        var params = currentModelInfo.supportedParams
        var keys = Object.keys(params)
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i]
            var param = params[key]
            var userValue = root.modelParams[key]
            var defaultValue = param["default"]
            if (userValue !== undefined && userValue !== defaultValue) {
                return true
            }
        }
        return false
    }

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

    // ===== 推理设备选择（CPU / Vulkan GPU） =====
    ColumnLayout {
        id: _deviceSection
        Layout.fillWidth: true; spacing: 10; visible: engine !== null

        readonly property bool _gpuReady: engine ? engine.gpuAvailable : false
        readonly property string _gpuName: engine ? engine.gpuDeviceName : ""

        // ── 标题行 ──
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            ColoredIcon {
                source: Theme.icon("zap"); iconSize: 14
                color: root.useGpu && _deviceSection._gpuReady ? Theme.colors.primary : Theme.colors.mutedForeground
            }
            Text { text: qsTr("推理设备"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.Medium }
            Item { Layout.fillWidth: true }
            Text {
                id: _statusText
                text: root.useGpu && _deviceSection._gpuReady
                      ? qsTr("GPU 模式")
                      : qsTr("CPU 模式")
                color: Theme.colors.primary
                font.pixelSize: 10; font.weight: Font.Medium
            }
        }

        // ── 卡片容器（固定高度确保中英文切换时一致） ──
        RowLayout {
            Layout.fillWidth: true; spacing: 10

            // ── CPU 卡片 ──
            Rectangle {
                id: _cpuCard
                Layout.fillWidth: true
                height: 72
                radius: Theme.radius.md
                color: !root.useGpu ? Theme.colors.primarySubtle : Theme.colors.card
                border.width: !root.useGpu ? 2 : 1
                border.color: !root.useGpu ? Theme.colors.primary : Theme.colors.border
                clip: true
                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }

                RowLayout {
                    anchors { fill: parent; margins: 10 }
                    spacing: 8

                    Rectangle {
                        width: 24; height: 24; radius: Theme.radius.sm
                        color: !root.useGpu ? Theme.colors.primary : Theme.colors.muted
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("cpu"); iconSize: 12
                            color: !root.useGpu ? Theme.colors.textOnPrimary : Theme.colors.mutedForeground
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: qsTr("CPU 推理")
                            color: Theme.colors.foreground
                            font.pixelSize: 11; font.weight: Font.DemiBold
                        }
                        Text {
                            text: qsTr("稳定通用，速度较慢")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 9
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    id: _cpuMouseArea
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: { if (root.useGpu) { root.useGpu = false; _onBackendChanged(); } }
                    hoverEnabled: true
                    onEntered: if (root.useGpu) _cpuCard.opacity = 0.85
                    onExited: _cpuCard.opacity = 1.0
                }
                Behavior on opacity { NumberAnimation { duration: Theme.animation.fast } }

                Tooltip {
                    id: _cpuTooltip
                    text: {
                        var isEn = (Theme.language === "en_US")
                        return isEn
                            ? "CPU Inference: Uses processor for AI computation.\nPros: Compatible with all hardware, stable, controllable memory.\nCons: Slower processing, longer time for large images and videos."
                            : "CPU 推理模式：使用处理器进行 AI 计算。\n优点：兼容所有硬件，稳定性高，内存占用可控。\n缺点：处理速度较慢，大图和视频耗时较长。"
                    }
                }

                Timer {
                    id: _cpuTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (_cpuMouseArea.containsMouse) {
                            _cpuTooltip.show(_cpuCard)
                        }
                    }
                }

                Connections {
                    target: _cpuMouseArea
                    function onContainsMouseChanged() {
                        if (_cpuMouseArea.containsMouse) {
                            _cpuTooltipTimer.start()
                        } else {
                            _cpuTooltipTimer.stop()
                            _cpuTooltip.close()
                        }
                    }
                }
            }

            // ── GPU 卡片 ──
            Rectangle {
                id: _gpuCard
                Layout.fillWidth: true
                height: 72
                radius: Theme.radius.md
                enabled: _deviceSection._gpuReady
                color: {
                    if (!enabled) return Theme.colors.muted
                    return root.useGpu ? Theme.colors.primarySubtle : Theme.colors.card
                }
                border.width: root.useGpu && enabled ? 2 : 1
                border.color: {
                    if (!enabled) return Theme.colors.border
                    return root.useGpu ? Theme.colors.primary : Theme.colors.border
                }
                opacity: enabled ? 1.0 : 0.55
                clip: true
                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
                Behavior on opacity { NumberAnimation { duration: Theme.animation.fast } }

                RowLayout {
                    anchors { fill: parent; margins: 10 }
                    spacing: 8

                    Rectangle {
                        width: 24; height: 24; radius: Theme.radius.sm
                        color: root.useGpu && _deviceSection._gpuReady ? Theme.colors.primary : (_deviceSection._gpuReady ? Theme.colors.muted : Theme.colors.border)
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("zap"); iconSize: 12
                            color: root.useGpu && _deviceSection._gpuReady
                                  ? Theme.colors.textOnPrimary
                                  : (_deviceSection._gpuReady ? Theme.colors.mutedForeground : Theme.colors.border)
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: qsTr("GPU 推理")
                            color: _deviceSection._gpuReady ? Theme.colors.foreground : Theme.colors.mutedForeground
                            font.pixelSize: 11; font.weight: Font.DemiBold
                        }
                        Text {
                            id: _gpuNameText
                            text: _deviceSection._gpuReady
                                  ? _deviceSection._gpuName
                                  : qsTr("不可用")
                            color: _deviceSection._gpuReady ? Theme.colors.success : Theme.colors.warning
                            font.pixelSize: 9
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            MouseArea {
                                id: _gpuNameHint
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            Tooltip {
                                id: _gpuNameTooltip
                                text: _deviceSection._gpuName
                            }

                            Timer {
                                id: _gpuNameTooltipTimer
                                interval: 500
                                repeat: false
                                onTriggered: {
                                    if (_gpuNameHint.containsMouse && _deviceSection._gpuReady) {
                                        _gpuNameTooltip.show(_gpuNameText)
                                    }
                                }
                            }

                            Connections {
                                target: _gpuNameHint
                                function onContainsMouseChanged() {
                                    if (_gpuNameHint.containsMouse && _deviceSection._gpuReady) {
                                        _gpuNameTooltipTimer.start()
                                    } else {
                                        _gpuNameTooltipTimer.stop()
                                        _gpuNameTooltip.close()
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    id: _gpuCardMouseArea
                    anchors.fill: parent
                    cursorShape: _deviceSection._gpuReady ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                    enabled: _deviceSection._gpuReady
                    onClicked: { if (!root.useGpu && _deviceSection._gpuReady) { root.useGpu = true; _onBackendChanged(); } }
                    hoverEnabled: true
                    onEntered: if (_deviceSection._gpuReady && !root.useGpu) _gpuCard.opacity = 0.9
                    onExited: if (_deviceSection._gpuReady) _gpuCard.opacity = 1.0
                }

                Tooltip {
                    id: _gpuTooltip
                    text: {
                        var isEn = (Theme.language === "en_US")
                        return isEn
                            ? "GPU Inference: Uses GPU for AI acceleration.\nPros: Fast processing, ideal for large images and videos.\nCons: High VRAM usage, incompatible with some devices."
                            : "GPU 推理模式：使用显卡进行 AI 加速。\n优点：处理速度快，适合大图和视频处理。\n缺点：显存占用高，部分设备不兼容。"
                    }
                }

                Timer {
                    id: _gpuTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (_gpuCardMouseArea.containsMouse && _deviceSection._gpuReady) {
                            _gpuTooltip.show(_gpuCard)
                        }
                    }
                }

                Connections {
                    target: _gpuCardMouseArea
                    function onContainsMouseChanged() {
                        if (_gpuCardMouseArea.containsMouse && _deviceSection._gpuReady) {
                            _gpuTooltipTimer.start()
                        } else {
                            _gpuTooltipTimer.stop()
                            _gpuTooltip.close()
                        }
                    }
                }
            }
        }
    }

    function _onBackendChanged() {
        root.paramsChanged()
    }

    Connections {
        target: engine
        function onAutoParamsComputed(autoTileSize) { root.computedAutoTileSize = autoTileSize }
    }

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
            var _ver = root._paramsModelVersion
            if (!currentModelInfo) return false
            var p = currentModelInfo.supportedParams
            if (!p || typeof p !== 'object') return false
            try { 
                var keys = Object.keys(p)
                return keys.length > 0 
            } catch(e) { return false }
        }

        Divider {}

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            ColoredIcon { source: Theme.icon("sliders"); iconSize: 13; color: Theme.colors.mutedForeground }
            Text { text: qsTr("模型参数"); color: Theme.colors.foreground; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Rectangle {
                width: _resetLbl.implicitWidth + 12; height: 20; radius: Theme.radius.sm
                color: hasParamModifications() ? Theme.colors.primary : Theme.colors.muted
                border.width: hasParamModifications() ? 0 : 1
                border.color: Theme.colors.border
                visible: currentModelInfo !== null && currentModelInfo.supportedParams && Object.keys(currentModelInfo.supportedParams).length > 0
                Text {
                    id: _resetLbl; anchors.centerIn: parent
                    text: qsTr("重置")
                    color: hasParamModifications() ? Theme.colors.textOnPrimary : Theme.colors.foreground
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var np = {}
                        if (currentModelInfo && currentModelInfo.supportedParams) {
                            var params = currentModelInfo.supportedParams
                            var keys = Object.keys(params)
                            for (var i = 0; i < keys.length; i++) {
                                var key = keys[i]
                                var param = params[key]
                                if (param && param.type === "bool") {
                                    np[key] = false
                                } else if (param && param["default"] !== undefined) {
                                    np[key] = param["default"]
                                }
                            }
                        }
                        root.modelParams = np
                        root.paramsChanged()
                    }
                }
            }
        }

        Repeater {
            id: _paramsRepeater
            model: {
                var _ver = root._paramsModelVersion
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
                            if (paramType === "bool") return ""
                            var val = root.modelParams[paramKey]
                            var dv  = (val !== undefined) ? val : paramDefault
                            return dv !== undefined ? dv.toString() : ""
                        }
                        color: Theme.colors.mutedForeground; font.pixelSize: 11
                        Layout.rightMargin: 8
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
                        property var currentValue: root.modelParams[paramKey]
                        checked: currentValue !== undefined ? currentValue : (paramDefault !== undefined ? paramDefault : false)
                        onCurrentValueChanged: {
                            checked = currentValue !== undefined ? currentValue : (paramDefault !== undefined ? paramDefault : false)
                        }
                        onToggled: {
                            var np = JSON.parse(JSON.stringify(root.modelParams))
                            np[paramKey] = checked
                            root.modelParams = np
                            root.paramsChanged()
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
                    color: Theme.colors.mutedForeground; font.pixelSize: 10; wrapMode: Text.WordWrap
                    visible: text !== "" && text !== root.getParamLabel(modelData)
                }
            }
        }
    }

    // ===== 语言切换监听 =====
    Connections {
        target: SettingsController
        function onLanguageChanged() {
            _paramsRepeater.model = _paramsRepeater.model
            _cpuTooltip.close()
            _gpuTooltip.close()
        }
    }
    Connections {
        target: Theme
        function onLanguageChanged() {
            _paramsRepeater.model = _paramsRepeater.model
            _cpuTooltip.close()
            _gpuTooltip.close()
        }
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
