import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

Rectangle {
    id: root
    
    property string activeSessionId: sessionController ? sessionController.activeSessionId : ""
    property bool expanded: true
    property bool batchMode: false
    property string pendingDeleteSessionId: ""
    property string pendingClearSessionId: ""
    property bool pendingBatchDelete: false
    property bool pendingBatchClear: false
    property int selectedCount: 0
    property int totalCount: sessionController ? sessionController.sessionCount : 0
    property alias isEditing: sessionList.isAnyEditing
    
    function cancelEditing() {
        sessionList.isAnyEditing = false
    }
    
    Connections {
        target: sessionController
        function onSelectionChanged() {
            root.selectedCount = sessionController.selectedCount()
        }
        function onSessionCountChanged() {
            root.totalCount = sessionController.sessionCount
        }
        function onErrorOccurred(message) {
            NotificationManager.showError(message)
        }
    }
    
    Component.onCompleted: {
        if (sessionController) {
            root.selectedCount = sessionController.selectedCount()
            root.totalCount = sessionController.sessionCount
        }
    }
    
    function selectAll() {
        if (sessionController) sessionController.selectAllSessions()
    }
    
    function deselectAll() {
        if (sessionController) sessionController.deselectAllSessions()
    }
    
    function deleteSelected() {
        root.pendingBatchDelete = true
        NotificationManager.showConfirmDialog(
            qsTr("批量删除会话"),
            qsTr("确定要删除选中的 %1 个会话吗？此操作不可恢复。").arg(root.selectedCount),
            function() {
                if (sessionController) sessionController.deleteSelectedSessions()
                root.pendingBatchDelete = false
                root.batchMode = false
            },
            function() {
                root.pendingBatchDelete = false
            },
            qsTr("删除"),
            qsTr("取消")
        )
    }
    
    function clearSelected() {
        root.pendingBatchClear = true
        NotificationManager.showConfirmDialog(
            qsTr("批量清空会话"),
            qsTr("确定要清空选中的 %1 个会话的消息内容吗？此操作不可恢复。").arg(root.selectedCount),
            function() {
                if (sessionController) sessionController.clearSelectedSessions()
                root.pendingBatchClear = false
                root.batchMode = false
            },
            function() {
                root.pendingBatchClear = false
            },
            qsTr("清空"),
            qsTr("取消")
        )
    }
    
    color: Theme.colors.sidebar
    
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.colors.sidebarBorder
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            
            Rectangle {
                id: batchBtn
                width: 32
                height: 32
                radius: Theme.radius.md
                color: root.batchMode ? Theme.colors.primarySubtle : (batchMouse.containsMouse ? Theme.colors.sidebarAccent : "transparent")
                border.width: 1
                border.color: root.batchMode ? Theme.colors.primary : Theme.colors.border
                
                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon(root.batchMode ? "arrow-left" : "list-select")
                    iconSize: 16
                    color: root.batchMode ? Theme.colors.textOnPrimary : Theme.colors.icon
                }
                
                MouseArea {
                    id: batchMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.batchMode = !root.batchMode
                        if (!root.batchMode && sessionController) {
                            sessionController.deselectAllSessions()
                        }
                    }
                }

                Tooltip {
                    id: batchTooltip
                    text: root.batchMode ? qsTr("取消批量") : qsTr("批量操作")
                }

                Timer {
                    id: batchTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (batchMouse.containsMouse) {
                            batchTooltip.show(batchBtn)
                        }
                    }
                }

                Connections {
                    target: batchMouse
                    function onContainsMouseChanged() {
                        if (batchMouse.containsMouse) {
                            batchTooltipTimer.start()
                        } else {
                            batchTooltipTimer.stop()
                            batchTooltip.close()
                        }
                    }
                }
            }
            
            Rectangle {
                visible: !root.batchMode && root.expanded
                implicitWidth: countText.implicitWidth + 12
                height: 20
                radius: Theme.radius.full
                color: Theme.colors.sidebarAccent
                
                Text {
                    id: countText
                    anchors.centerIn: parent
                    text: root.totalCount.toString()
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: Theme.colors.mutedForeground
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Rectangle {
                id: newBtn
                width: 32
                height: 32
                radius: Theme.radius.md
                color: newMouse.containsMouse ? Theme.colors.sidebarAccent : "transparent"
                border.width: 1
                border.color: Theme.colors.border
                
                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon("plus")
                    iconSize: 16
                    color: Theme.colors.icon
                }
                
                MouseArea {
                    id: newMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (sessionController) {
                            sessionController.createAndSelectSession()
                        }
                    }
                }

                Tooltip {
                    id: newTooltip
                    text: qsTr("新建会话")
                }

                Timer {
                    id: newTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (newMouse.containsMouse) {
                            newTooltip.show(newBtn)
                        }
                    }
                }

                Connections {
                    target: newMouse
                    function onContainsMouseChanged() {
                        if (newMouse.containsMouse) {
                            newTooltipTimer.start()
                        } else {
                            newTooltipTimer.stop()
                            newTooltip.close()
                        }
                    }
                }
            }
        }
        
        RowLayout {
            visible: root.batchMode
            Layout.fillWidth: true
            spacing: 6
            
            Rectangle {
                width: 28
                height: 28
                radius: Theme.radius.sm
                color: selectAllMouse.containsMouse ? Theme.colors.sidebarAccent : "transparent"
                border.width: 1
                border.color: Theme.colors.border
                
                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon(root.selectedCount === root.totalCount && root.totalCount > 0 ? "check-square" : "square")
                    iconSize: 14
                    color: Theme.colors.icon
                }
                
                MouseArea {
                    id: selectAllMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.selectedCount === root.totalCount && root.totalCount > 0) {
                            root.deselectAll()
                        } else {
                            root.selectAll()
                        }
                    }
                }

                Tooltip {
                    id: selectAllTooltip
                    text: root.selectedCount === root.totalCount && root.totalCount > 0 ? qsTr("取消全选") : qsTr("全选")
                }

                Timer {
                    id: selectAllTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (selectAllMouse.containsMouse) {
                            selectAllTooltip.show(parent.parent)
                        }
                    }
                }

                Connections {
                    target: selectAllMouse
                    function onContainsMouseChanged() {
                        if (selectAllMouse.containsMouse) {
                            selectAllTooltipTimer.start()
                        } else {
                            selectAllTooltipTimer.stop()
                            selectAllTooltip.close()
                        }
                    }
                }
            }
            
            Rectangle {
                implicitWidth: selectedCountText.implicitWidth + 10
                height: 28
                radius: Theme.radius.sm
                color: Theme.colors.primarySubtle
                border.width: 0
                
                Text {
                    id: selectedCountText
                    anchors.centerIn: parent
                    text: qsTr("%1/%2").arg(root.selectedCount).arg(root.totalCount)
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: Theme.colors.primary
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Rectangle {
                width: 28
                height: 28
                radius: Theme.radius.sm
                color: clearMouse.containsMouse && root.selectedCount > 0 ? 
                       Theme.colors.warningSubtleBg : "transparent"
                border.width: 1
                border.color: root.selectedCount > 0 ? 
                              (clearMouse.containsMouse ? Theme.colors.warningHover : Theme.colors.border) : 
                              Theme.colors.border
                opacity: root.selectedCount > 0 ? 1.0 : 0.4
                
                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon("eraser")
                    iconSize: 14
                    color: root.selectedCount > 0 ? Theme.colors.warning : Theme.colors.mutedForeground
                }
                
                MouseArea {
                    id: clearMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.selectedCount > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: root.selectedCount > 0
                    onClicked: root.clearSelected()
                }

                Tooltip {
                    id: clearTooltip
                    text: qsTr("清空选中会话")
                }

                Timer {
                    id: clearTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (clearMouse.containsMouse && root.selectedCount > 0) {
                            clearTooltip.show(parent.parent)
                        }
                    }
                }

                Connections {
                    target: clearMouse
                    function onContainsMouseChanged() {
                        if (clearMouse.containsMouse && root.selectedCount > 0) {
                            clearTooltipTimer.start()
                        } else {
                            clearTooltipTimer.stop()
                            clearTooltip.close()
                        }
                    }
                }
            }
            
            Rectangle {
                width: 28
                height: 28
                radius: Theme.radius.sm
                color: deleteMouse.containsMouse && root.selectedCount > 0 ? 
                       Theme.colors.destructiveSubtle : "transparent"
                border.width: 1
                border.color: root.selectedCount > 0 ? 
                              (deleteMouse.containsMouse ? Theme.colors.destructive : Theme.colors.border) : 
                              Theme.colors.border
                opacity: root.selectedCount > 0 ? 1.0 : 0.4
                
                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon("trash-2")
                    iconSize: 14
                    color: root.selectedCount > 0 ? Theme.colors.destructive : Theme.colors.mutedForeground
                }
                
                MouseArea {
                    id: deleteMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: root.selectedCount > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: root.selectedCount > 0
                    onClicked: root.deleteSelected()
                }

                Tooltip {
                    id: deleteTooltip
                    text: qsTr("删除选中会话")
                }

                Timer {
                    id: deleteTooltipTimer
                    interval: Theme.tooltip.delay
                    repeat: false
                    onTriggered: {
                        if (deleteMouse.containsMouse && root.selectedCount > 0) {
                            deleteTooltip.show(parent.parent)
                        }
                    }
                }

                Connections {
                    target: deleteMouse
                    function onContainsMouseChanged() {
                        if (deleteMouse.containsMouse && root.selectedCount > 0) {
                            deleteTooltipTimer.start()
                        } else {
                            deleteTooltipTimer.stop()
                            deleteTooltip.close()
                        }
                    }
                }
            }
        }
        
        SessionList {
            id: sessionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            expanded: root.expanded
            batchMode: root.batchMode
            activeSessionId: root.activeSessionId
            sessionModel: sessionController ? sessionController.sessionModel : null
            
            onSessionClicked: function(sessionId) {
                if (sessionController) sessionController.switchSession(sessionId)
            }
            
            onRenameRequested: function(sessionId, newName) {
                if (sessionController) sessionController.renameSession(sessionId, newName)
            }
            
            onPinRequested: function(sessionId, pinned) {
                if (sessionController) sessionController.pinSession(sessionId, pinned)
            }
            
            onClearRequested: function(sessionId) {
                root.pendingClearSessionId = sessionId
                var sessionName = sessionController ? sessionController.getSessionName(sessionId) : ""
                NotificationManager.showConfirmDialog(
                    qsTr("清空会话"),
                    qsTr("确定要清空会话「%1」的所有消息吗？此操作不可恢复。").arg(sessionName),
                    function() {
                        if (sessionController) sessionController.clearSession(root.pendingClearSessionId)
                        root.pendingClearSessionId = ""
                    },
                    function() {
                        root.pendingClearSessionId = ""
                    },
                    qsTr("清空"),
                    qsTr("取消")
                )
            }
            
            onDeleteRequested: function(sessionId) {
                root.pendingDeleteSessionId = sessionId
                var sessionName = sessionController ? sessionController.getSessionName(sessionId) : ""
                NotificationManager.showConfirmDialog(
                    qsTr("删除会话"),
                    qsTr("确定要删除会话「%1」吗？此操作不可恢复。").arg(sessionName),
                    function() {
                        if (sessionController) sessionController.deleteSession(root.pendingDeleteSessionId)
                        root.pendingDeleteSessionId = ""
                    },
                    function() {
                        root.pendingDeleteSessionId = ""
                    },
                    qsTr("删除"),
                    qsTr("取消")
                )
            }
            
            onSelectionChanged: function(sessionId, selected) {
                if (sessionController) sessionController.selectSession(sessionId, selected)
            }
            
            onMoveRequested: function(fromIndex, toIndex) {
                if (sessionController) sessionController.moveSession(fromIndex, toIndex)
            }
        }
    }
    
    Behavior on color {
        ColorAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
    }
}
