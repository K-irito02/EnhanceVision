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
    border {
        width: 1
        color: Theme.colors.sidebarBorder
    }
    
    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: expanded ? Theme.spacing._3 : Theme.spacing._2
        anchors.rightMargin: expanded ? Theme.spacing._3 : Theme.spacing._2
        anchors.topMargin: Theme.spacing._3
        anchors.bottomMargin: Theme.spacing._3
        spacing: Theme.spacing._2
        
        // ========== 顶部：新开会话按钮和标题 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2
            
            // 标题（仅展开时显示）
            Text {
                visible: expanded
                text: qsTr("会话")
                color: Theme.colors.sidebarForeground
                font.pixelSize: 14
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // 新开会话按钮
            IconButton {
                iconSource: "+"
                tooltip: qsTr("新开会话")
                Layout.alignment: Qt.AlignVCenter
                onClicked: {
                    // TODO: 调用 C++ 控制器创建新会话
                    console.log("创建新会话")
                }
            }
        }
        
        // ========== 会话列表 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            radius: Theme.radius.md
            
            ListView {
                id: sessionList
                anchors.fill: parent
                anchors.margins: Theme.spacing._1
                clip: true
                
                // 临时会话数据（实际应该使用 C++ 模型）
                model: ListModel {
                    ListElement { sessionId: "session1"; name: qsTr("未命名会话 1"); modifiedAt: "10:30" }
                    ListElement { sessionId: "session2"; name: qsTr("未命名会话 2"); modifiedAt: "09:15" }
                    ListElement { sessionId: "session3"; name: qsTr("风景照片处理"); modifiedAt: "昨天" }
                }
                
                // 会话项委托
                delegate: Rectangle {
                    id: sessionItem
                    width: sessionList.width
                    height: expanded ? 48 : 48
                    radius: Theme.radius.sm
                    color: model.sessionId === activeSessionId ? Theme.colors.sidebarPrimary : 
                           (mouseArea.containsMouse ? Theme.colors.sidebarAccent : "transparent")
                    
                    // 鼠标区域
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            activeSessionId = model.sessionId
                            // TODO: 切换到该会话
                            console.log("切换到会话:", model.name)
                        }
                    }
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacing._2
                        anchors.rightMargin: Theme.spacing._2
                        spacing: Theme.spacing._2
                        
                        // 会话图标
                        Rectangle {
                            width: 24
                            height: 24
                            radius: Theme.radius.sm
                            color: model.sessionId === activeSessionId ? 
                                   Theme.colors.sidebarPrimaryForeground : 
                                   Theme.colors.sidebarForeground
                            Layout.alignment: Qt.AlignVCenter
                            
                            Text {
                                anchors.centerIn: parent
                                text: model.name.charAt(0).toUpperCase()
                                color: model.sessionId === activeSessionId ? 
                                       Theme.colors.sidebarPrimary : 
                                       Theme.colors.sidebar
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        
                        // 会话信息（仅展开时显示）
                        ColumnLayout {
                            visible: expanded
                            Layout.fillWidth: true
                            spacing: 0
                            
                            Text {
                                text: model.name
                                color: model.sessionId === activeSessionId ? 
                                       Theme.colors.sidebarPrimaryForeground : 
                                       Theme.colors.sidebarForeground
                                font.pixelSize: 13
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            
                            Text {
                                text: model.modifiedAt
                                color: Theme.colors.mutedForeground
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                        }
                        
                        // 删除按钮（仅展开且悬停时显示）
                        IconButton {
                            visible: expanded && mouseArea.containsMouse
                            iconSource: "×"
                            tooltip: qsTr("删除会话")
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: {
                                // TODO: 调用 C++ 控制器删除会话
                                console.log("删除会话:", model.name)
                            }
                        }
                    }
                }
                
                // 空状态提示
                Rectangle {
                    visible: sessionList.count === 0
                    anchors.centerIn: parent
                    color: "transparent"
                    
                    ColumnLayout {
                        spacing: Theme.spacing._2
                        
                        Text {
                            text: qsTr("暂无会话")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                        }
                        
                        Text {
                            text: qsTr("点击 + 创建新会话")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
