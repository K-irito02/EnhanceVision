import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

Item {
    id: root

    property var sessionModel: null
    property string activeSessionId: ""
    property bool expanded: true
    property bool batchMode: false

    signal sessionClicked(string sessionId)
    signal sessionDoubleClicked(string sessionId)
    signal createSessionRequested()
    signal renameRequested(string sessionId, string currentName)
    signal pinRequested(string sessionId, bool pinned)
    signal clearRequested(string sessionId)
    signal deleteRequested(string sessionId)
    signal selectionChanged(string sessionId, bool selected)
    signal moveRequested(int fromIndex, int toIndex)

    ListView {
        id: sessionListView
        anchors.fill: parent
        clip: true
        spacing: 2
        model: root.sessionModel

        property int dragFromIndex: -1
        property int dragToIndex: -1
        property bool isDragging: false

        delegate: Item {
            id: delegateRoot
            width: sessionListView.width
            height: root.expanded ? 48 : 36

            property bool isActive: model.id === root.activeSessionId
            property bool isHovered: itemMouseArea.containsMouse || moreButton.hovered
            property bool isDragTarget: sessionListView.isDragging && 
                                        sessionListView.dragToIndex === index &&
                                        sessionListView.dragFromIndex !== index
            property bool isBeingDragged: sessionListView.isDragging && 
                                          sessionListView.dragFromIndex === index
            property bool isEditing: false

            Rectangle {
                id: sessionItemBg
                anchors.fill: parent
                radius: Theme.radius.md
                
                color: {
                    if (delegateRoot.isActive) return Theme.colors.primary
                    if (delegateRoot.isDragTarget) return Theme.colors.primarySubtle
                    if (delegateRoot.isHovered) return Theme.colors.sidebarAccent
                    return "transparent"
                }
                
                opacity: delegateRoot.isBeingDragged ? 0.5 : 1.0
                
                Rectangle {
                    visible: delegateRoot.isDragTarget
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 2
                    color: Theme.colors.primary
                    radius: 1
                }

                Behavior on color {
                    ColorAnimation { duration: Theme.animation.fast; easing.type: Easing.OutCubic }
                }
            }

            MouseArea {
                id: itemMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                drag.target: root.expanded && !root.batchMode ? dragHandle : null
                drag.axis: Drag.YAxis

                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        contextMenu.sessionModel = model
                        var globalPos = mapToGlobal(mouse.x, mouse.y)
                        contextMenu.showAt(globalPos.x, globalPos.y)
                    } else if (!delegateRoot.isEditing) {
                        if (root.batchMode) {
                            root.selectionChanged(model.id, !model.isSelected)
                        } else {
                            root.sessionClicked(model.id)
                        }
                    }
                }

                onDoubleClicked: {
                    if (!root.batchMode && root.expanded) {
                        delegateRoot.isEditing = true
                        nameEditField.text = model.name
                        nameEditField.forceActiveFocus()
                        nameEditField.selectAll()
                    }
                }

                onPressed: {
                    if (!root.batchMode && root.expanded) {
                        sessionListView.dragFromIndex = index
                    }
                }

                onReleased: {
                    if (sessionListView.isDragging && sessionListView.dragToIndex !== -1 &&
                        sessionListView.dragFromIndex !== sessionListView.dragToIndex) {
                        root.moveRequested(sessionListView.dragFromIndex, sessionListView.dragToIndex)
                    }
                    sessionListView.isDragging = false
                    sessionListView.dragFromIndex = -1
                    sessionListView.dragToIndex = -1
                }

                onPositionChanged: {
                    if (pressed && sessionListView.dragFromIndex !== -1 && !root.batchMode) {
                        sessionListView.isDragging = true
                        var targetIndex = sessionListView.indexAt(mouseX, y + mouseY)
                        if (targetIndex >= 0 && targetIndex < sessionListView.count) {
                            sessionListView.dragToIndex = targetIndex
                        }
                    }
                }
            }

            Item {
                id: dragHandle
                anchors.fill: parent
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 6
                spacing: 8

                Rectangle {
                    visible: root.batchMode
                    width: 18
                    height: 18
                    radius: 4
                    color: model.isSelected ? Theme.colors.primary : "transparent"
                    border.width: model.isSelected ? 0 : 2
                    border.color: Theme.colors.border
                    Layout.alignment: Qt.AlignVCenter

                    ColoredIcon {
                        anchors.centerIn: parent
                        visible: model.isSelected
                        source: Theme.icon("check")
                        iconSize: 12
                        color: "#FFFFFF"
                    }
                }

                Rectangle {
                    visible: !root.batchMode
                    width: root.expanded ? 28 : 24
                    height: root.expanded ? 28 : 24
                    radius: Theme.radius.sm
                    color: delegateRoot.isActive ? Qt.rgba(1,1,1,0.2) : Theme.colors.accent
                    Layout.alignment: Qt.AlignVCenter

                    ColoredIcon {
                        visible: model.isPinned
                        anchors.centerIn: parent
                        source: Theme.icon("pin")
                        iconSize: 12
                        color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.primary
                    }

                    ColoredIcon {
                        visible: !model.isPinned
                        anchors.centerIn: parent
                        source: Theme.icon("message-square")
                        iconSize: 12
                        color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.icon
                    }
                }

                ColumnLayout {
                    visible: root.expanded && !delegateRoot.isEditing
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: model.name
                        color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.sidebarForeground
                        font.pixelSize: 12
                        font.weight: delegateRoot.isActive ? Font.DemiBold : Font.Normal
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 4
                        
                        Rectangle {
                            visible: model.isPinned
                            width: pinnedLabel.implicitWidth + 6
                            height: 12
                            radius: 2
                            color: Theme.colors.primarySubtle
                            
                            Text {
                                id: pinnedLabel
                                anchors.centerIn: parent
                                text: qsTr("置顶")
                                color: Theme.colors.primary
                                font.pixelSize: 8
                            }
                        }
                        
                        Text {
                            text: model.messageCount + (Theme.language === "zh_CN" ? " 条消息" : " messages")
                            color: delegateRoot.isActive ? Qt.rgba(1,1,1,0.7) : Theme.colors.mutedForeground
                            font.pixelSize: 10
                        }
                    }
                }

                TextField {
                    id: nameEditField
                    visible: delegateRoot.isEditing && root.expanded
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    font.pixelSize: 12
                    selectByMouse: true
                    
                    background: Rectangle {
                        radius: Theme.radius.sm
                        color: Theme.colors.inputBackground
                        border.color: Theme.colors.primary
                        border.width: 1
                    }

                    onAccepted: {
                        if (text.trim() !== "" && text.trim() !== model.name) {
                            root.renameRequested(model.id, text.trim())
                        }
                        delegateRoot.isEditing = false
                    }

                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            delegateRoot.isEditing = false
                        }
                    }

                    Keys.onEscapePressed: {
                        delegateRoot.isEditing = false
                    }
                }

                IconButton {
                    id: moreButton
                    visible: root.expanded && delegateRoot.isHovered && !root.batchMode && !delegateRoot.isEditing
                    iconName: "more-vertical"
                    iconSize: 14
                    btnSize: 24
                    tooltip: qsTr("更多操作")
                    Layout.alignment: Qt.AlignVCenter
                    iconColor: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.icon
                    
                    onClicked: {
                        contextMenu.sessionModel = model
                        var globalPos = mapToGlobal(0, height)
                        contextMenu.showAt(globalPos.x, globalPos.y)
                    }
                }
            }

            Popup {
                id: contextMenu
                property var sessionModel: null
                
                parent: Overlay.overlay
                padding: 4
                modal: false
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                
                function showAt(globalX, globalY) {
                    var menuWidth = 140
                    var menuHeight = 170
                    var margin = 8
                    var offsetX = 4
                    var offsetY = 4
                    
                    var overlayPos = parent.mapFromGlobal(globalX, globalY)
                    
                    var finalX = overlayPos.x + offsetX
                    var finalY = overlayPos.y + offsetY
                    
                    if (finalX + menuWidth + margin > parent.width) {
                        finalX = overlayPos.x - menuWidth - offsetX
                    }
                    if (finalY + menuHeight + margin > parent.height) {
                        finalY = overlayPos.y - menuHeight - offsetY
                    }
                    
                    finalX = Math.max(margin, Math.min(finalX, parent.width - menuWidth - margin))
                    finalY = Math.max(margin, Math.min(finalY, parent.height - menuHeight - margin))
                    
                    contextMenu.x = finalX
                    contextMenu.y = finalY
                    contextMenu.open()
                }
                
                background: Rectangle {
                    implicitWidth: 140
                    color: Theme.colors.popover
                    border.width: 1
                    border.color: Theme.colors.border
                    radius: Theme.radius.md
                    
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -8
                        color: "transparent"
                        border.width: 8
                        border.color: "transparent"
                        z: -1
                        
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 8
                            radius: Theme.radius.md + 2
                            color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.5 : 0.15)
                            z: -1
                        }
                    }
                }
                
                contentItem: ColumnLayout {
                    spacing: 2
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        height: 32
                        radius: Theme.radius.xs
                        color: renameMouse.containsMouse ? Theme.colors.primary : "transparent"
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            
                            ColoredIcon {
                                source: Theme.icon("pencil")
                                iconSize: 14
                                color: renameMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                text: qsTr("重命名")
                                color: renameMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            Item { Layout.fillWidth: true }
                        }
                        
                        MouseArea {
                            id: renameMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                contextMenu.close()
                                delegateRoot.isEditing = true
                                nameEditField.text = contextMenu.sessionModel.name
                                nameEditField.forceActiveFocus()
                                nameEditField.selectAll()
                            }
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        height: 32
                        radius: Theme.radius.xs
                        color: pinMouse.containsMouse ? Theme.colors.primary : "transparent"
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            
                            ColoredIcon {
                                source: Theme.icon("pin")
                                iconSize: 14
                                color: pinMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                text: contextMenu.sessionModel && contextMenu.sessionModel.isPinned ? qsTr("取消置顶") : qsTr("置顶")
                                color: pinMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            Item { Layout.fillWidth: true }
                        }
                        
                        MouseArea {
                            id: pinMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (contextMenu.sessionModel) {
                                    root.pinRequested(contextMenu.sessionModel.id, !contextMenu.sessionModel.isPinned)
                                }
                                contextMenu.close()
                            }
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        height: 1
                        color: Theme.colors.border
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        height: 32
                        radius: Theme.radius.xs
                        color: clearMouse.containsMouse && contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? Theme.colors.primary : "transparent"
                        opacity: contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? 1.0 : 0.5
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            
                            ColoredIcon {
                                source: Theme.icon("eraser")
                                iconSize: 14
                                color: clearMouse.containsMouse && contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? "#FFFFFF" : "#F59E0B"
                            }
                            
                            Text {
                                text: qsTr("清空会话")
                                color: clearMouse.containsMouse && contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? "#FFFFFF" : Theme.colors.foreground
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            Item { Layout.fillWidth: true }
                        }
                        
                        MouseArea {
                            id: clearMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                            enabled: contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0
                            onClicked: {
                                if (contextMenu.sessionModel) {
                                    root.clearRequested(contextMenu.sessionModel.id)
                                }
                                contextMenu.close()
                            }
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 2
                        Layout.rightMargin: 2
                        height: 32
                        radius: Theme.radius.xs
                        color: deleteMouse.containsMouse ? Theme.colors.primary : "transparent"
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            
                            ColoredIcon {
                                source: Theme.icon("trash-2")
                                iconSize: 14
                                color: deleteMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                            }
                            
                            Text {
                                text: qsTr("删除会话")
                                color: deleteMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            
                            Item { Layout.fillWidth: true }
                        }
                        
                        MouseArea {
                            id: deleteMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (contextMenu.sessionModel) {
                                    root.deleteRequested(contextMenu.sessionModel.id)
                                }
                                contextMenu.close()
                            }
                        }
                    }
                }
            }
        }

        Item {
            visible: sessionListView.count === 0
            anchors.fill: parent

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

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
                    visible: root.expanded
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
