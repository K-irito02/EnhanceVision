import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./styles"
import "./components"
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
    
    property bool sidebarExpanded: true
    property bool controlPanelExpanded: true
    property bool controlPanelCollapsed: false
    property int currentPage: 0
    property int processingMode: 0
    
    // 侧边栏宽度（可拖拽调整）
    property int sidebarWidth: 200
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

            onToggleSidebar: root.sidebarExpanded = !root.sidebarExpanded
            onToggleControlPanel: root.controlPanelExpanded = !root.controlPanelExpanded
            
            onToggleBrowseMode: {
                var hasExpanded = root.sidebarExpanded || !root.controlPanelCollapsed
                if (hasExpanded) {
                    root.sidebarExpanded = false
                    root.controlPanelCollapsed = true
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
            Sidebar {
                id: sidebar
                visible: true
                expanded: root.sidebarExpanded
                Layout.preferredWidth: sidebarExpanded ? root.sidebarWidth : 0
                Layout.fillHeight: true
                Layout.maximumWidth: sidebarExpanded ? root.sidebarWidth : 0
                Layout.minimumWidth: 0
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
                    root.sidebarWidth = Math.max(root.sidebarMinWidth, 
                                                 Math.min(root.sidebarMaxWidth, Math.round(newWidth)))
                }

                Behavior on Layout.preferredWidth {
                    id: sidebarAnimation
                    enabled: true
                    NumberAnimation {
                        duration: Theme.animation.normal
                        easing.type: Easing.OutCubic
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
                    processingMode: root.processingMode
                    aiScaleFactor: root.aiScaleFactor
                    onProcessingModeChanged: root.processingMode = processingMode
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
                    root.controlPanelCollapsed = !root.controlPanelCollapsed
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

    // ========== 崩溃恢复提示弹窗 ==========
    Rectangle {
        id: crashRecoveryDialog
        anchors.centerIn: parent
        width: 400
        height: crashContent.implicitHeight + 48
        radius: Theme.radius.lg
        color: Theme.colors.card
        border.width: 1
        border.color: Theme.colors.cardBorder
        z: 30000
        visible: SettingsController.crashDetectedOnStartup

        ColumnLayout {
            id: crashContent
            anchors.fill: parent
            anchors.margins: 24
            spacing: 16

            RowLayout {
                spacing: 10
                ColoredIcon { 
                    iconSize: 24
                    source: Theme.icon("alert-triangle")
                    color: Theme.colors.warning
                }
                Text { 
                    text: qsTr("检测到上次应用异常退出")
                    color: Theme.colors.foreground
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }

            Text {
                text: qsTr("为确保稳定性，已自动关闭\"自动重新处理\"功能。\n您可以在设置中重新开启此功能。")
                color: Theme.colors.mutedForeground
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("知道了")
                variant: "primary"
                Layout.alignment: Qt.AlignRight
                onClicked: crashRecoveryDialog.visible = false
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
        onPressed: function(mouse) {
            var focusObject = Window.window ? Window.window.activeFocusItem : null
            if (root._isInputItem(focusObject)) {
                root.clearAllFocus()
                mouse.accepted = true
            } else {
                mouse.accepted = false
            }
        }
    }

    TapHandler {
        onTapped: function(eventPoint, button) {
            root.clearAllFocus()
        }
    }
}
