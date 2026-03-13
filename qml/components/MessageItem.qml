import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 消息项组件
 * 
 * 显示单个处理任务的信息，参考功能设计文档 0.2 节 3.10 节
 */
Rectangle {
    id: root
    
    // ========== 属性定义 ==========
    property string taskId: ""
    property string timestamp: ""
    property int mode: 0        // 0: Shader, 1: AI
    property int status: 0      // 0: Pending, 1: Running, 2: Completed, 3: Failed, 4: Cancelled
    property real progress: 0
    property int queuePosition: 0
    property string errorMessage: ""
    property bool selected: false
    property bool selectable: false
    
    // ========== 信号定义 ==========
    signal cancelClicked()
    signal editClicked()
    signal saveClicked()
    signal deleteClicked()
    signal selectedChanged(bool isSelected)
    
    // ========== 视觉属性 ==========
    color: mouseArea.containsMouse ? Theme.colors.surfaceHover : Theme.colors.card
    border.width: selected ? 2 : 1
    border.color: selected ? Theme.colors.primary : Theme.colors.cardBorder
    radius: Theme.radius.lg
    implicitHeight: contentLayout.implicitHeight + 24
    
    // ========== 内部属性 ==========
    readonly property color statusColor: {
        switch (status) {
            case 0: return Theme.colors.mutedForeground
            case 1: return Theme.colors.primary
            case 2: return Theme.colors.success
            case 3: return Theme.colors.destructive
            case 4: return Theme.colors.mutedForeground
            default: return Theme.colors.mutedForeground
        }
    }
    
    readonly property string statusText: {
        switch (status) {
            case 0: return qsTr("排队中")
            case 1: return qsTr("处理中")
            case 2: return qsTr("已完成")
            case 3: return qsTr("失败")
            case 4: return qsTr("已取消")
            default: return qsTr("未知")
        }
    }
    
    readonly property string modeText: mode === 0 ? "Shader" : "AI"
    readonly property string modeIconName: mode === 0 ? "sliders" : "sparkles"
    
    // ========== 鼠标交互 ==========
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (selectable) {
                root.selected = !root.selected
                root.selectedChanged(root.selected)
            }
        }
    }
    
    // ========== 内部布局 ==========
    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        
        // ========== 顶部：模式标签、ID、时间 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            // 选择框
            Rectangle {
                visible: selectable
                width: 18; height: 18; radius: 4
                color: selected ? Theme.colors.primary : "transparent"
                border.width: selected ? 0 : 1
                border.color: Theme.colors.border
                
                Image {
                    anchors.centerIn: parent
                    width: 10; height: 10
                    source: Theme.icon("check-circle")
                    sourceSize: Qt.size(10, 10)
                    visible: selected
                    smooth: true
                }
            }
            
            // 模式标签
            Rectangle {
                width: modeRow.implicitWidth + 12
                height: 22
                radius: Theme.radius.sm
                color: mode === 0 ? Theme.colors.primarySubtle : Theme.colors.accent
                
                Row {
                    id: modeRow
                    anchors.centerIn: parent
                    spacing: 4
                    
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10; height: 10
                        source: Theme.icon(modeIconName)
                        sourceSize: Qt.size(10, 10)
                        smooth: true
                    }
                    
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modeText
                        color: mode === 0 ? Theme.colors.primary : Theme.colors.accentForeground
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }
            
            Text {
                text: "#" + taskId.substring(0, 8)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
                font.family: "Consolas"
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                text: timestamp
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }
        }
        
        // ========== 进度条（处理中） ==========
        ColumnLayout {
            visible: status === 1
            Layout.fillWidth: true
            spacing: 4
            
            RowLayout {
                Text { text: qsTr("处理进度"); color: Theme.colors.foreground; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text { text: Math.round(progress) + "%"; color: Theme.colors.primary; font.pixelSize: 12; font.weight: Font.Bold }
            }
            
            Rectangle {
                Layout.fillWidth: true
                height: 4; radius: 2
                color: Theme.colors.muted
                
                Rectangle {
                    width: parent.width * (progress / 100)
                    height: parent.height; radius: parent.radius
                    color: Theme.colors.primary
                    Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                }
            }
        }
        
        // ========== 状态行 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            
            Rectangle { width: 6; height: 6; radius: 3; color: statusColor }
            Text { text: statusText; color: statusColor; font.pixelSize: 12; font.weight: Font.Medium }
            
            Text {
                visible: status === 0 && queuePosition > 0
                text: qsTr("队列 #%1").arg(queuePosition)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }
            
            Item { Layout.fillWidth: true }
            
            // 操作按钮（悬浮时显示）
            Row {
                spacing: 2
                visible: mouseArea.containsMouse || status === 2 || status === 3
                
                IconButton {
                    visible: status === 0 || status === 1
                    iconName: "x-circle"; iconSize: 14; btnSize: 26
                    tooltip: qsTr("取消任务")
                    onClicked: root.cancelClicked()
                }
                IconButton {
                    visible: status === 2
                    iconName: "pencil"; iconSize: 14; btnSize: 26
                    tooltip: qsTr("编辑参数")
                    onClicked: root.editClicked()
                }
                IconButton {
                    visible: status === 2
                    iconName: "save"; iconSize: 14; btnSize: 26
                    tooltip: qsTr("保存结果")
                    onClicked: root.saveClicked()
                }
                IconButton {
                    visible: status >= 2
                    iconName: "trash"; iconSize: 14; btnSize: 26
                    danger: true
                    tooltip: qsTr("删除任务")
                    onClicked: root.deleteClicked()
                }
            }
        }
        
        // ========== 错误信息 ==========
        Rectangle {
            visible: status === 3 && errorMessage !== ""
            Layout.fillWidth: true
            height: errorText.implicitHeight + 12
            radius: Theme.radius.sm
            color: Theme.colors.destructiveSubtle
            
            Text {
                id: errorText
                anchors.fill: parent
                anchors.margins: 6
                text: errorMessage
                color: Theme.colors.destructive
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
    Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
}
