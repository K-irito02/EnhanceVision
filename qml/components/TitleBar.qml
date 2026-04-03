import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../styles"
import "../controls"
import EnhanceVision.Utils

/**
 * @brief 自定义标题栏组件
 * 
 * 包含窗口控制、主题切换、语言切换、设置等功能
 * 参考功能设计文档 0.2 节 3.1 顶部工具栏模块
 */
Rectangle {
    id: root
    
    // ========== 信号 ==========
    signal toggleSidebar()
    signal toggleControlPanel()
    signal toggleBrowseMode()
    signal navigateToSettings()
    signal navigateToHome()
    signal newSession()
    
    // ========== 视觉属性 ==========
    color: Theme.colors.titleBar
    height: 48
    
    // ========== 排除区域管理 ==========
    function updateExcludeRegions() {
        WindowHelper.clearExcludeRegions()
        
        // 左侧按钮组区域
        if (leftButtonsRow.width > 0) {
            WindowHelper.addExcludeRegion(leftButtonsRow.x, leftButtonsRow.y, leftButtonsRow.width, leftButtonsRow.height)
        }
        
        // 右侧按钮组区域
        if (rightButtonsRow.width > 0) {
            WindowHelper.addExcludeRegion(rightButtonsRow.x, rightButtonsRow.y, rightButtonsRow.width, rightButtonsRow.height)
        }
    }
    
    Component.onCompleted: {
        updateExcludeRegions()
    }
    
    onWidthChanged: {
        updateExcludeRegions()
    }
    
    // ========== 底部分隔线 ==========
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.colors.titleBarBorder
    }
    
    // ========== 内部布局 ==========
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 2
        
        // ========== 左侧：品牌标识 ==========
        RowLayout {
            spacing: 10
            Layout.alignment: Qt.AlignVCenter
            
            // 品牌图标 - 使用自定义 Logo
            Image {
                source: "qrc:/icons/app_icon.png"
                sourceSize.width: 28
                sourceSize.height: 28
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            
            // 应用标题
            Text {
                text: "EnhanceVision"
                color: Theme.colors.foreground
                font.pixelSize: 14
                font.weight: Font.DemiBold
                font.letterSpacing: 0.2
            }
            
            // 版本标签
            Rectangle {
                width: versionText.implicitWidth + 10
                height: 18
                radius: 9
                color: Theme.colors.primarySubtle
                
                Text {
                    id: versionText
                    anchors.centerIn: parent
                    text: "v0.1"
                    color: Theme.colors.primary
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }
        
        // ========== 中间：可拖动区域（由 Windows 系统处理） ==========
        Item {
            id: dragAreaContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            z: 0
        }
        
        // ========== 左侧：功能按钮组 ==========
        RowLayout {
            id: leftButtonsRow
            z: 1
            spacing: 2
            Layout.alignment: Qt.AlignVCenter
            
            // 新建会话按钮
            IconButton {
                iconName: "plus"
                iconSize: 16
                tooltip: qsTr("新建会话")
                onClicked: root.newSession()
            }
            
            // 侧边栏切换
            IconButton {
                iconName: "panel-left"
                iconSize: 16
                tooltip: qsTr("展开/收缩会话栏")
                onClicked: root.toggleSidebar()
            }
            
            // 分隔线
            Rectangle {
                width: 1
                height: 20
                color: Theme.colors.border
                Layout.alignment: Qt.AlignVCenter
            }
        }
        
        // ========== 右侧：功能按钮组 ==========
        RowLayout {
            id: rightButtonsRow
            spacing: 2
            Layout.alignment: Qt.AlignVCenter
            
            // 浏览模式按钮
            IconButton {
                iconName: "eye"
                iconSize: 16
                tooltip: qsTr("浏览模式")
                onClicked: root.toggleBrowseMode()
            }
            
            // 主题切换按钮
            IconButton {
                iconName: Theme.isDark ? "sun" : "moon"
                iconSize: 16
                tooltip: Theme.isDark ? qsTr("切换到亮色主题") : qsTr("切换到暗色主题")
                onClicked: Theme.toggle()
            }
            
            // 语言切换按钮
            IconButton {
                iconName: "globe"
                iconSize: 16
                tooltip: Theme.language === "zh_CN" ? qsTr("Switch to English") : qsTr("切换到中文")
                onClicked: Theme.toggleLanguage()
                
                // 语言标签
                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 2
                    anchors.bottomMargin: 2
                    width: 14
                    height: 10
                    radius: 3
                    color: Theme.colors.primary
                    
                    Text {
                        anchors.centerIn: parent
                        text: Theme.language === "zh_CN" ? qsTr("中") : qsTr("En")
                        color: Theme.colors.textOnPrimary
                        font.pixelSize: 7
                        font.weight: Font.Bold
                    }
                }
            }
            
            // 设置按钮
            IconButton {
                iconName: "settings"
                iconSize: 16
                tooltip: qsTr("设置")
                onClicked: root.navigateToSettings()
            }
            
            // ========== 分隔线 ==========
            Rectangle {
                width: 1
                height: 20
                color: Theme.colors.border
                Layout.alignment: Qt.AlignVCenter
            }
            
            // ========== 窗口控制按钮 ==========
            // 最小化
            IconButton {
                iconName: "minus"
                iconSize: 14
                btnSize: 32
                tooltip: qsTr("最小化")
                onClicked: WindowHelper.minimize()
            }
            
            // 最大化/还原
            IconButton {
                iconName: WindowHelper.maximized ? "copy" : "square"
                iconSize: 13
                btnSize: 32
                tooltip: WindowHelper.maximized ? qsTr("还原") : qsTr("最大化")
                onClicked: WindowHelper.toggleMaximize()
            }
            
            // 关闭按钮 - 特殊红色悬浮效果
            IconButton {
                id: closeBtn
                iconName: "x"
                iconSize: 14
                btnSize: 32
                danger: true
                tooltip: qsTr("关闭")
                onClicked: WindowHelper.close()
            }
        }
    }
    
    // ========== 颜色过渡动画 ==========
    Behavior on color {
        ColorAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
    }
}
