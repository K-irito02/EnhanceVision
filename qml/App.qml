import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./styles"
import "./components"
import "./controls"
import "./pages"
import "./utils"
import EnhanceVision.Utils
import EnhanceVision.Controllers

/**
 * @brief 应用根组件
 * 
 * 管理整体布局结构，包括标题栏、侧边栏、主内容区域和控制面板
 * 参考功能设计文档 0.2 和 UI 设计文档 08-ui-design.md
 */
FocusScope {
    id: root
    objectName: "AppRoot"
    
    property bool sidebarExpanded: UIStateController.sidebarExpanded
    property bool controlPanelExpanded: true
    property bool controlPanelCollapsed: UIStateController.controlPanelCollapsed
    property int currentPage: 0
    property int processingMode: UIStateController.processingMode
    
    // 侧边栏宽度（可拖拽调整）
    property int sidebarWidth: UIStateController.sidebarWidth
    readonly property int sidebarMinWidth: 160
    readonly property int sidebarMaxWidth: 320

    property alias shaderBrightness: controlPanel.brightness
    property alias shaderContrast: controlPanel.contrast
    property alias shaderSaturation: controlPanel.saturation
    property alias shaderHue: controlPanel.hue
    property alias shaderSharpness: controlPanel.sharpness
    property alias shaderBlur: controlPanel.blur
    property alias shaderDenoise: controlPanel.denoise
    property alias shaderExposure: controlPanel.exposure
    property alias shaderGamma: controlPanel.gamma
    property alias shaderTemperature: controlPanel.temperature
    property alias shaderTint: controlPanel.tint
    property alias shaderVignette: controlPanel.vignette
    property alias shaderHighlights: controlPanel.highlights
    property alias shaderShadows: controlPanel.shadows
    property alias hasShaderModifications: controlPanel.hasShaderModifications
    property alias aiSelectedCategory: controlPanel.aiSelectedCategory
    property alias aiUseGpu: controlPanel.aiUseGpu
    property alias aiTileSize: controlPanel.aiTileSize
    property alias aiScaleFactor: controlPanel.aiScaleFactor
    function getAIParams() { return controlPanel.getAIParams() }

    function clearAllFocus() {
        focusCatcher.forceActiveFocus()
    }

    Item {
        id: focusCatcher
        focus: true
        visible: false
    }

    Connections {
        target: WindowHelper
        function onResizeStarted() {
            sidebarAnimation.enabled = false
            controlPanelAnimation.enabled = false
        }
        function onResizeFinished() {
            sidebarAnimation.enabled = true
            controlPanelAnimation.enabled = true
        }
    }

    // ========== 背景 ==========
    Rectangle {
        id: backgroundRect
        anchors.fill: parent
        color: Theme.colors.background
    }

    // ========== 主布局 ==========
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ========== 标题栏 ==========
        TitleBar {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            controlPanelCollapsed: root.controlPanelCollapsed

            onToggleSidebar: {
                root.sidebarExpanded = !root.sidebarExpanded
                UIStateController.sidebarExpanded = root.sidebarExpanded
            }
            onToggleControlPanel: {
                root.controlPanelCollapsed = !root.controlPanelCollapsed
                UIStateController.controlPanelCollapsed = root.controlPanelCollapsed
            }
            
            onToggleBrowseMode: {
                var hasExpanded = root.sidebarExpanded || !root.controlPanelCollapsed
                if (hasExpanded) {
                    root.sidebarExpanded = false
                    root.controlPanelCollapsed = true
                    UIStateController.sidebarExpanded = false
                    UIStateController.controlPanelCollapsed = true
                }
            }
            
            onNavigateToSettings: root.currentPage = 1
            onNewSession: {
                if (sessionController) {
                    var newId = sessionController.createSession()
                    sessionController.switchSession(newId)
                }
            }
        }

        // ========== 主内容区域 ==========
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ========== 侧边栏 ==========
            Item {
                id: sidebarContainer
                Layout.preferredWidth: root.sidebarExpanded ? root.sidebarWidth : 0
                Layout.fillHeight: true
                Layout.maximumWidth: root.sidebarExpanded ? root.sidebarMaxWidth : 0
                Layout.minimumWidth: root.sidebarExpanded ? root.sidebarMinWidth : 0
                
                Behavior on Layout.preferredWidth {
                    id: sidebarAnimation
                    enabled: true
                    NumberAnimation {
                        duration: Theme.animation.normal
                        easing.type: Easing.OutCubic
                    }
                }
                
                Sidebar {
                    id: sidebar
                    anchors.fill: parent
                    visible: true
                    expanded: root.sidebarExpanded
                    clip: true
                    minWidth: root.sidebarMinWidth
                    maxWidth: root.sidebarMaxWidth
                    
                    onResizeStarted: {
                        sidebarAnimation.enabled = false
                    }
                    
                    onResizeFinished: {
                        sidebarAnimation.enabled = true
                    }
                    
                    onResizeDelta: function(delta) {
                        var newWidth = root.sidebarWidth + delta
                        var clampedWidth = Math.max(root.sidebarMinWidth, 
                                                   Math.min(root.sidebarMaxWidth, Math.round(newWidth)))
                        root.sidebarWidth = clampedWidth
                        UIStateController.sidebarWidth = clampedWidth
                    }
                }
                
            }
            
            // ========== 主页面容器 ==========
            StackLayout {
                id: pageStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage

                // 主页面
                MainPage {
                    id: mainPage
                    aiScaleFactor: root.aiScaleFactor
                    onExpandControlPanel: {
                        root.controlPanelCollapsed = false
                    }
                    onNewSessionRequested: {
                        if (sessionController) {
                            var newId = sessionController.createSession()
                            sessionController.switchSession(newId)
                        }
                    }
                }

                // 设置页面
                SettingsPage {
                    id: settingsPage
                    onGoBack: root.currentPage = 0
                }
            }

            // ========== 右侧控制面板 ==========
            ControlPanel {
                id: controlPanel
                visible: root.currentPage === 0
                collapsed: root.controlPanelCollapsed
                processingMode: root.processingMode
                Layout.preferredWidth: controlPanelCollapsed ? 52 : (controlPanelExpanded ? 280 : 0)
                Layout.fillHeight: true
                Layout.maximumWidth: controlPanelCollapsed ? 52 : (controlPanelExpanded ? 280 : 0)
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignRight
                clip: true

                onCollapseToggleRequested: {
                    // 直接同步 UIStateController 的状态值，避免双重取反导致首次点击无效
                    root.controlPanelCollapsed = UIStateController.controlPanelCollapsed
                }

                Behavior on Layout.preferredWidth {
                    id: controlPanelAnimation
                    enabled: true
                    NumberAnimation {
                        duration: Theme.animation.normal
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }

    // ========== Toast 提示组件 ==========
    Toast {
        id: toast
        anchors.top: parent.top
        anchors.topMargin: 56
        anchors.horizontalCenter: parent.horizontalCenter
        z: 10000

        Component.onCompleted: {
            NotificationManager.toastComponent = toast
        }
    }

    // ========== 启动恢复弹窗 ==========
    Item {
        id: startupRecoveryOverlay
        anchors.fill: parent
        z: 30000
        opacity: visible ? 1 : 0
        visible: typeof taskRecoveryController !== "undefined"
                 && taskRecoveryController
                 && taskRecoveryController.hasPendingRecovery

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.animation.normal
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.isDark ? Qt.rgba(2 / 255, 8 / 255, 20 / 255, 0.8) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.28)
        }

        MouseArea {
            anchors.fill: parent
            enabled: startupRecoveryOverlay.visible
        }

        Rectangle {
            id: startupRecoveryCard
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 760)
            implicitHeight: startupRecoveryColumn.implicitHeight + 44
            height: Math.min(implicitHeight, parent.height - 48)
            radius: Theme.radius.xxl
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.cardBorder
            scale: startupRecoveryOverlay.visible ? 1 : 0.97

            Behavior on scale {
                NumberAnimation {
                    duration: Theme.animation.normal
                    easing.type: Easing.OutCubic
                }
            }

            ColumnLayout {
                id: startupRecoveryColumn
                anchors.fill: parent
                anchors.margins: 22
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Rectangle {
                        width: 44
                        height: 44
                        radius: 14
                        color: Theme.colors.warningSubtleBg
                        border.width: 1
                        border.color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.24)

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("refresh-cw")
                            iconSize: 20
                            color: Theme.colors.warning
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: qsTr("上次退出时仍有未完成任务")
                            color: Theme.colors.foreground
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: {
                                if (typeof taskRecoveryController === "undefined" || !taskRecoveryController) {
                                    return ""
                                }
                                var reason = taskRecoveryController.shutdownReason || ""
                                var abnormal = reason !== "" && reason !== "normal" && reason !== "main_window_closed" && reason !== "user_request"
                                return abnormal
                                    ? qsTr("检测到应用异常退出。您可以恢复关闭前的任务状态，或直接将这些未完成任务标记为失败。")
                                    : qsTr("检测到上次关闭前仍有未完成任务。您可以恢复关闭前的任务状态，或直接将这些未完成任务标记为失败。")
                            }
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radius.lg
                    color: Theme.isDark ? Qt.rgba(30 / 255, 86 / 255, 208 / 255, 0.12) : Qt.rgba(0 / 255, 47 / 255, 167 / 255, 0.06)
                    border.width: 1
                    border.color: Theme.isDark ? Qt.rgba(91 / 255, 141 / 255, 239 / 255, 0.22) : Qt.rgba(0 / 255, 47 / 255, 167 / 255, 0.12)
                    implicitHeight: summaryColumn.implicitHeight + 18

                    ColumnLayout {
                        id: summaryColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: typeof taskRecoveryController !== "undefined" && taskRecoveryController
                                ? qsTr("涉及 %1 个会话标签，共 %2 张消息卡片待恢复。")
                                    .arg(taskRecoveryController.recoverySummary.length)
                                    .arg((function() {
                                        var total = 0
                                        for (var i = 0; i < taskRecoveryController.recoverySummary.length; ++i) {
                                            total += taskRecoveryController.recoverySummary[i].totalCount || 0
                                        }
                                        return total
                                    })())
                                : ""
                            color: Theme.colors.foreground
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: Math.min(recoverySummaryColumn.implicitHeight, 320)
                    clip: true

                    ColumnLayout {
                        id: recoverySummaryColumn
                        width: parent.width
                        spacing: 10

                        Repeater {
                            model: typeof taskRecoveryController !== "undefined" && taskRecoveryController
                                ? taskRecoveryController.recoverySummary
                                : []

                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                radius: Theme.radius.lg
                                color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(1, 1, 1, 0.92)
                                border.width: 1
                                border.color: Theme.colors.border
                                implicitHeight: recoverySessionColumn.implicitHeight + 18

                                ColumnLayout {
                                    id: recoverySessionColumn
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Rectangle {
                                            width: 30
                                            height: 30
                                            radius: 10
                                            color: Theme.colors.primarySubtle

                                            ColoredIcon {
                                                anchors.centerIn: parent
                                                source: Theme.icon("folder")
                                                iconSize: 15
                                                color: Theme.colors.primary
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.sessionName || qsTr("当前会话")
                                                color: Theme.colors.foreground
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                                wrapMode: Text.Wrap
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("%1 张消息卡片待恢复").arg(modelData.totalCount || 0)
                                                color: Theme.colors.mutedForeground
                                                font.pixelSize: 11
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Rectangle {
                                            visible: (modelData.processingCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Theme.colors.primarySubtle
                                            implicitHeight: recoveryProcessingText.implicitHeight + 8
                                            implicitWidth: recoveryProcessingText.implicitWidth + 14

                                            Text {
                                                id: recoveryProcessingText
                                                anchors.centerIn: parent
                                                text: qsTr("关闭前处理中 %1").arg(modelData.processingCount || 0)
                                                color: Theme.colors.primary
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Rectangle {
                                            visible: (modelData.pendingCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Theme.colors.warningSubtleBg
                                            implicitHeight: recoveryPendingText.implicitHeight + 8
                                            implicitWidth: recoveryPendingText.implicitWidth + 14

                                            Text {
                                                id: recoveryPendingText
                                                anchors.centerIn: parent
                                                text: qsTr("关闭前等待 %1").arg(modelData.pendingCount || 0)
                                                color: Theme.colors.warning
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Rectangle {
                                            visible: (modelData.pausedCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Theme.isDark ? Qt.rgba(232 / 255, 237 / 255, 245 / 255, 0.08) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.06)
                                            border.width: 1
                                            border.color: Theme.isDark ? Qt.rgba(232 / 255, 237 / 255, 245 / 255, 0.12) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.08)
                                            implicitHeight: recoveryPausedText.implicitHeight + 8
                                            implicitWidth: recoveryPausedText.implicitWidth + 14

                                            Text {
                                                id: recoveryPausedText
                                                anchors.centerIn: parent
                                                text: qsTr("关闭前暂停 %1").arg(modelData.pausedCount || 0)
                                                color: Theme.colors.foreground
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Item { Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    visible: typeof taskRecoveryController !== "undefined"
                             && taskRecoveryController
                             && !taskRecoveryController.restoreAvailable
                    implicitHeight: recoveryUnavailableText.implicitHeight + 16
                    radius: Theme.radius.lg
                    color: Theme.colors.warningSubtleBg
                    border.width: 1
                    border.color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.2)

                    Text {
                        id: recoveryUnavailableText
                        anchors.fill: parent
                        anchors.margins: 10
                        text: qsTr("恢复快照不可用，当前只能将这些未完成任务统一标记为失败。")
                        color: Theme.colors.foreground
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("全部标记为失败")
                        variant: "secondary"
                        iconName: "x-circle"
                        onClicked: {
                            if (typeof taskRecoveryController !== "undefined" && taskRecoveryController) {
                                taskRecoveryController.resolveFailAllRecoverableTasks()
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("恢复关闭前状态")
                        variant: "primary"
                        iconName: "refresh-cw"
                        enabled: typeof taskRecoveryController !== "undefined"
                                 && taskRecoveryController
                                 && taskRecoveryController.restoreAvailable
                        onClicked: {
                            if (typeof taskRecoveryController !== "undefined" && taskRecoveryController) {
                                taskRecoveryController.resolveRestorePreviousState()
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== Dialog 对话框组件 ==========
    Dialog {
        id: dialog
        anchors.fill: parent
        z: 20000

        Component.onCompleted: {
            NotificationManager.dialogComponent = dialog
        }
        
        onPrimaryButtonClicked: {
            NotificationManager.handleConfirm()
        }
        
        onSecondaryButtonClicked: {
            NotificationManager.handleCancel()
        }
    }
    
    // ========== 离屏 Shader 渲染器 ==========
    OffscreenShaderRenderer {
        id: offscreenRenderer
        z: -1
    }

    property bool _inputHasFocus: false

    function _isInputItem(item) {
        if (!item) return false
        var typeName = item.toString()
        return typeName.indexOf("TextInput") >= 0 ||
               typeName.indexOf("TextField") >= 0 ||
               typeName.indexOf("TextEdit") >= 0
    }

    MouseArea {
        id: focusOverlay
        anchors.fill: parent
        z: 9998
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        onPressed: function(mouse) {
            var focusObject = Window.window ? Window.window.activeFocusItem : null
            if (root._isInputItem(focusObject)) {
                root.clearAllFocus()
            }
            mouse.accepted = false
        }
    }
    
    // ========== 侧边栏拖拽调整大小手柄 ==========
    Rectangle {
        id: sidebarResizeHandle
        x: sidebarContainer.x + sidebarContainer.width - width
        y: titleBar.height
        width: 6
        height: parent.height - titleBar.height
        color: "transparent"
        visible: root.sidebarExpanded
        z: 10000
        
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height
            color: sidebarResizeMouse.containsMouse || sidebarResizeMouse.pressed ?
                   Theme.colors.primary : "transparent"
            opacity: sidebarResizeMouse.containsMouse || sidebarResizeMouse.pressed ? 0.8 : 0 

            Behavior on opacity {
                NumberAnimation { duration: 100 }
            }
        }
        
        MouseArea {
            id: sidebarResizeMouse
            anchors.fill: parent
            anchors.leftMargin: -4
            anchors.rightMargin: -2
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            
            property real lastGlobalX: 0
            
            onPressed: function(mouse) {
                var globalPos = mapToGlobal(mouse.x, mouse.y)
                lastGlobalX = globalPos.x
                sidebar.resizeStarted()
            }
            
            onReleased: {
                sidebar.resizeFinished()
            }
            
            onPositionChanged: function(mouse) {
                if (pressed) {
                    var globalPos = mapToGlobal(mouse.x, mouse.y)
                    var delta = globalPos.x - lastGlobalX
                    lastGlobalX = globalPos.x
                    sidebar.resizeDelta(delta)
                }
            }
            
            onCanceled: {
                sidebar.resizeFinished()
            }
        }
    }

    TapHandler {
        onTapped: function(eventPoint, button) {
            root.clearAllFocus()
        }
    }
}
