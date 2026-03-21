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
    property bool isAnyEditing: false

    signal sessionClicked(string sessionId)
    signal sessionDoubleClicked(string sessionId)
    signal createSessionRequested()
    signal renameRequested(string sessionId, string currentName)
    signal pinRequested(string sessionId, bool pinned)
    signal clearRequested(string sessionId)
    signal deleteRequested(string sessionId)
    signal selectionChanged(string sessionId, bool selected)
    signal moveRequested(int fromIndex, int toIndex)

    // 全局点击处理器 - 用于退出编辑模式
    MouseArea {
        id: globalClickHandler
        anchors.fill: parent
        enabled: root.isAnyEditing
        z: 999
        onClicked: {
            root.isAnyEditing = false
            sessionListView.forceActiveFocus()
        }
    }

    ListView {
        id: sessionListView
        anchors.fill: parent
        clip: true
        spacing: 4
        model: root.sessionModel
        cacheBuffer: 1000

        property int dragFromIndex: -1
        property int dragToIndex: -1
        property bool isDragging: false
        property real dragMouseY: 0

        displaced: Transition {
            NumberAnimation {
                properties: "y"
                duration: 250
                easing.type: Easing.OutCubic
            }
        }

        add: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        remove: Transition {
            NumberAnimation {
                property: "opacity"
                to: 0
                duration: 100
                easing.type: Easing.InCubic
            }
        }

        move: Transition {
            NumberAnimation {
                properties: "y"
                duration: 250
                easing.type: Easing.OutCubic
            }
        }

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
            property bool isEditing: root.isAnyEditing && sessionListView.currentIndex === index
            
            // 当进入编辑模式时，更新全局状态
            function startEditing() {
                sessionListView.currentIndex = index
                root.isAnyEditing = true
            }
            
            // 监听全局编辑状态变化
            Connections {
                target: root
                function onIsAnyEditingChanged() {
                    if (!root.isAnyEditing && delegateRoot.isEditing) {
                        // 如果全局退出编辑，当前项也退出
                        if (nameEditField.text.trim() !== "" && nameEditField.text.trim() !== model.name) {
                            root.renameRequested(model.id, nameEditField.text.trim())
                        }
                    }
                }
            }

            scale: delegateRoot.isBeingDragged ? 1.02 : 1.0
            
            z: delegateRoot.isBeingDragged ? 100 : (delegateRoot.isActive ? 10 : 1)

            Behavior on scale {
                NumberAnimation { 
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }

            Rectangle {
                id: sessionItemBg
                anchors.fill: parent
                radius: Theme.radius.md
                
                color: {
                    if (delegateRoot.isActive) return Theme.colors.primary
                    if (delegateRoot.isBeingDragged) return Qt.lighter(Theme.colors.accent, 1.1)
                    if (delegateRoot.isDragTarget) return Theme.colors.primarySubtle
                    if (delegateRoot.isHovered) return Theme.colors.sidebarAccent
                    return "transparent"
                }
                
                border.width: delegateRoot.isBeingDragged ? 2 : 0
                border.color: Theme.colors.primary

                Rectangle {
                    visible: model.isPinned && !delegateRoot.isActive
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    radius: sessionItemBg.radius
                    color: Theme.colors.primary
                    opacity: 0.8
                }

                Behavior on border.width {
                    NumberAnimation { duration: 100 }
                }
            }

            Rectangle {
                id: dropIndicator
                visible: delegateRoot.isDragTarget
                anchors.fill: parent
                radius: sessionItemBg.radius
                color: "transparent"
                z: 10

                border.width: 2
                border.color: Theme.colors.primary

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: parent.radius + 4
                    color: "transparent"
                    border.width: 1
                    border.color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, 
                                          Theme.colors.primary.b, 0.3)
                }

                SequentialAnimation on border.color {
                    running: delegateRoot.isDragTarget
                    loops: Animation.Infinite
                    ColorAnimation { 
                        to: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, 
                                   Theme.colors.primary.b, 0.5)
                        duration: 500
                        easing.type: Easing.OutCubic 
                    }
                    ColorAnimation { 
                        to: Theme.colors.primary
                        duration: 500
                        easing.type: Easing.InCubic 
                    }
                }
            }

            MouseArea {
                id: itemMouseArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                drag.target: root.expanded && !root.batchMode ? dragHandle : null
                drag.axis: Drag.YAxis
                drag.smoothed: false
                drag.threshold: 10

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
                        delegateRoot.startEditing()
                        nameEditField.text = model.name
                        nameEditField.forceActiveFocus()
                        nameEditField.selectAll()
                    }
                }

                onPressed: function(mouse) {
                    if (!root.batchMode && root.expanded) {
                        sessionListView.dragFromIndex = index
                        sessionListView.dragMouseY = mouseY
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

                onPositionChanged: function(mouse) {
                    if (pressed && sessionListView.dragFromIndex !== -1 && !root.batchMode) {
                        var dragDistance = Math.abs(mouseY - sessionListView.dragMouseY)
                        if (dragDistance > 10) {
                            sessionListView.isDragging = true
                            var globalY = mapToItem(sessionListView, 0, mouseY).y
                            var targetIndex = sessionListView.indexAt(sessionListView.width / 2, globalY)
                            if (targetIndex >= 0 && targetIndex < sessionListView.count && 
                                targetIndex !== sessionListView.dragFromIndex) {
                                sessionListView.dragToIndex = targetIndex
                            }
                        }
                    }
                }
            }

            Item {
                id: dragHandle
                anchors.fill: parent
                Drag.active: sessionListView.isDragging && sessionListView.dragFromIndex === index
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 10

                Rectangle {
                    visible: root.batchMode
                    width: 20
                    height: 20
                    radius: 5
                    color: model.isSelected ? Theme.colors.primary : "transparent"
                    border.width: model.isSelected ? 0 : 2
                    border.color: Theme.colors.border
                    Layout.alignment: Qt.AlignVCenter

                    ColoredIcon {
                        anchors.centerIn: parent
                        visible: model.isSelected
                        source: Theme.icon("check")
                        iconSize: 14
                        color: "#FFFFFF"
                    }
                }

                Rectangle {
                    visible: !root.batchMode
                    width: root.expanded ? 32 : 28
                    height: root.expanded ? 32 : 28
                    radius: Theme.radius.sm
                    color: delegateRoot.isActive ? Qt.rgba(1,1,1,0.2) : Theme.colors.accent
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        id: processingIndicator
                        visible: model.isProcessing && !root.batchMode
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.topMargin: -2
                        anchors.rightMargin: -2
                        width: 8
                        height: 8
                        radius: 4
                        color: "#F59E0B"

                        SequentialAnimation on opacity {
                            running: processingIndicator.visible
                            loops: Animation.Infinite
                            NumberAnimation { from: 0.5; to: 1.0; duration: 800 }
                            NumberAnimation { from: 1.0; to: 0.5; duration: 800 }
                        }
                    }

                    ColoredIcon {
                        visible: model.isPinned
                        anchors.centerIn: parent
                        source: Theme.icon("pin")
                        iconSize: 14
                        color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.primary
                    }

                    ColoredIcon {
                        visible: !model.isPinned
                        anchors.centerIn: parent
                        source: Theme.icon("message-square")
                        iconSize: 14
                        color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.icon
                    }
                }

                ColumnLayout {
                    visible: root.expanded && !delegateRoot.isEditing
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: model.name
                        color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.sidebarForeground
                        font.pixelSize: 13
                        font.weight: delegateRoot.isActive ? Font.DemiBold : (model.isPinned ? Font.Medium : Font.Normal)
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        spacing: 6
                        
                        Rectangle {
                            visible: model.isPinned
                            width: pinnedLabel.implicitWidth + 8
                            height: 14
                            radius: 3
                            color: delegateRoot.isActive ? Qt.rgba(1,1,1,0.2) : Theme.colors.primarySubtle
                            
                            Text {
                                id: pinnedLabel
                                anchors.centerIn: parent
                                text: qsTr("置顶")
                                color: delegateRoot.isActive ? "#FFFFFF" : Theme.colors.primary
                                font.pixelSize: 9
                                font.weight: Font.Medium
                            }
                        }
                        
                        Text {
                            text: model.messageCount + (Theme.language === "zh_CN" ? " 条消息" : " messages")
                            color: delegateRoot.isActive ? Qt.rgba(1,1,1,0.8) : Theme.colors.mutedForeground
                            font.pixelSize: 11
                        }
                    }
                }

                TextField {
                    id: nameEditField
                    visible: delegateRoot.isEditing && root.expanded
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    font.pixelSize: 13
                    selectByMouse: true
                    
                    background: Rectangle {
                        radius: Theme.radius.sm
                        color: Theme.colors.inputBackground
                        border.color: Theme.colors.primary
                        border.width: 1.5
                    }

                    onVisibleChanged: {
                        if (visible) {
                            forceActiveFocus()
                            selectAll()
                        }
                    }

                    onActiveFocusChanged: {
                        if (!activeFocus && root.isAnyEditing) {
                            if (text.trim() !== "" && text.trim() !== model.name) {
                                root.renameRequested(model.id, text.trim())
                            }
                            root.isAnyEditing = false
                        }
                    }

                    onAccepted: {
                        if (text.trim() !== "" && text.trim() !== model.name) {
                            root.renameRequested(model.id, text.trim())
                        }
                        root.isAnyEditing = false
                    }

                    Keys.onEscapePressed: {
                        root.isAnyEditing = false
                    }

                    Keys.onBackPressed: {
                        root.isAnyEditing = false
                    }
                }

                IconButton {
                    id: moreButton
                    visible: root.expanded && delegateRoot.isHovered && !root.batchMode && !delegateRoot.isEditing
                    iconName: "more-vertical"
                    iconSize: 16
                    btnSize: 28
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
                padding: 6
                modal: false
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                
                function showAt(globalX, globalY) {
                    var menuWidth = 150
                    var menuHeight = 180
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
                    implicitWidth: 150
                    color: Theme.colors.popover
                    border.width: 1
                    border.color: Theme.colors.border
                    radius: Theme.radius.lg
                    
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -10
                        color: "transparent"
                        border.width: 10
                        border.color: "transparent"
                        z: -1
                        
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 10
                            radius: Theme.radius.lg + 3
                            color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                            z: -1
                        }
                    }
                }
                
                contentItem: ColumnLayout {
                    spacing: 3
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 3
                        Layout.rightMargin: 3
                        height: 34
                        radius: Theme.radius.sm
                        color: renameMouse.containsMouse ? Theme.colors.primary : "transparent"
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10
                            
                            ColoredIcon {
                                source: Theme.icon("pencil")
                                iconSize: 16
                                color: renameMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                text: qsTr("重命名")
                                color: renameMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                                font.pixelSize: 14
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
                                delegateRoot.startEditing()
                                nameEditField.text = contextMenu.sessionModel.name
                                nameEditField.forceActiveFocus()
                                nameEditField.selectAll()
                            }
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 3
                        Layout.rightMargin: 3
                        height: 34
                        radius: Theme.radius.sm
                        color: pinMouse.containsMouse ? Theme.colors.primary : "transparent"
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10
                            
                            ColoredIcon {
                                source: Theme.icon("pin")
                                iconSize: 16
                                color: pinMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                text: contextMenu.sessionModel && contextMenu.sessionModel.isPinned ? qsTr("取消置顶") : qsTr("置顶")
                                color: pinMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                                font.pixelSize: 14
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
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        height: 1
                        color: Theme.colors.border
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 3
                        Layout.rightMargin: 3
                        height: 34
                        radius: Theme.radius.sm
                        color: clearMouse.containsMouse && contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? Theme.colors.primary : "transparent"
                        opacity: contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? 1.0 : 0.5
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10
                            
                            ColoredIcon {
                                source: Theme.icon("eraser")
                                iconSize: 16
                                color: clearMouse.containsMouse && contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? "#FFFFFF" : "#F59E0B"
                            }
                            
                            Text {
                                text: qsTr("清空会话")
                                color: clearMouse.containsMouse && contextMenu.sessionModel && contextMenu.sessionModel.messageCount > 0 ? "#FFFFFF" : Theme.colors.foreground
                                font.pixelSize: 14
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
                        Layout.leftMargin: 3
                        Layout.rightMargin: 3
                        height: 34
                        radius: Theme.radius.sm
                        color: deleteMouse.containsMouse ? Theme.colors.destructive : "transparent"
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10
                            
                            ColoredIcon {
                                source: Theme.icon("trash-2")
                                iconSize: 16
                                color: deleteMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                            }
                            
                            Text {
                                text: qsTr("删除会话")
                                color: deleteMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                                font.pixelSize: 14
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
                spacing: 16

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 56
                    height: 56
                    radius: 28
                    color: Theme.colors.accent

                    ColoredIcon {
                        anchors.centerIn: parent
                        source: Theme.icon("message-square")
                        iconSize: 24
                        color: Theme.colors.icon
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("暂无会话")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 14
                }

                Text {
                    visible: root.expanded
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("点击 + 创建新会话")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 12
                    opacity: 0.7
                }
            }
        }
    }
}
