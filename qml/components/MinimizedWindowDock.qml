import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 最小化窗口停靠区
 *
 * 显示在消息展示区域底部，用于存放最小化的内嵌窗口。
 * 点击可恢复对应窗口。
 */
Rectangle {
    id: root
    
    // 最小化窗口列表
    property ListModel minimizedWindows: ListModel {}
    property string currentSessionId: ""  // 当前会话 ID，用于过滤标签
    property bool showGlobalOnly: false    // 是否只显示全局标签（待处理查看器）
    
    // 信号
    signal restoreWindow(string viewerId)
    signal closeWindow(string viewerId)
    
    height: minimizedWindows.count > 0 ? 48 : 0
    color: Theme.colors.card
    visible: minimizedWindows.count > 0
    
    Behavior on height {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }
    
    // 顶部边框
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.colors.border
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8
        
        // 标签
        Text {
            text: qsTr("最小化窗口")
            color: Theme.colors.mutedForeground
            font.pixelSize: 11
        }
        
        Rectangle {
            width: 1
            height: 20
            color: Theme.colors.border
        }
        
        // 最小化窗口列表
        ListView {
            id: windowList
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            orientation: ListView.Horizontal
            spacing: 8
            clip: true
            
            model: root.minimizedWindows
            
            delegate: Rectangle {
                id: windowItem
                required property int index
                required property string viewerId
                required property string title
                required property string thumbnail
                
                width: itemContent.implicitWidth + 24
                height: 28
                radius: 6
                color: itemMouse.containsMouse ? Theme.colors.accent : Theme.colors.muted
                
                Behavior on color { ColorAnimation { duration: 100 } }
                
                // 底层点击区域
                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    
                    onClicked: function(mouse) {
                        // 检查是否点击在关闭按钮区域
                        if (closeBtn.visible && mouse.x >= closeBtn.x && mouse.x <= closeBtn.x + closeBtn.width &&
                            mouse.y >= closeBtn.y && mouse.y <= closeBtn.y + closeBtn.height) {
                            return
                        }
                        root.restoreWindow(windowItem.viewerId)
                    }
                }
                
                RowLayout {
                    id: itemContent
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8
                    
                    // 缩略图
                    Rectangle {
                        width: 24
                        height: 24
                        radius: 4
                        color: Theme.colors.card
                        clip: true
                        Layout.alignment: Qt.AlignVCenter
                        
                        Image {
                            anchors.fill: parent
                            source: windowItem.thumbnail
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: status === Image.Ready
                        }
                        
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("image")
                            iconSize: 14
                            color: Theme.colors.mutedForeground
                            visible: parent.children[0].status !== Image.Ready
                        }
                    }
                    
                    // 标题
                    Text {
                        Layout.alignment: Qt.AlignVCenter
                        text: windowItem.title.length > 20 ? windowItem.title.substring(0, 20) + "..." : windowItem.title
                        color: Theme.colors.foreground
                        font.pixelSize: 12
                    }
                    
                    // 关闭按钮
                    Rectangle {
                        id: closeBtn
                        width: 18
                        height: 18
                        radius: 9
                        Layout.alignment: Qt.AlignVCenter
                        color: closeBtnMouse.containsMouse ? Theme.colors.destructive : Theme.colors.card
                        visible: itemMouse.containsMouse || closeBtnMouse.containsMouse
                        z: 10
                        
                        Behavior on color { ColorAnimation { duration: 100 } }
                        
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("x")
                            iconSize: 10
                            color: closeBtnMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                        }
                        
                        MouseArea {
                            id: closeBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            z: 100
                            onClicked: function(mouse) {
                                mouse.accepted = true
                                root.closeWindow(windowItem.viewerId)
                            }
                        }
                    }
                }
                
                ToolTip {
                    visible: itemMouse.containsMouse && !closeBtnMouse.containsMouse
                    text: {
                        if (windowItem.viewerId === "pending-viewer") {
                            return qsTr("待处理文件 - ") + windowItem.title
                        } else if (windowItem.viewerId === "message-viewer") {
                            return qsTr("消息文件 - ") + windowItem.title
                        }
                        return windowItem.title
                    }
                    delay: 500
                }
            }
        }
        
        // 全部恢复按钮
        Rectangle {
            visible: root.minimizedWindows.count > 1
            width: restoreAllRow.implicitWidth + 16
            height: 28
            radius: 6
            color: restoreAllMouse.containsMouse ? Theme.colors.primarySubtle : Theme.colors.muted
            
            Row {
                id: restoreAllRow
                anchors.centerIn: parent
                spacing: 4
                
                ColoredIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    source: Theme.icon("maximize")
                    iconSize: 12
                    color: Theme.colors.foreground
                }
                
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("全部恢复")
                    color: Theme.colors.foreground
                    font.pixelSize: 11
                }
            }
            
            MouseArea {
                id: restoreAllMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    // 恢复所有窗口
                    for (var i = root.minimizedWindows.count - 1; i >= 0; i--) {
                        root.restoreWindow(root.minimizedWindows.get(i).viewerId)
                    }
                }
            }
        }
    }
    
    // ========== 公共方法 ==========
    
    function addWindow(viewerId, title, thumbnail) {
        // 检查是否已存在
        for (var i = 0; i < minimizedWindows.count; i++) {
            if (minimizedWindows.get(i).viewerId === viewerId) {
                // 更新现有项
                minimizedWindows.set(i, { viewerId: viewerId, title: title, thumbnail: thumbnail })
                return
            }
        }
        // 添加新项
        minimizedWindows.append({ viewerId: viewerId, title: title, thumbnail: thumbnail })
    }
    
    function removeWindow(viewerId) {
        for (var i = 0; i < minimizedWindows.count; i++) {
            if (minimizedWindows.get(i).viewerId === viewerId) {
                minimizedWindows.remove(i)
                return
            }
        }
    }
    
    /** @brief 清理所有消息查看器标签（保留待处理查看器标签） */
    function clearMessageViewerWindows() {
        for (var i = minimizedWindows.count - 1; i >= 0; i--) {
            var viewerId = minimizedWindows.get(i).viewerId
            if (viewerId === "message-viewer") {
                minimizedWindows.remove(i)
            }
        }
    }
    
    function clear() {
        minimizedWindows.clear()
    }
}
