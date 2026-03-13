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
    
    /**
     * @brief 当前语言
     */
    property string currentLanguage: "zh_CN"
    
    // ========== 视觉属性 ==========
    color: Theme.colors.card
    height: 48
    
    // ========== 边框和分隔 ==========
    border {
        width: 1
        color: Theme.colors.border
    }
    
    // ========== 内部布局 ==========
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing._2
        anchors.rightMargin: Theme.spacing._2
        anchors.topMargin: Theme.spacing._1
        anchors.bottomMargin: Theme.spacing._1
        spacing: Theme.spacing._2
        
        // ========== 左侧：应用图标和标题 ==========
        RowLayout {
            spacing: Theme.spacing._2
            
            // 应用图标
            Rectangle {
                width: 32
                height: 32
                radius: Theme.radius.sm
                color: Theme.colors.primary
                
                Text {
                    anchors.centerIn: parent
                    text: "E"
                    color: Theme.colors.primaryForeground
                    font.pixelSize: 18
                    font.bold: true
                }
            }
            
            // 应用标题
            Text {
                text: qsTr("EnhanceVision")
                color: Theme.colors.foreground
                font.pixelSize: 16
                font.bold: true
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
        
        // ========== 右侧：功能按钮 ==========
        RowLayout {
            spacing: Theme.spacing._1
            
            // 新开会话按钮
            IconButton {
                iconSource: "+"
                tooltip: qsTr("新开会话 (Ctrl+N)")
                onClicked: {
                    // TODO: 调用 C++ 控制器创建新会话
                    console.log("创建新会话")
                }
            }
            
            // 收缩侧边栏按钮
            IconButton {
                iconSource: "⇤"
                tooltip: qsTr("收缩侧边栏")
                onClicked: {
                    if (parent && parent.parent && parent.parent.sidebarExpanded !== undefined) {
                        parent.parent.sidebarExpanded = !parent.parent.sidebarExpanded
                    }
                }
            }
            
            // 主题切换按钮
            IconButton {
                id: themeButton
                iconSource: Theme.isDark ? "☀️" : "🌙"
                tooltip: Theme.isDark ? qsTr("切换到亮色主题") : qsTr("切换到暗色主题")
                onClicked: {
                    Theme.toggle()
                }
            }
            
            // 语言切换按钮
            IconButton {
                id: languageButton
                iconSource: currentLanguage === "zh_CN" ? "EN" : "中"
                tooltip: currentLanguage === "zh_CN" ? qsTr("切换到英文") : qsTr("切换到中文")
                onClicked: {
                    currentLanguage = currentLanguage === "zh_CN" ? "en_US" : "zh_CN"
                    // TODO: 调用 C++ 控制器切换语言
                    console.log("切换语言到:", currentLanguage)
                }
            }
            
            // 设置按钮
            IconButton {
                iconSource: "⚙"
                tooltip: qsTr("设置")
                onClicked: {
                    if (parent && parent.parent && parent.parent.currentPage !== undefined) {
                        parent.parent.currentPage = parent.parent.currentPage === 0 ? 1 : 0
                    }
                }
            }
            
            // 分隔线
            Rectangle {
                width: 1
                height: 24
                color: Theme.colors.border
            }
            
            // 最小化按钮
            IconButton {
                iconSource: "−"
                tooltip: qsTr("最小化")
                onClicked: {
                    if (parentWindow) {
                        parentWindow.showMinimized()
                    }
                }
            }
            
            // 最大化/还原按钮
            IconButton {
                id: maximizeButton
                iconSource: (parentWindow && parentWindow.visibility === Window.Maximized) ? "❐" : "□"
                tooltip: (parentWindow && parentWindow.visibility === Window.Maximized) ? qsTr("还原") : qsTr("最大化")
                onClicked: {
                    toggleMaximize()
                }
            }
            
            // 关闭按钮
            IconButton {
                iconSource: "×"
                tooltip: qsTr("关闭")
                onClicked: {
                    if (parentWindow) {
                        parentWindow.close()
                    }
                }
                
                // 关闭按钮特殊样式
                background: Rectangle {
                    color: root.hovered ? Theme.colors.destructive : "transparent"
                    radius: Theme.radius.sm
                }
                
                contentItem: Text {
                    text: "×"
                    color: Theme.colors.foreground
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 20
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
}
