import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 侧边栏组件
 * 
 * 包含会话管理功能，参考功能设计文档 0.2 节 3.2 会话管理模块
 */
Rectangle {
    id: root
    
    // ========== 属性定义 ==========
    /**
     * @brief 当前活动会话 ID
     */
    property string activeSessionId: ""
    
    /**
     * @brief 会话列表数据模型
     */
    property var sessionModel: null
    
    /**
     * @brief 是否展开侧边栏
     */
    property bool expanded: true
    
    // ========== 视觉属性 ==========
    color: Theme.colors.sidebar
    
    // 右侧分隔线
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.colors.sidebarBorder
    }
    
    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: expanded ? 12 : 8
        anchors.rightMargin: expanded ? 12 : 8
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 8
        
        // ========== 顶部：标题和新建按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            // 标题（仅展开时显示）
            Text {
                visible: expanded
                text: qsTr("会话")
                color: Theme.colors.sidebarForeground
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.letterSpacing: 1.0
                opacity: 0.6
                Layout.alignment: Qt.AlignVCenter
            }
            
            Item { Layout.fillWidth: true }
            
            // 新建会话按钮
            IconButton {
                iconName: "plus"
                iconSize: 14
                btnSize: 28
                tooltip: qsTr("新开会话")
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    console.log("创建新会话")
                }
            }
        }
        
        // ========== 会话列表 ==========
        ListView {
            id: sessionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            
            // 临时会话数据（实际应该使用 C++ 模型）
            model: ListModel {
                ListElement { sessionId: "session1"; name: "未命名会话 1"; modifiedAt: "10:30"; fileCount: 3 }
                ListElement { sessionId: "session2"; name: "未命名会话 2"; modifiedAt: "09:15"; fileCount: 1 }
                ListElement { sessionId: "session3"; name: "风景照片处理"; modifiedAt: "昨天"; fileCount: 5 }
            }
            
            // 会话项委托
            delegate: Rectangle {
                id: sessionItem
                width: sessionList.width
                height: expanded ? 52 : 40
                radius: Theme.radius.md
                
                property bool isActive: model.sessionId === root.activeSessionId
                property bool isHovered: mouseArea.containsMouse
                
                color: {
                    if (isActive) return Theme.colors.primary
                    if (isHovered) return Theme.colors.sidebarAccent
                    return "transparent"
                }
                
                Behavior on color {
                    ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
                }
                
                // 鼠标区域
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.activeSessionId = model.sessionId
                        console.log("切换到会话:", model.name)
                    }
                }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 10
                    
                    // 会话图标
                    Rectangle {
                        width: expanded ? 32 : 28
                        height: expanded ? 32 : 28
                        radius: Theme.radius.sm
                        color: sessionItem.isActive ? Qt.rgba(1,1,1,0.2) : Theme.colors.accent
                        Layout.alignment: Qt.AlignVCenter
                        
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("message-square")
                            iconSize: 14
                            color: sessionItem.isActive ? "#FFFFFF" : Theme.colors.icon
                        }
                    }
                    
                    // 会话信息（仅展开时显示）
                    ColumnLayout {
                        visible: expanded
                        Layout.fillWidth: true
                        spacing: 2
                        
                        Text {
                            text: model.name
                            color: sessionItem.isActive ? "#FFFFFF" : Theme.colors.sidebarForeground
                            font.pixelSize: 13
                            font.weight: sessionItem.isActive ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        
                        RowLayout {
                            spacing: 6
                            Text {
                                text: model.modifiedAt
                                color: sessionItem.isActive ? Qt.rgba(1,1,1,0.7) : Theme.colors.mutedForeground
                                font.pixelSize: 11
                            }
                            Rectangle {
                                width: 3
                                height: 3
                                radius: 1.5
                                color: sessionItem.isActive ? Qt.rgba(1,1,1,0.4) : Theme.colors.mutedForeground
                                opacity: 0.5
                            }
                            Text {
                                text: model.fileCount + (Theme.language === "zh_CN" ? " 个文件" : " files")
                                color: sessionItem.isActive ? Qt.rgba(1,1,1,0.7) : Theme.colors.mutedForeground
                                font.pixelSize: 11
                            }
                        }
                    }
                    
                    // 删除按钮（仅展开且悬停时显示）
                    IconButton {
                        visible: expanded && sessionItem.isHovered && !sessionItem.isActive
                        iconName: "x"
                        iconSize: 12
                        btnSize: 24
                        danger: true
                        tooltip: qsTr("删除会话")
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            console.log("删除会话:", model.name)
                        }
                    }
                }
            }
            
            // 空状态提示
            Item {
                visible: sessionList.count === 0
                anchors.fill: parent
                
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    
                    // 空状态图标
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 48
                        height: 48
                        radius: 24
                        color: Theme.colors.accent
                        
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("message-square")
                            iconSize: 20
                            color: Theme.colors.icon
                        }
                    }
                    
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("暂无会话")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 13
                    }
                    
                    Text {
                        visible: expanded
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("点击 + 创建新会话")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 11
                        opacity: 0.7
                    }
                }
            }
        }
        
    }
    
    // ========== 颜色过渡动画 ==========
    Behavior on color {
        ColorAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
    }
}
