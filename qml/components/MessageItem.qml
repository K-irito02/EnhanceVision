import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 消息项组件
 * 
 * 显示单个处理任务的信息，包含：
 * - 缩略图展示区（支持展开/收缩、拖拽滚动、右键菜单、点击放大）
 * - 处理中呼吸感闪烁边框
 * - 进度条带预估完成时间
 * - 右上角：下载、编辑、删除按钮
 * - 右下角：时间戳
 * - 只显示已完成的文件缩略图
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
    
    /** @brief 媒体文件列表模型 */
    property var mediaFiles: ListModel {}
    
    /** @brief 已完成文件数 */
    property int completedCount: 0
    
    /** @brief 总文件数 */
    property int totalFileCount: mediaFiles ? mediaFiles.count : 0
    
    /** @brief 预估剩余时间（秒） */
    property int estimatedRemainingSec: 0
    
    // ========== 信号定义 ==========
    signal cancelClicked()
    signal editClicked()
    signal saveClicked()
    signal deleteClicked()
    signal downloadClicked()
    signal selectionToggled(bool isSelected)
    signal viewMediaFile(int index)
    signal saveMediaFile(int index)
    signal deleteMediaFile(int index)
    
    // ========== 视觉属性 ==========
    color: Theme.colors.card
    radius: Theme.radius.lg
    implicitHeight: contentLayout.implicitHeight + 24
    
    // 边框 - 处理中时呼吸闪烁
    border.width: {
        if (selected) return 2
        if (status === 1) return 1.5  // 处理中
        return 1
    }
    border.color: {
        if (selected) return Theme.colors.primary
        if (status === 1) return breathColor
        return Theme.colors.cardBorder
    }
    
    // ========== 呼吸灯动画（处理中） ==========
    property color breathColor: Theme.colors.primary
    
    SequentialAnimation {
        id: breathAnimation
        running: root.status === 1
        loops: Animation.Infinite
        
        ColorAnimation {
            target: root
            property: "breathColor"
            from: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.3)
            to: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.9)
            duration: 1200
            easing.type: Easing.InOutSine
        }
        ColorAnimation {
            target: root
            property: "breathColor"
            from: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.9)
            to: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.3)
            duration: 1200
            easing.type: Easing.InOutSine
        }
    }
    
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
    
    readonly property string statusIconName: {
        switch (status) {
            case 0: return "loader"
            case 1: return "loader"
            case 2: return "check-circle"
            case 3: return "alert-triangle"
            case 4: return "x-circle"
            default: return "loader"
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
        acceptedButtons: Qt.LeftButton
        onClicked: {
            if (selectable) {
                root.selected = !root.selected
                root.selectionToggled(root.selected)
            }
        }
    }
    
    // ========== 内部布局 ==========
    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        
        // ========== 顶部：状态指示 + 时间 + 操作按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            // 选择框（批量模式）
            Rectangle {
                visible: selectable
                width: 18; height: 18; radius: 4
                color: selected ? Theme.colors.primary : "transparent"
                border.width: selected ? 0 : 1
                border.color: Theme.colors.border
                
                ColoredIcon {
                    anchors.centerIn: parent
                    source: Theme.icon("check")
                    iconSize: 10
                    color: "#FFFFFF"
                    visible: selected
                }
            }
            
            // 状态图标
            ColoredIcon {
                source: Theme.icon(statusIconName)
                iconSize: 16
                color: statusColor
                
                RotationAnimation on rotation {
                    from: 0; to: 360
                    duration: 1500
                    loops: Animation.Infinite
                    running: root.status === 0 || root.status === 1
                }
            }
            
            // 时间戳
            Text {
                text: root.timestamp
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
            }
            
            Item { Layout.fillWidth: true }
            
            // ========== 右上角操作按钮 ==========
            Row {
                spacing: 2
                visible: mouseArea.containsMouse || status === 2
                
                // 下载按钮
                IconButton {
                    iconName: "download"; iconSize: 14; btnSize: 26
                    tooltip: qsTr("下载全部已处理文件")
                    onClicked: root.downloadClicked()
                }
                
                // 编辑按钮
                IconButton {
                    iconName: "pencil"; iconSize: 14; btnSize: 26
                    tooltip: qsTr("编辑文件")
                    onClicked: root.editClicked()
                }
                
                // 删除按钮
                IconButton {
                    iconName: "trash"; iconSize: 14; btnSize: 26
                    danger: true
                    tooltip: qsTr("删除此消息")
                    onClicked: root.deleteClicked()
                }
            }
        }
        
        // ========== 缩略图展示区 ==========
        MediaThumbnailStrip {
            Layout.fillWidth: true
            mediaModel: root.mediaFiles
            thumbSize: 80
            thumbSpacing: 8
            expandable: true
            onlyCompleted: true
            messageMode: true
            
            onViewFile: function(index) { root.viewMediaFile(index) }
            onSaveFile: function(index) { root.saveMediaFile(index) }
            onDeleteFile: function(index) { root.deleteMediaFile(index) }
        }
        
        // ========== 进度条（处理中/排队中） ==========
        ColumnLayout {
            visible: status === 0 || status === 1
            Layout.fillWidth: true
            spacing: 4
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                
                Text {
                    text: {
                        if (status === 0) return qsTr("等待处理...")
                        return qsTr("处理进度")
                    }
                    color: Theme.colors.foreground
                    font.pixelSize: 12
                }
                
                Item { Layout.fillWidth: true }
                
                // 已完成/总数
                Text {
                    visible: status === 1
                    text: qsTr("%1/%2 文件").arg(root.completedCount).arg(root.totalFileCount)
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                }
                
                Text {
                    visible: status === 1
                    text: Math.round(progress) + "%"
                    color: Theme.colors.primary
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }
                
                // 预估剩余时间
                Text {
                    visible: status === 1 && root.estimatedRemainingSec > 0
                    text: {
                        var s = root.estimatedRemainingSec
                        if (s >= 3600) return qsTr("预计 %1h%2m").arg(Math.floor(s/3600)).arg(Math.floor((s%3600)/60))
                        if (s >= 60) return qsTr("预计 %1m%2s").arg(Math.floor(s/60)).arg(s%60)
                        return qsTr("预计 %1s").arg(s)
                    }
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                }
            }
            
            // 进度条
            Rectangle {
                Layout.fillWidth: true
                height: 4; radius: 2
                color: Theme.colors.muted
                
                Rectangle {
                    width: {
                        if (root.status === 0) return 0
                        return parent.width * (root.progress / 100)
                    }
                    height: parent.height; radius: parent.radius
                    color: Theme.colors.primary
                    
                    Behavior on width {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }
                }
                
                // 排队中的不确定进度动画
                Rectangle {
                    visible: root.status === 0
                    width: parent.width * 0.3
                    height: parent.height; radius: parent.radius
                    color: Theme.colors.primary
                    opacity: 0.5
                    
                    SequentialAnimation on x {
                        running: root.status === 0
                        loops: Animation.Infinite
                        NumberAnimation {
                            from: 0
                            to: root.width * 0.7
                            duration: 1500
                            easing.type: Easing.InOutQuad
                        }
                        NumberAnimation {
                            from: root.width * 0.7
                            to: 0
                            duration: 1500
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }
        }
        
        // ========== 底部状态行 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            
            // 状态标签
            Rectangle {
                width: statusLabelRow.implicitWidth + 12
                height: 22
                radius: Theme.radius.sm
                color: {
                    switch (root.status) {
                        case 0: return Theme.colors.muted
                        case 1: return Theme.colors.primarySubtle
                        case 2: return Theme.colors.successSubtle
                        case 3: return Theme.colors.destructiveSubtle
                        case 4: return Theme.colors.muted
                        default: return Theme.colors.muted
                    }
                }
                
                Row {
                    id: statusLabelRow
                    anchors.centerIn: parent
                    spacing: 4
                    
                    Rectangle { 
                        anchors.verticalCenter: parent.verticalCenter
                        width: 6; height: 6; radius: 3; color: statusColor 
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: statusText
                        color: statusColor
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }
            
            // 模式标签
            Rectangle {
                width: modeLabelRow.implicitWidth + 10
                height: 22
                radius: Theme.radius.sm
                color: mode === 0 ? Theme.colors.primarySubtle : Theme.colors.accent
                
                Row {
                    id: modeLabelRow
                    anchors.centerIn: parent
                    spacing: 4
                    
                    ColoredIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        source: Theme.icon(modeIconName)
                        iconSize: 10
                        color: mode === 0 ? Theme.colors.primary : Theme.colors.accentForeground
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
            
            // 队列位置
            Text {
                visible: status === 0 && queuePosition > 0
                text: qsTr("队列 #%1").arg(queuePosition)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }
            
            Item { Layout.fillWidth: true }
            
            // 文件计数
            Text {
                visible: root.totalFileCount > 0
                text: qsTr("%1 个文件").arg(root.totalFileCount)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
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
}
