import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../styles"
import "../controls"

/**
 * @brief 自定义标题栏组件
 * 
 * 包含窗口控制、主题切换、语言切换、设置等功能
 * 参考功能设计文档 0.2 节 3.1 顶部工具栏模块
 */
Rectangle {
    id: root
    
    // ========== 属性定义 ==========
    /**
     * @brief 父窗口引用
     */
    property var parentWindow: null
    
    /**
     * @brief 是否可拖动区域
     */
    property bool isDraggable: true
    
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
            
            // 品牌图标 - 克莱因蓝渐变
            Rectangle {
                width: 28
                height: 28
                radius: 7
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#002FA7" }
                    GradientStop { position: 1.0; color: "#1A56DB" }
                }
                
                Text {
                    anchors.centerIn: parent
                    text: "E"
                    color: "#FFFFFF"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    font.letterSpacing: -0.5
                }
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
        
        // ========== 中间：可拖动区域 ==========
        MouseArea {
            id: dragArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            enabled: isDraggable && parentWindow
            
            property point startMousePos: Qt.point(0, 0)
            property point startWindowPos: Qt.point(0, 0)
            
            onPressed: {
                startMousePos = Qt.point(mouseX, mouseY)
                startWindowPos = Qt.point(parentWindow.x, parentWindow.y)
            }
            
            onPositionChanged: {
                if (pressed && parentWindow) {
                    var dx = mouseX - startMousePos.x
                    var dy = mouseY - startMousePos.y
                    parentWindow.x = startWindowPos.x + dx
                    parentWindow.y = startWindowPos.y + dy
                }
            }
            
            onDoubleClicked: {
                if (parentWindow) {
                    toggleMaximize()
                }
            }
        }
        
        // ========== 左侧：功能按钮组 ==========
        RowLayout {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter
            
            // 新建会话按钮
            IconButton {
                iconName: "plus"
                iconSize: 16
                tooltip: qsTr("新开会话")
                onClicked: root.newSession()
            }
            
            // 侧边栏切换
            IconButton {
                iconName: "panel-left"
                iconSize: 16
                tooltip: qsTr("切换侧边栏")
                onClicked: root.toggleSidebar()
            }
        }
        
        // ========== 右侧：功能按钮组 ==========
        RowLayout {
            spacing: 2
            Layout.alignment: Qt.AlignVCenter
            
            // 浏览模式按钮
            IconButton {
                iconName: "eye"
                iconSize: 16
                tooltip: qsTr("浏览模式")
                onClicked: root.toggleBrowseMode()
            }
            
            // 快捷键说明按钮
            IconButton {
                iconName: "help-circle"
                iconSize: 16
                tooltip: qsTr("快捷键说明")
                onClicked: console.log("Show shortcuts")
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
                        text: Theme.language === "zh_CN" ? "中" : "En"
                        color: "#FFFFFF"
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
                onClicked: {
                    if (parentWindow) parentWindow.showMinimized()
                }
            }
            
            // 最大化/还原
            IconButton {
                iconName: (parentWindow && parentWindow.visibility === Window.Maximized) ? "copy" : "square"
                iconSize: 13
                btnSize: 32
                tooltip: (parentWindow && parentWindow.visibility === Window.Maximized) ? qsTr("还原") : qsTr("最大化")
                onClicked: toggleMaximize()
            }
            
            // 关闭按钮 - 特殊红色悬浮效果
            IconButton {
                id: closeBtn
                iconName: "x"
                iconSize: 14
                btnSize: 32
                danger: true
                tooltip: qsTr("关闭")
                onClicked: {
                    if (parentWindow) parentWindow.close()
                }
            }
        }
    }
    
    // ========== 辅助函数 ==========
    /**
     * @brief 切换最大化/还原状态
     */
    function toggleMaximize() {
        if (parentWindow) {
            if (parentWindow.visibility === Window.Maximized) {
                parentWindow.showNormal()
            } else {
                parentWindow.showMaximized()
            }
        }
    }
    
    // ========== 初始化 ==========
    Component.onCompleted: {
        // 查找父窗口
        var item = parent
        while (item) {
            if (item.window) {
                parentWindow = item.window
                break
            }
            item = item.parent
        }
    }
    
    // ========== 颜色过渡动画 ==========
    Behavior on color {
        ColorAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
    }
}
