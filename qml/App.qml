import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./styles"
import "./components"
import "./pages"
import "./utils"
import EnhanceVision.Utils

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

    property alias shaderBrightness: controlPanel.brightness
    property alias shaderContrast: controlPanel.contrast
    property alias shaderSaturation: controlPanel.saturation
    property alias shaderHue: controlPanel.hue
    property alias shaderSharpness: controlPanel.sharpness
    property alias shaderBlur: controlPanel.blur
    property alias shaderExposure: controlPanel.exposure
    property alias shaderGamma: controlPanel.gamma
    property alias shaderTemperature: controlPanel.temperature
    property alias shaderTint: controlPanel.tint
    property alias shaderVignette: controlPanel.vignette
    property alias shaderHighlights: controlPanel.highlights
    property alias shaderShadows: controlPanel.shadows
    property alias hasShaderModifications: controlPanel.hasShaderModifications

    function clearAllFocus() {
        focusCatcher.forceActiveFocus()
    }

    Item {
        id: focusCatcher
        focus: true
        visible: false
    }

    TapHandler {
        onTapped: function(eventPoint, button) {
            focusCatcher.forceActiveFocus()
        }
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
                Layout.preferredWidth: sidebarExpanded ? 200 : 0
                Layout.fillHeight: true
                Layout.maximumWidth: sidebarExpanded ? 200 : 0
                Layout.minimumWidth: 0
                clip: true

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
                    onProcessingModeChanged: root.processingMode = processingMode
                    onExpandControlPanel: {
                        root.controlPanelCollapsed = false
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
}
