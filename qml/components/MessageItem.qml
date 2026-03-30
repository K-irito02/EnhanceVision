import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

Rectangle {
    id: root
    
    property string taskId: ""
    property string timestamp: ""
    property int mode: 0
    property int status: 0
    property real progress: 0
    property int queuePosition: 0
    property string errorMessage: ""
    property bool selected: false
    property bool selectable: false
    
    property var mediaFiles: null
    
    property int completedCount: 0
    property int totalFileCount: mediaFiles ? mediaFiles.count : 0
    property int estimatedRemainingSec: 0
    
    property var _timeSamples: []
    property real _lastProgress: 0
    
    property double _processingStartTime: 0
    property int _elapsedSec: 0
    
    // 使用 onProgressChanged 更新剩余时间，避免绑定循环
    onProgressChanged: {
        _updateRemainingTime()
    }
    
    on_ElapsedSecChanged: {
        _updateRemainingTime()
    }
    
    function _updateRemainingTime() {
        if (status !== 1 || progress <= 0) {
            estimatedRemainingSec = 0
            return
        }
        
        // 检测进度回退（新任务开始）
        if (_lastProgress > 0 && progress < _lastProgress - 10) {
            _timeSamples = []
            _lastProgress = progress
            estimatedRemainingSec = 0
            return
        }
        
        if (_elapsedSec > 0 && progress > 1) {
            var rate = _elapsedSec / progress
            var remaining = Math.round(rate * (100 - progress))
            
            // 更新样本数组（创建新数组避免绑定循环）
            var newSamples = _timeSamples.slice()
            newSamples.push(remaining)
            if (newSamples.length > 5) {
                newSamples.shift()
            }
            _timeSamples = newSamples
            
            var smoothed = 0
            for (var i = 0; i < _timeSamples.length; i++) {
                smoothed += _timeSamples[i]
            }
            smoothed = Math.round(smoothed / _timeSamples.length)
            
            _lastProgress = progress
            estimatedRemainingSec = Math.max(0, Math.min(smoothed, 3600 * 24))
        } else {
            estimatedRemainingSec = Math.max(0, Math.round((100 - progress) * 0.3))
        }
    }
    
    Timer {
        id: elapsedTimer
        interval: 1000
        repeat: true
        running: root.status === 1  // 仅在处理状态下运行
        onTriggered: {
            if (root._processingStartTime > 0) {
                root._elapsedSec = Math.round((Date.now() - root._processingStartTime) / 1000)
            }
        }
    }
    
    onStatusChanged: {
        if (status === 1) {
            // 开始处理：记录开始时间
            if (_processingStartTime <= 0) {
                _processingStartTime = Date.now()
                _elapsedSec = 0
            }
        } else {
            // 处理结束：重置时间跟踪
            _processingStartTime = 0
            _elapsedSec = 0
        }
    }
    
    property int successFileCount: 0
    property int failedFileCount: 0
    property int pendingFileCount: 0
    property int processingFileCount: 0
    property bool hasFailedFiles: failedFileCount > 0
    property bool allFilesSettled: totalFileCount > 0 && pendingFileCount === 0 && processingFileCount === 0
    property bool allFilesPending: totalFileCount > 0 && pendingFileCount === totalFileCount
    property bool failedTipDismissed: false
    
    signal cancelClicked()
    signal retryClicked()
    signal retryFailedFilesClicked()
    signal deleteClicked()
    signal downloadClicked()
    signal saveSuccessfulFilesClicked()
    signal selectionToggled(bool isSelected)
    signal viewMediaFile(int index)
    signal saveMediaFile(int index)
    signal deleteMediaFile(int index)
    signal retrySingleFailedFile(int index)
    
    property bool _hasStatsSnapshot: false

    function _applyFileStats(success, failed, pending, processing) {
        successFileCount = success
        failedFileCount = failed
        pendingFileCount = pending
        processingFileCount = processing
        _hasStatsSnapshot = true

        if (failed === 0 || pending > 0 || processing > 0) {
            failedTipDismissed = false
        }
    }

    function _updateFileCounts() {
        var success = 0
        var failed = 0
        var pending = 0
        var processing = 0
        if (mediaFiles && mediaFiles.count > 0) {
            for (var i = 0; i < mediaFiles.count; i++) {
                var item = mediaFiles.get(i)
                if (item.status === 2) {
                    success++
                } else if (item.status === 3 || item.status === 4) {
                    failed++
                } else if (item.status === 0) {
                    pending++
                } else if (item.status === 1) {
                    processing++
                }
            }
        }

        _applyFileStats(success, failed, pending, processing)
    }

    function scheduleFileCountUpdate(force) {
        if (force === true) {
            if (fileCountUpdateTimer.running) {
                fileCountUpdateTimer.stop()
            }
            _updateFileCounts()
            return
        }

        if (fileCountUpdateTimer.running) {
            return
        }
        fileCountUpdateTimer.start()
    }

    Timer {
        id: fileCountUpdateTimer
        interval: 80
        repeat: false
        onTriggered: root._updateFileCounts()
    }
    
    Connections {
        target: mediaFiles
        enabled: mediaFiles !== null
        function onDataChanged() {
            root.scheduleFileCountUpdate()
        }
        function onCountChanged() {
            root.scheduleFileCountUpdate()
        }
    }
    
    onMediaFilesChanged: {
        if (!_hasStatsSnapshot) {
            scheduleFileCountUpdate()
        }
    }
    
    color: Theme.colors.card
    radius: Theme.radius.lg
    implicitHeight: contentLayout.implicitHeight + 24
    
    border.width: {
        if (selected) return 2
        if (status === 1) return 1.5
        return 1
    }
    border.color: {
        if (selected) return Theme.colors.primary
        if (status === 1) return breathColor
        return Theme.colors.cardBorder
    }
    
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
    
    readonly property string modeText: mode === 0 ? "Shader" : "AI"
    readonly property string modeIconName: mode === 0 ? "sliders" : "sparkles"
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
        acceptedButtons: Qt.LeftButton | Qt.NoButton
        onClicked: function(mouse) {
            if (selectable && mouse.button === Qt.LeftButton) {
                root.selected = !root.selected
                root.selectionToggled(root.selected)
            }
        }
        onWheel: function(wheel) {
            wheel.accepted = false
        }
    }
    
    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
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
                    color: Theme.colors.textOnPrimary
                    visible: selected
                }
            }
            
            Rectangle {
                height: 22
                radius: Theme.radius.sm
                color: mode === 0 ? Theme.colors.primarySubtle : Theme.colors.accent
                Layout.minimumWidth: modeLabelRow.implicitWidth + 14
                Layout.preferredWidth: modeLabelRow.implicitWidth + 14

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
            
            Text {
                text: root.timestamp
                color: Theme.colors.mutedForeground
                font.pixelSize: 12
            }
            
            Item { Layout.fillWidth: true }
            
            Row {
                spacing: 2
                visible: root.status === 2 || root.status === 3 || root.successFileCount > 0 || root.hasFailedFiles || root.allFilesPending
                
                IconButton {
                    iconName: "refresh-cw"; iconSize: 14; btnSize: 26
                    iconColor: Theme.colors.iconActive
                    tooltip: qsTr("重新处理失败文件")
                    visible: root.hasFailedFiles
                    onClicked: root.retryFailedFilesClicked()
                }
                
                IconButton {
                    iconName: "download"; iconSize: 14; btnSize: 26
                    iconColor: Theme.colors.iconActive
                    tooltip: qsTr("保存成功文件")
                    visible: root.successFileCount > 0
                    onClicked: root.saveSuccessfulFilesClicked()
                }
                
                IconButton {
                    iconName: "trash"; iconSize: 14; btnSize: 26
                    danger: true
                    tooltip: root.allFilesPending ? qsTr("删除待处理任务") : qsTr("删除此消息")
                    visible: root.totalFileCount > 0 || root.allFilesPending
                    onClicked: root.deleteClicked()
                }
            }
        }
        
        MediaThumbnailStrip {
            Layout.fillWidth: true
            mediaModel: root.mediaFiles
            messageId: root.taskId
            thumbSize: 80
            thumbSpacing: 8
            expandable: true
            onlyCompleted: false
            messageMode: true
            showFailedFiles: true
            
            onViewFile: function(index) { root.viewMediaFile(index) }
            onSaveFile: function(index) { root.saveMediaFile(index) }
            onDeleteFile: function(index) { root.deleteMediaFile(index) }
            onFileRemoved: function(msgId, fileIndex) { root.deleteMediaFile(fileIndex) }
            onRetryFailedFile: function(index) { root.retrySingleFailedFile(index) }
        }
        
        Rectangle {
            id: autoProcessFailedTip
            visible: root.allFilesSettled && root.hasFailedFiles && !root.failedTipDismissed
            Layout.fillWidth: true
            height: tipContentRow.implicitHeight + 16
            radius: Theme.radius.sm
            color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.1)
            border.width: 1
            border.color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.3)
            clip: true
            
            Row {
                id: tipContentRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8
                
                Text {
                    id: tipText
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("部分文件处理失败，可点击\"重新处理\"按钮或右键选择\"重新处理\"")
                    color: Theme.colors.warning
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width - tipCloseBtn.width - parent.spacing
                }
                
                IconButton {
                    id: tipCloseBtn
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "x"
                    iconSize: 14
                    btnSize: 24
                    tooltip: qsTr("关闭提示")
                    background: Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius.sm
                        color: parent.hovered ? Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.2) : "transparent"
                    }
                    onClicked: {
                        // 手动关闭单条提示仍可用；当状态再次变化时会自动重新评估显示
                        root.failedTipDismissed = true
                    }
                }
            }
        }
        
        ColumnLayout {
            visible: ((status === 0 || status === 1) && !root._hasStatsSnapshot) || (root.pendingFileCount > 0 || root.processingFileCount > 0)
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
            
            Rectangle {
                Layout.fillWidth: true
                height: 4; radius: 2
                color: Theme.colors.muted
                
                Rectangle {
                    width: {
                        if (root.status === 0) return 0
                        if (root.status === 2) return parent.width
                        return parent.width * (Math.min(root.progress, 100) / 100)
                    }
                    height: parent.height; radius: parent.radius
                    color: Theme.colors.primary
                    
                    Behavior on width {
                        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                    }
                }
                
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
        
        Text {
            visible: status === 0 && queuePosition > 0
            text: qsTr("队列 #%1").arg(queuePosition)
            color: Theme.colors.mutedForeground
            font.pixelSize: 11
        }
        
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: root.totalFileCount > 0 && (root.status === 2 || root.status === 3 || root.successFileCount > 0 || root.hasFailedFiles)
            
            RowLayout {
                spacing: 8
                
                Rectangle {
                    height: 24
                    radius: Theme.radius.sm
                    color: Theme.colors.successSubtle
                    Layout.minimumWidth: successCountRow.implicitWidth + 12
                    Layout.preferredWidth: successCountRow.implicitWidth + 12
                    
                    Row {
                        id: successCountRow
                        anchors.centerIn: parent
                        spacing: 4
                        
                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("check-circle")
                            iconSize: 12
                            color: Theme.colors.success
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("成功：%1个文件").arg(root.successFileCount)
                            color: Theme.colors.success
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }
                }
                
                Rectangle {
                    height: 24
                    radius: Theme.radius.sm
                    color: Theme.colors.muted
                    Layout.minimumWidth: failedCountRow.implicitWidth + 12
                    Layout.preferredWidth: failedCountRow.implicitWidth + 12
                    
                    Row {
                        id: failedCountRow
                        anchors.centerIn: parent
                        spacing: 4
                        
                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("alert-triangle")
                            iconSize: 12
                            color: Theme.colors.foreground
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("失败：%1个文件").arg(root.failedFileCount)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                text: qsTr("%1 个文件").arg(root.totalFileCount)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }
        }
        
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
