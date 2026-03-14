import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "./styles"
import "./components"
import "./pages"
import "./utils"

/**
 * @brief 应用根组件
 * 
 * 管理整体布局结构，包括标题栏、侧边栏、主内容区域和控制面板
 * 参考功能设计文档 0.2 和 UI 设计文档 08-ui-design.md
 */
Rectangle {
    id: root
    color: Theme.colors.background

    // ========== 属性定义 ==========
    property bool sidebarExpanded: true
    property bool controlPanelExpanded: true
    property bool controlPanelCollapsed: false
    property bool browseMode: false
    property int currentPage: 0

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
                root.browseMode = !root.browseMode
                if (root.browseMode) {
                    root.sidebarExpanded = false
                    root.controlPanelExpanded = false
                }
            }
            onNavigateToSettings: root.currentPage = 1
            onNewSession: console.log("New session requested")
        }

        // ========== 主内容区域 ==========
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ========== 侧边栏 ==========
            Sidebar {
                id: sidebar
                visible: !root.browseMode || root.sidebarExpanded
                expanded: root.sidebarExpanded
                Layout.preferredWidth: sidebarExpanded ? 200 : 0
                Layout.fillHeight: true
                Layout.maximumWidth: sidebarExpanded ? 200 : 0
                Layout.minimumWidth: 0

                Behavior on Layout.preferredWidth {
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
                visible: root.currentPage === 0 && (!root.browseMode || root.controlPanelExpanded)
                collapsed: root.controlPanelCollapsed
                Layout.preferredWidth: controlPanelExpanded ? (controlPanelCollapsed ? 52 : 280) : 0
                Layout.fillHeight: true
                Layout.maximumWidth: controlPanelExpanded ? (controlPanelCollapsed ? 52 : 280) : 0
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignRight

                onCollapseToggleRequested: {
                    root.controlPanelCollapsed = !root.controlPanelCollapsed
                }

                Behavior on Layout.preferredWidth {
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
        z: 1000

        Component.onCompleted: {
            NotificationManager.toastComponent = toast
        }
    }

    // ========== Dialog 对话框组件 ==========
    Dialog {
        id: dialog
        anchors.fill: parent
        z: 2000

        Component.onCompleted: {
            NotificationManager.dialogComponent = dialog
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
