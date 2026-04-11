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
    // 从模型读取的提示关闭状态（用于重启后恢复）
    property bool persistedFailedTipDismissed: false
    property bool persistedErrorTipDismissed: false

    // 标记是否已开始处理（避免进度条倒走）
    property bool _hasStartedProcessing: persistedProcessingStartTime > 0
                                         || _processingStartTime > 0
                                         || elapsedSec > 0
                                         || _actualTotalSec > 0
                                         || persistedActualTotalSec > 0

    // 动画控制属性
    readonly property bool processingBorderAnimated: status === 1 && processingFileCount > 0
    readonly property bool waitingBarAnimated: status === 0
    property real _processingBorderOpacity: processingBorderAnimated ? 0.32 : 0.0
    property real _waitingBarOffset: 0

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
            // 仅作为临时显示值，最终以 C++ 持久化值为准
            var totalSec = elapsedSec > 0 ? elapsedSec : persistedActualTotalSec
            if (totalSec > 0) {
                _actualTotalSec = totalSec
            }
        }
    }

    // 从持久化值恢复实际总耗时（用于重新打开应用时显示）
    onPersistedActualTotalSecChanged: {
        // C++ 是总耗时单一真源，允许覆盖早到的临时值（例如完成瞬间的低估值）
        if (persistedActualTotalSec > 0 && persistedActualTotalSec !== _actualTotalSec) {
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
    property bool errorTipDismissed: false

    onPersistedFailedTipDismissedChanged: {
        failedTipDismissed = persistedFailedTipDismissed
    }
    onPersistedErrorTipDismissedChanged: {
        errorTipDismissed = persistedErrorTipDismissed
    }
    
    onProcessingBorderAnimatedChanged: {
        if (!processingBorderAnimated) {
            _processingBorderOpacity = 0.0
        }
    }

    onWaitingBarAnimatedChanged: {
        if (!waitingBarAnimated) {
            _waitingBarOffset = 0
        }
    }

    function _formatCompactDuration(seconds) {
        var s = Math.max(0, Math.round(seconds))
        if (s >= 3600) return qsTr("%1h%2m").arg(Math.floor(s / 3600)).arg(Math.floor((s % 3600) / 60))
        if (s >= 60) return qsTr("%1m%2s").arg(Math.floor(s / 60)).arg(s % 60)
        return qsTr("%1s").arg(s)
    }

    function _formatEstimatedDuration(seconds) {
        return qsTr("预估: %1").arg(_formatCompactDuration(seconds))
    }

    function _formatElapsedDuration(seconds, paused) {
        var prefix = paused ? qsTr("暂停于: ") : qsTr("已耗时: ")
        return prefix + _formatCompactDuration(seconds)
    }

    function _formatTotalDuration(seconds) {
        if (seconds < 1) return qsTr("总耗时: <1s")
        return qsTr("总耗时: %1").arg(_formatCompactDuration(seconds))
    }

    function _localizedErrorMessage(rawError) {
        if (!rawError || rawError.length === 0) return ""

        var lower = rawError.toLowerCase()
        if (rawError.indexOf("应用关闭导致任务中断") !== -1 || lower.indexOf("app was closed") !== -1) {
            return qsTr("应用关闭导致任务中断，未执行恢复")
        }
        if (rawError.indexOf("恢复快照不可用") !== -1 || lower.indexOf("recovery snapshot") !== -1) {
            return qsTr("恢复快照不可用，未完成任务已标记为失败")
        }
        return rawError
    }

    function _syncTipDismissState() {
        if (typeof messageModel === "undefined" || !taskId || taskId.length === 0) return
        messageModel.updateTipDismissState(taskId, failedTipDismissed, errorTipDismissed)
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
    
    // 消息是否已被激活（模式2自由选择使用，用户点击过"继续"）
    property bool isActivated: false
    
    // 是否应该显示继续按钮（模式三：全局暂停时所有待处理卡片显示继续按钮）
    readonly property bool shouldShowResumeButton: {
        if (root.isPaused) return true
        if (root.isRecoverable) return false
        // 模式2：已激活的消息不显示继续按钮（显示暂停按钮）
        if (pauseMode === 2 && root.isActivated) return false
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
        // 模式2：已激活但等待处理的消息显示暂停按钮
        if (pauseMode === 2 && root.isActivated && status === 0 && !root.allFilesSettled) return true
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
            if (failedTipDismissed) {
                failedTipDismissed = false
                _syncTipDismissState()
            }
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
    
    border.width: 1
    border.color: Theme.colors.cardBorder

    SequentialAnimation on _processingBorderOpacity {
        running: root.processingBorderAnimated
        loops: Animation.Infinite

        NumberAnimation {
            from: 0.28
            to: 0.88
            duration: 1200
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            from: 0.88
            to: 0.28
            duration: 1200
            easing.type: Easing.InOutSine
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: 2
        border.color: Theme.colors.primary
        visible: root.selected
        z: 1
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.width: (root.selected || (!root.processingBorderAnimated && !root.isRecoverable)) ? 0 : 1.5
        border.color: root.isRecoverable
            ? Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.45)
            : Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, root._processingBorderOpacity)
        visible: !root.selected && (root.processingBorderAnimated || root.isRecoverable)
        z: 1
    }
    
    readonly property string modeText: mode === 0 ? qsTr("Shader") : qsTr("AI")
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
                id: modeLabel
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
                        visible: root.width >= 350
                    }
                }

                Tooltip {
                    id: modeTooltip
                    text: modeText
                }

                Timer {
                    id: modeTooltipTimer
                    interval: 500
                    repeat: false
                    onTriggered: {
                        if (modeMouseArea.containsMouse && root.width < 350) {
                            modeTooltip.show(modeLabel)
                        }
                    }
                }

                MouseArea {
                    id: modeMouseArea
                    anchors.fill: parent
                    hoverEnabled: root.width < 350
                    acceptedButtons: Qt.NoButton
                    onContainsMouseChanged: {
                        if (containsMouse && root.width < 350) {
                            modeTooltipTimer.start()
                        } else {
                            modeTooltipTimer.stop()
                            modeTooltip.close()
                        }
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
                    iconName: "download"; iconSize: root.width < 350 ? 12 : 14; btnSize: root.width < 350 ? 22 : 26
                    iconColor: Theme.colors.icon
                    tooltip: qsTr("保存成功文件")
                    visible: root.successFileCount > 0
                    onClicked: root.saveSuccessfulFilesClicked()
                }

                IconButton {
                    iconName: "refresh-cw"; iconSize: root.width < 350 ? 12 : 14; btnSize: root.width < 350 ? 22 : 26
                    iconColor: Theme.colors.icon
                    tooltip: qsTr("重新处理失败文件")
                    visible: root.hasFailedFiles
                    onClicked: root.retryFailedFilesClicked()
                }

                IconButton {
                    iconName: root.shouldShowResumeButton ? "play" : "pause"
                    iconSize: root.width < 350 ? 12 : 14; btnSize: root.width < 350 ? 22 : 26
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
                    iconName: "trash"
                    iconSize: root.width < 350 ? 12 : 14; btnSize: root.width < 350 ? 22 : 26
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
                        root._syncTipDismissState()
                    }
                }
            }
        }
        
        ColumnLayout {
            visible: (status === 0 || status === 1 || root.isPaused || root.isRecoverable || (root.pauseMode === 2 && root.isActivated)) && !root.allFilesSettled
            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                // 左侧状态文本
                Text {
                    text: {
                        if (root.isRecoverable) return qsTr("待恢复")
                        // 模式2：已激活但等待处理的消息显示"等待处理..."
                        if (root.isPaused && !(root.pauseMode === 2 && root.isActivated)) return qsTr("已暂停")
                        if (status === 0 || (root.pauseMode === 2 && root.isActivated && status === 5)) return qsTr("等待处理...")
                        return qsTr("处理中")
                    }
                    color: root.isRecoverable ? Theme.colors.primary : ((root.isPaused && !(root.pauseMode === 2 && root.isActivated)) ? Theme.colors.warning : Theme.colors.foreground)
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    Layout.maximumWidth: Math.max(100, root.width * 0.28)
                }

                // 处理中显示文件进度
                Text {
                    visible: root.processingBorderAnimated
                    text: qsTr("(%1/%2)").arg(root.completedCount).arg(root.totalFileCount)
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.maximumWidth: Math.max(60, root.width * 0.18)
                }

                Item { Layout.fillWidth: true }

                // 时间显示区域，右侧紧凑成组，避免被拉得过开
                Item {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    implicitWidth: timeInfoRow.implicitWidth

                    Row {
                        id: timeInfoRow
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Text {
                            visible: root.predictedTotalSec > 0 && root.width > 280
                            text: root._formatEstimatedDuration(root.predictedTotalSec)
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 10
                            width: visible ? Math.min(96, parent.parent.width * 0.42) : 0
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: ((root.processingBorderAnimated || root.isPaused) && root.elapsedSec > 0 && root.predictedTotalSec > 0) && root.width > 320
                            text: "|"
                            color: Theme.colors.border
                            font.pixelSize: 10
                        }

                        Text {
                            visible: ((root.processingBorderAnimated || root.isPaused) && root.elapsedSec > 0)
                            text: root._formatElapsedDuration(root.elapsedSec, root.isPaused)
                            color: root.isPaused ? Theme.colors.warning : Theme.colors.primary
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            width: visible ? Math.min(112, parent.parent.width * 0.48) : 0
                            elide: Text.ElideRight
                        }
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
                    x: root._waitingBarOffset
                }
                
                SequentialAnimation {
                    running: root.waitingBarAnimated
                    loops: Animation.Infinite
                    
                    NumberAnimation {
                        target: root
                        property: "_waitingBarOffset"
                        from: 0
                        to: waitingBar.parent.width * 0.7
                        duration: 1500
                        easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        target: root
                        property: "_waitingBarOffset"
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
                    id: successCountRect
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
                            text: root.width < 350 ? root.successFileCount : qsTr("成功：%1个文件").arg(root.successFileCount)
                            color: Theme.colors.success
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }

                    Tooltip {
                        id: successTooltip
                        text: qsTr("成功：%1个文件").arg(root.successFileCount)
                    }

                    Timer {
                        id: successTooltipTimer
                        interval: 500
                        repeat: false
                        onTriggered: {
                            if (successMouseArea.containsMouse && root.width < 350) {
                                successTooltip.show(successCountRect)
                            }
                        }
                    }

                    MouseArea {
                        id: successMouseArea
                        anchors.fill: parent
                        hoverEnabled: root.width < 350
                        acceptedButtons: Qt.NoButton
                        onContainsMouseChanged: {
                            if (containsMouse && root.width < 350) {
                                successTooltipTimer.start()
                            } else {
                                successTooltipTimer.stop()
                                successTooltip.close()
                            }
                        }
                    }
                }
                
                Rectangle {
                    id: failedCountRect
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
                            text: root.width < 350 ? root.failedFileCount : qsTr("失败：%1个文件").arg(root.failedFileCount)
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }

                    Tooltip {
                        id: failedTooltip
                        text: qsTr("失败：%1个文件").arg(root.failedFileCount)
                    }

                    Timer {
                        id: failedTooltipTimer
                        interval: 500
                        repeat: false
                        onTriggered: {
                            if (failedMouseArea.containsMouse && root.width < 350) {
                                failedTooltip.show(failedCountRect)
                            }
                        }
                    }

                    MouseArea {
                        id: failedMouseArea
                        anchors.fill: parent
                        hoverEnabled: root.width < 350
                        acceptedButtons: Qt.NoButton
                        onContainsMouseChanged: {
                            if (containsMouse && root.width < 350) {
                                failedTooltipTimer.start()
                            } else {
                                failedTooltipTimer.stop()
                                failedTooltip.close()
                            }
                        }
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                text: qsTr("%1 个文件").arg(root.totalFileCount)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: Math.max(70, root.width * 0.2)
                visible: root.width >= 350
            }

            Text {
                property real displayTotalSec: root._actualTotalSec > 0 ? root._actualTotalSec : root.persistedActualTotalSec
                visible: root.allFilesSettled && displayTotalSec > 0
                text: root._formatTotalDuration(displayTotalSec)
                color: "#5B8DEF"
                font.pixelSize: root.width < 350 ? 10 : 11
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.minimumWidth: 60
                Layout.maximumWidth: Math.max(85, root.width * 0.28)
            }
        }
        
        Rectangle {
            visible: status === 3 && errorMessage !== "" && !root.errorTipDismissed
            Layout.fillWidth: true
            height: Math.max(36, errorRow.implicitHeight + 12)
            radius: Theme.radius.sm
            color: Theme.colors.destructiveSubtle

            Row {
                id: errorRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 6

                Text {
                    id: errorText
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - errorCloseBtn.width - parent.spacing
                    text: root._localizedErrorMessage(errorMessage)
                    color: Theme.colors.destructive
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                IconButton {
                    id: errorCloseBtn
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "x"
                    iconSize: 14
                    btnSize: 24
                    tooltip: qsTr("关闭提示")
                    background: Rectangle {
                        anchors.fill: parent
                        radius: Theme.radius.sm
                        color: parent.hovered ? Qt.rgba(Theme.colors.destructive.r, Theme.colors.destructive.g, Theme.colors.destructive.b, 0.18) : "transparent"
                    }
                    onClicked: {
                        root.errorTipDismissed = true
                        root._syncTipDismissState()
                    }
                }
            }
        }
    }
    
    Component.onCompleted: {
        failedTipDismissed = persistedFailedTipDismissed
        errorTipDismissed = persistedErrorTipDismissed
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
}
