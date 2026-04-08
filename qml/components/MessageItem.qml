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

    // ========== 时间预测系统（由 C++ ProcessingTimeManager 管理）==========
    // 直接从模型读取的值，由 C++ 后端每秒更新
    property real predictedTotalSec: 0.0
    property real elapsedSec: 0
    property real remainingSec: 0
    property bool isOvertime: false
    
    // 实际处理开始时间戳（用于持久化）
    property double _processingStartTime: 0
    // 所有任务完成后的实际总耗时（秒）
    property real _actualTotalSec: 0
    // 是否使用 GPU
    property bool useGpu: false
    // AI 模型 ID
    property string modelId: ""
    // 从模型读取的持久化实际总耗时（用于恢复显示）
    property real persistedActualTotalSec: 0
    // 从模型读取的持久化处理开始时间（用于会话切换后恢复进度）
    property double persistedProcessingStartTime: 0

    // 标记是否已开始处理（避免进度条倒走）
    property bool _hasStartedProcessing: persistedProcessingStartTime > 0 || _processingStartTime > 0

    // 动画控制属性
    property bool _shouldBreathAnimate: status === 1
    property bool _shouldWaitingAnimate: status === 0

    // 基于时间的进度百分比（用于进度条）
    readonly property real _timeBasedProgress: {
        if (!_hasStartedProcessing) return 0
        if (predictedTotalSec <= 0) return 0
        if (root.allFilesSettled) return 100
        var progressPct = (elapsedSec / predictedTotalSec) * 100
        return Math.min(99, Math.max(0, progressPct))
    }

    // 当所有文件处理完成后，计算实际总耗时并持久化
    onAllFilesSettledChanged: {
        // 【修复】只有在真正开始处理过（有处理开始时间）且所有文件都已结算时才计算总耗时
        // 避免在任务还未开始时就显示总耗时
        if (allFilesSettled && _actualTotalSec === 0 && _hasStartedProcessing) {
            // 使用实际经过的时间，如果太短则使用持久化的值或默认1秒
            var totalSec = elapsedSec > 0 ? elapsedSec : (persistedActualTotalSec > 0 ? persistedActualTotalSec : 1)
            _actualTotalSec = totalSec
            // 通知父组件持久化总耗时到 C++
            if (totalSec > 0) {
                actualTotalSecChanged(totalSec)
            }
        }
    }

    // 从持久化值恢复实际总耗时（用于重新打开应用时显示）
    onPersistedActualTotalSecChanged: {
        if (persistedActualTotalSec > 0 && _actualTotalSec === 0) {
            _actualTotalSec = persistedActualTotalSec
        }
    }

    property int successFileCount: 0
    property int failedFileCount: 0
    property int pendingFileCount: 0
    property int processingFileCount: 0
    property int pausedFileCount: 0
    property int recoverableFileCount: 0
    property bool hasFailedFiles: failedFileCount > 0
    property bool allFilesSettled: totalFileCount > 0 && pendingFileCount === 0 && processingFileCount === 0 && pausedFileCount === 0 && recoverableFileCount === 0
    property bool allFilesPending: totalFileCount > 0 && pendingFileCount === totalFileCount
    property bool failedTipDismissed: false
    
    // 状态变化时显式控制动画
    onStatusChanged: {
        _updateBreathAnimation()
        _updateWaitingAnimation()
    }
    
    // 呼吸感动画控制函数
    function _updateBreathAnimation() {
        if (_shouldBreathAnimate) {
            breathAnimation.start()
        } else {
            breathAnimation.stop()
        }
    }
    
    // 等待状态动画控制函数
    function _updateWaitingAnimation() {
        if (_shouldWaitingAnimate) {
            waitingAnimation.start()
        } else {
            waitingAnimation.stop()
        }
    }
    
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
    signal pauseClicked()
    signal resumeClicked()
    signal actualTotalSecChanged(real totalSec)
    
    // 暂停状态（status === 5 表示 Paused）
    property bool isPaused: status === 5
    property bool isRecoverable: status === 6
    
    // 全局暂停状态（模式三使用）
    property bool globalPauseEnabled: false
    
    // 当前暂停模式：0=单任务暂停, 1=顺序暂停, 2=自由选择
    property int pauseMode: 0
    
    // 是否应该显示继续按钮（模式三：全局暂停时所有待处理卡片显示继续按钮）
    readonly property bool shouldShowResumeButton: {
        if (root.isPaused) return true
        if (root.isRecoverable) return false
        if (pauseMode === 2 && globalPauseEnabled && !root.allFilesSettled && (status === 0 || status === 1)) {
            return true
        }
        return false
    }
    
    // 是否应该显示暂停按钮
    readonly property bool shouldShowPauseButton: {
        if (root.isPaused) return false
        if (root.isRecoverable) return false
        if (status === 1 && !root.allFilesSettled) return true  // 处理中
        return false
    }
    
    property bool _hasStatsSnapshot: false
    property var _deletingFileIds: ({})
    property int _deletingCount: 0

    function _applyFileStats(success, failed, pending, processing, paused, recoverable) {
        successFileCount = success
        failedFileCount = failed
        pendingFileCount = pending
        processingFileCount = processing
        pausedFileCount = paused !== undefined ? paused : 0
        recoverableFileCount = recoverable !== undefined ? recoverable : 0
        _hasStatsSnapshot = true

        if (failed === 0 || pending > 0 || processing > 0 || recoverableFileCount > 0) {
            failedTipDismissed = false
        }
    }

    function _updateFileCounts() {
        var success = 0
        var failed = 0
        var pending = 0
        var processing = 0
        var paused = 0
        var recoverable = 0
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
                } else if (item.status === 5) {
                    paused++
                } else if (item.status === 6) {
                    recoverable++
                }
            }
        }

        _applyFileStats(success, failed, pending, processing, paused, recoverable)
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
        if (status === 1 || root.isRecoverable) return 1.5
        return 1
    }
    border.color: {
        if (selected) return Theme.colors.primary
        if (status === 1) return breathColor
        if (root.isRecoverable) return Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.45)
        return Theme.colors.cardBorder
    }
    
    property color breathColor: Theme.colors.primary
    
    SequentialAnimation {
        id: breathAnimation
        running: false
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
                visible: root.totalFileCount > 0
                
                IconButton {
                    iconName: "download"; iconSize: 14; btnSize: 26
                    iconColor: Theme.colors.icon
                    tooltip: qsTr("保存成功文件")
                    visible: root.successFileCount > 0
                    onClicked: root.saveSuccessfulFilesClicked()
                }
                
                IconButton {
                    iconName: "refresh-cw"; iconSize: 14; btnSize: 26
                    iconColor: Theme.colors.icon
                    tooltip: qsTr("重新处理失败文件")
                    visible: root.hasFailedFiles
                    onClicked: root.retryFailedFilesClicked()
                }
                
                IconButton {
                    iconName: root.shouldShowResumeButton ? "play" : "pause"
                    iconSize: 14; btnSize: 26
                    iconColor: root.shouldShowResumeButton ? Theme.colors.success : Theme.colors.warning
                    tooltip: root.shouldShowResumeButton ? qsTr("继续处理") : qsTr("暂停处理")
                    visible: (root.shouldShowPauseButton || root.shouldShowResumeButton) && !root.allFilesSettled
                    onClicked: {
                        if (root.shouldShowResumeButton) {
                            root.resumeClicked()
                        } else {
                            root.pauseClicked()
                        }
                    }
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
            messagePaused: root.isPaused
            
            onViewFile: function(index) { root.viewMediaFile(index) }
            onSaveFile: function(index) { root.saveMediaFile(index) }
            onDeleteFile: function(index) {
                var _msgId = root.taskId
                var _fileId = ""
                if (root.mediaFiles && index >= 0 && index < root.mediaFiles.count) {
                    var _item = root.mediaFiles.get(index)
                    if (_item) _fileId = _item.id
                }
                if (!_fileId) return

                // fileId 级防抖：阻止对同一文件的重复删除
                if (root._deletingFileIds[_fileId]) return
                root._deletingFileIds[_fileId] = true
                root._deletingCount++

                var __msgId = _msgId
                var __fileId = _fileId
                Qt.callLater(function() {
                    messageModel.removeMediaFileById(__msgId, __fileId)
                    delete root._deletingFileIds[__fileId]
                    root._deletingCount = Math.max(0, root._deletingCount - 1)
                })
            }
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
            visible: (status === 0 || status === 1 || root.isPaused || root.isRecoverable) && !root.allFilesSettled
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                // 左侧状态文本
                Text {
                    text: {
                        if (root.isRecoverable) return qsTr("待恢复")
                        if (root.isPaused) return qsTr("已暂停")
                        if (status === 0) return qsTr("等待处理...")
                        return qsTr("处理中")
                    }
                    color: root.isRecoverable ? Theme.colors.primary : (root.isPaused ? Theme.colors.warning : Theme.colors.foreground)
                    font.pixelSize: 12
                }

                // 处理中显示文件进度
                Text {
                    visible: status === 1
                    text: qsTr("(%1/%2)").arg(root.completedCount).arg(root.totalFileCount)
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                // 时间显示区域
                RowLayout {
                    spacing: 6

                    // 预测总时间
                    Text {
                        visible: root.predictedTotalSec > 0
                        text: {
                            var s = Math.round(root.predictedTotalSec)
                            if (s >= 3600) return qsTr("预估: %1h%2m").arg(Math.floor(s/3600)).arg(Math.floor((s%3600)/60))
                            if (s >= 60) return qsTr("预估: %1m%2s").arg(Math.floor(s/60)).arg(s%60)
                            return qsTr("预估: %1s").arg(s)
                        }
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 10
                    }

                    // 分隔符
                    Text {
                        visible: status === 1 && root.predictedTotalSec > 0
                        text: "|"
                        color: Theme.colors.border
                        font.pixelSize: 10
                    }

                    // 已耗时（正向计时，处理中或暂停时显示）
                    Text {
                        visible: (status === 1 || root.isPaused) && root.elapsedSec > 0
                        text: {
                            var s = Math.round(root.elapsedSec)
                            // 暂停时显示"暂停于"前缀
                            var prefix = root.isPaused ? qsTr("暂停于: ") : qsTr("已耗时: ")
                            if (s >= 3600) return prefix + qsTr("%1h%2m").arg(Math.floor(s/3600)).arg(Math.floor((s%3600)/60))
                            if (s >= 60) return prefix + qsTr("%1m%2s").arg(Math.floor(s/60)).arg(s%60)
                            return prefix + qsTr("%1s").arg(s)
                        }
                        // 暂停时显示警告色，否则显示主色
                        color: root.isPaused ? Theme.colors.warning : Theme.colors.primary
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }

                }
            }

            Text {
                visible: root.isRecoverable
                Layout.fillWidth: true
                text: qsTr("上次退出时未完成，等待恢复决策")
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            // 进度条（基于时间预测移动）
            Rectangle {
                Layout.fillWidth: true
                height: 4; radius: 2
                color: Theme.colors.muted
                visible: !root.allFilesSettled

                Rectangle {
                    width: {
                        if (root.status === 0) return 0
                        if (root.allFilesSettled) return parent.width
                        // 【修复】优先使用实际的任务进度（来自 AI 推理或其他处理器）
                        // 只有当实际进度为 0 且有时间预测时才使用时间预测进度
                        var actualProgress = Math.min(root.progress, 100)
                        if (actualProgress > 0) {
                            return parent.width * (actualProgress / 100)
                        }
                        // 回退到基于时间预测的进度
                        if (root.predictedTotalSec > 0 && root._timeBasedProgress > 0) {
                            return parent.width * (root._timeBasedProgress / 100)
                        }
                        return 0
                    }
                    height: parent.height; radius: parent.radius
                    color: Theme.colors.primary

                    Behavior on width {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }
                }

                // 等待状态动画
                Rectangle {
                    id: waitingBar
                    visible: root.status === 0
                    width: parent.width * 0.3
                    height: parent.height; radius: parent.radius
                    color: Theme.colors.primary
                    opacity: 0.5
                }
                
                SequentialAnimation {
                    id: waitingAnimation
                    running: root._shouldWaitingAnimate
                    loops: Animation.Infinite
                    
                    NumberAnimation {
                        target: waitingBar
                        property: "x"
                        from: 0
                        to: waitingBar.parent.width * 0.7
                        duration: 1500
                        easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        target: waitingBar
                        property: "x"
                        from: waitingBar.parent.width * 0.7
                        to: 0
                        duration: 1500
                        easing.type: Easing.InOutQuad
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

            // 完成后显示实际总耗时（放在文件统计右侧）
            // 显示条件：必须已开始处理且所有文件都已结算，或者有持久化的总耗时
            Text {
                property real displayTotalSec: root._actualTotalSec > 0 ? root._actualTotalSec : root.persistedActualTotalSec
                // 【修复】只有在真正处理完成后才显示总耗时
                // 条件：(已开始处理 且 所有文件已结算 且 有耗时数据) 或 有持久化的耗时数据
                visible: (root._hasStartedProcessing && root.allFilesSettled && displayTotalSec > 0) || root.persistedActualTotalSec > 0
                text: {
                    var s = displayTotalSec
                    if (s < 1) return qsTr("总耗时: <1s")
                    var sInt = Math.round(s)
                    if (sInt >= 3600) return qsTr("总耗时: %1h%2m%3s").arg(Math.floor(sInt/3600)).arg(Math.floor((sInt%3600)/60)).arg(sInt%60)
                    if (sInt >= 60) return qsTr("总耗时: %1m%2s").arg(Math.floor(sInt/60)).arg(sInt%60)
                    return qsTr("总耗时: %1s").arg(sInt)
                }
                // 使用蓝色
                color: "#4a9eff"
                font.pixelSize: 11
                font.weight: Font.DemiBold
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
    
    Component.onCompleted: {
        _updateBreathAnimation()
        _updateWaitingAnimation()
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
}
