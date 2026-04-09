import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import EnhanceVision.Controllers
import "../styles"
import "../controls"
import "../components"

Rectangle {
    id: root

    signal goBack()

    color: Theme.colors.background
    property var pauseModeSwitchBlockers: []
    property bool pauseModeBlockerDialogVisible: false

    function totalBlockedCount(statusKey) {
        var total = 0
        for (var i = 0; i < pauseModeSwitchBlockers.length; ++i) {
            total += pauseModeSwitchBlockers[i][statusKey] || 0
        }
        return total
    }

    function openPauseModeBlockerDialog(blockers) {
        pauseModeSwitchBlockers = blockers || []
        pauseModeBlockerDialogVisible = pauseModeSwitchBlockers.length > 0
    }

    function closePauseModeBlockerDialog() {
        pauseModeBlockerDialogVisible = false
    }

    function hasCacheClearSummary() {
        return SettingsController.lastCacheClearSummary
            && SettingsController.lastCacheClearSummary.reason !== undefined
            && SettingsController.lastCacheClearSummary.reason !== ""
    }

    function cacheViewerImpactText(viewerImpact) {
        if (viewerImpact === "closed") {
            return qsTr("已关闭")
        }
        if (viewerImpact === "sync") {
            return qsTr("自动切换或关闭")
        }
        return qsTr("未关闭")
    }

    function cacheReasonText(reason) {
        if (reason === "cache-ai-image") {
            return qsTr("AI 推理图像")
        }
        if (reason === "cache-ai-video") {
            return qsTr("AI 推理视频")
        }
        if (reason === "cache-shader-image") {
            return qsTr("Shader 图像")
        }
        if (reason === "cache-shader-video") {
            return qsTr("Shader 视频")
        }
        if (reason === "cache-thumbnails") {
            return qsTr("缩略图缓存")
        }
        if (reason === "cache-logs") {
            return qsTr("日志文件")
        }
        if (reason === "cache-all") {
            return qsTr("全部缓存")
        }
        return qsTr("缓存")
    }

    function formatCacheClearSummary() {
        if (!hasCacheClearSummary()) {
            return ""
        }

        var summary = SettingsController.lastCacheClearSummary
        return qsTr("%1 | 移除文件 %2 个 | 移除消息 %3 张 | 取消任务 %4 个 | 影响会话 %5 个 | 查看器：%6")
            .arg(cacheReasonText(summary.reason || ""))
            .arg(summary.removedFiles || 0)
            .arg(summary.removedMessages || 0)
            .arg(summary.cancelledTasks || 0)
            .arg(summary.sessionsAffected || 0)
            .arg(cacheViewerImpactText(summary.viewerImpact || "unchanged"))
    }

    function hasResidualCacheData() {
        if (!hasCacheClearSummary()) {
            return false
        }
        var summary = SettingsController.lastCacheClearSummary
        return summary.hasResidualData || false
    }

    function isCacheClearSuccessful() {
        if (!hasCacheClearSummary()) {
            return true
        }
        return SettingsController.lastCacheClearSummary.success || false
    }

    function cacheClearStatusTitle() {
        if (!hasCacheClearSummary()) {
            return ""
        }

        if (hasResidualCacheData()) {
            return qsTr("最近一次清理存在残留数据")
        }
        if (!isCacheClearSuccessful()) {
            return qsTr("最近一次清理未完成，请重试")
        }
        return qsTr("最近一次清理已完成")
    }

    function formatResidualEntryList() {
        if (!hasResidualCacheData()) {
            return ""
        }

        var summary = SettingsController.lastCacheClearSummary
        var entries = summary.residualEntries || []
        var lines = []
        var maxLines = Math.min(entries.length, 3)
        for (var i = 0; i < maxLines; i++) {
            var entry = entries[i]
            if (!entry) continue
            lines.push(qsTr("• %1（%2 个文件，%3）")
                .arg(entry.path || "")
                .arg(entry.files || 0)
                .arg(SettingsController.formatSize(entry.bytes || 0)))
        }
        if (entries.length > maxLines) {
            lines.push(qsTr("• 其余 %1 项请在日志中查看").arg(entries.length - maxLines))
        }
        return lines.join("\n")
    }

    function cacheClearGuidanceText() {
        var summary = SettingsController.lastCacheClearSummary
        if (hasResidualCacheData()) {
            return qsTr("仍有 %1 个文件（%2）未被删除，常见原因是文件被占用或权限不足。\n建议：\n1. 关闭正在播放/预览相关文件的窗口后重试清理\n2. 关闭占用该路径的外部程序（播放器、资源管理器、杀毒软件）\n3. 检查目录写权限后再次清理")
                .arg(summary.residualFiles || 0)
                .arg(SettingsController.formatSize(summary.residualBytes || 0))
        }
        if (!isCacheClearSuccessful()) {
            return qsTr("未检测到残留文件，但会话数据同步未完成。请重试清理；若仍出现，请重启应用后再次执行。")
        }
        return ""
    }

    function buildProcessingCacheClearMessage(title, fileCount, sizeText, path, pipelineLabel, mediaLabel) {
        return qsTr("确定要清理【%1】吗？\n\n清理详情：\n• 命中数量：%2 个%3文件\n• 占用空间：%4\n• 存储位置：%5\n\n此操作将：\n• 删除磁盘上的 %6处理结果文件\n• 取消并移除命中的等待中、处理中、暂停中、待恢复文件\n• 将消息卡片和放大查看器缩略图实时同步\n• 若当前正在查看被清理文件，会自动切换到剩余文件或关闭查看器\n• 不会删除用户原始文件\n\n清理后如需恢复结果，需要重新执行对应处理。")
            .arg(title)
            .arg(fileCount)
            .arg(mediaLabel)
            .arg(sizeText)
            .arg(path)
            .arg(pipelineLabel)
    }

    function buildLogClearMessage(title, sizeText) {
        return qsTr("确定要清理【%1】吗？\n\n清理详情：\n• 占用空间：%2\n\n此操作将：\n• 删除运行日志和崩溃日志文件\n• 不会影响消息卡片、任务状态或放大查看器")
            .arg(title)
            .arg(sizeText)
    }

    function buildThumbnailClearMessage(title, fileCount, sizeText, path) {
        return qsTr("确定要清理【%1】吗？\n\n清理详情：\n• 缩略图缓存：%2 个\n• 占用空间：%3\n• 存储位置：%4\n\n此操作将：\n• 清空缩略图磁盘缓存和元数据记录\n• 不会删除消息卡片、不取消任务、不关闭查看器\n• 后续打开文件时会自动重新生成缩略图")
            .arg(title)
            .arg(fileCount)
            .arg(sizeText)
            .arg(path)
    }

    function buildClearAllMessage(totalFiles) {
        return qsTr("确定要清理所有可清理数据吗？\n\n清理详情：\n• AI推理图像：%1 个文件（%2）\n• AI推理视频：%3 个文件（%4）\n• Shader图像：%5 个文件（%6）\n• Shader视频：%7 个文件（%8）\n• 日志文件：%9\n• 缩略图缓存：%10 个文件（%11）\n\n总计：%12 个条目，共 %13\n\n此操作将：\n• 删除所有 AI/Shader 处理结果文件\n• 取消并移除所有等待中、处理中、暂停中、待恢复消息卡片\n• 删除日志文件与缩略图缓存\n• 清空所有会话中的消息记录，但保留会话标签\n• 关闭消息查看器、待处理查看器及最小化停靠项\n• 不会删除用户原始文件")
            .arg(SettingsController.aiImageFileCount)
            .arg(SettingsController.formatSize(SettingsController.aiImageSize))
            .arg(SettingsController.aiVideoFileCount)
            .arg(SettingsController.formatSize(SettingsController.aiVideoSize))
            .arg(SettingsController.shaderImageFileCount)
            .arg(SettingsController.formatSize(SettingsController.shaderImageSize))
            .arg(SettingsController.shaderVideoFileCount)
            .arg(SettingsController.formatSize(SettingsController.shaderVideoSize))
            .arg(SettingsController.formatSize(SettingsController.logSize))
            .arg(SettingsController.thumbnailCacheCount)
            .arg(SettingsController.formatSize(SettingsController.thumbnailDiskSize))
            .arg(totalFiles)
            .arg(SettingsController.formatSize(SettingsController.totalCacheSize))
    }

    function tryChangePauseMode(targetMode) {
        if (SettingsController.pauseMode === targetMode) {
            return
        }

        var blockers = typeof taskRecoveryController !== "undefined"
            && taskRecoveryController
            && taskRecoveryController.getPauseModeSwitchBlockers
            ? taskRecoveryController.getPauseModeSwitchBlockers()
            : (typeof processingController !== "undefined"
            && processingController
            && processingController.getPauseModeSwitchBlockers
            ? processingController.getPauseModeSwitchBlockers()
            : [])

        if (blockers.length > 0) {
            openPauseModeBlockerDialog(blockers)
            return
        }

        SettingsController.pauseMode = targetMode
    }

    FolderDialog {
        id: customDataPathDialog
        title: qsTr("选择应用数据目录")
        currentFolder: SettingsController.customDataPath !== "" ? "file:///" + SettingsController.customDataPath : StandardPaths.writableLocation(StandardPaths.HomeLocation)
        onAccepted: {
            var path = selectedFolder.toString().replace("file:///", "")
            SettingsController.customDataPath = path
        }
    }

    FolderDialog {
        id: defaultSavePathDialog
        title: qsTr("选择默认保存路径")
        currentFolder: "file:///" + SettingsController.defaultSavePath
        onAccepted: {
            var path = selectedFolder.toString().replace("file:///", "")
            SettingsController.defaultSavePath = path
        }
    }

    Dialog {
        id: confirmClearDialog
        anchors.fill: parent
        property string clearType: ""
        property string clearTitle: ""
        property string clearMessage: ""

        function showClearDialog(type, title, message) {
            clearType = type
            clearTitle = title
            clearMessage = message
            showDialog(clearTitle, clearMessage, Dialog.DialogType.Warning)
        }

        onPrimaryButtonClicked: {
            var success = false
            if (clearType === "aiImage") {
                success = SettingsController.clearAIImageData()
            } else if (clearType === "aiVideo") {
                success = SettingsController.clearAIVideoData()
            } else if (clearType === "shaderImage") {
                success = SettingsController.clearShaderImageData()
            } else if (clearType === "shaderVideo") {
                success = SettingsController.clearShaderVideoData()
            } else if (clearType === "logs") {
                success = SettingsController.clearLogs()
            } else if (clearType === "thumbnails") {
                success = SettingsController.clearThumbnailCache()
            } else if (clearType === "all") {
                success = SettingsController.clearAllCache()
            }
        }
    }

    Item {
        id: pauseModeBlockerOverlay
        anchors.fill: parent
        z: 100
        visible: opacity > 0
        opacity: root.pauseModeBlockerDialogVisible ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.isDark ? Qt.rgba(2 / 255, 8 / 255, 20 / 255, 0.78) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.26)
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.pauseModeBlockerDialogVisible
            onClicked: root.closePauseModeBlockerDialog()
        }

        Rectangle {
            id: pauseModeBlockerCard
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 760)
            implicitHeight: blockerDialogColumn.implicitHeight + 44
            height: Math.min(implicitHeight, parent.height - 48)
            radius: Theme.radius.xxl
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.isDark ? Qt.rgba(91 / 255, 141 / 255, 239 / 255, 0.28) : Qt.rgba(0 / 255, 47 / 255, 167 / 255, 0.14)
            scale: root.pauseModeBlockerDialogVisible ? 1 : 0.94
            y: root.pauseModeBlockerDialogVisible ? 0 : 12

            Behavior on scale {
                NumberAnimation { duration: Theme.animation.slow; easing.type: Easing.OutCubic }
            }

            Behavior on y {
                NumberAnimation { duration: Theme.animation.slow; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: Math.min(parent.height * 0.4, 180)
                radius: parent.radius
                color: Theme.isDark ? Qt.rgba(91 / 255, 141 / 255, 239 / 255, 0.08) : Qt.rgba(0 / 255, 47 / 255, 167 / 255, 0.04)
            }

            ColumnLayout {
                id: blockerDialogColumn
                anchors.fill: parent
                anchors.margins: 22
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Rectangle {
                        width: 44
                        height: 44
                        radius: 14
                        color: Theme.colors.warningSubtleBg
                        border.width: 1
                        border.color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.24)

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("alert-triangle")
                            iconSize: 20
                            color: Theme.colors.warning
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: qsTr("暂时无法切换任务控制模式")
                            color: Theme.colors.foreground
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.totalBlockedCount("recoverableCount") > 0
                                ? qsTr("存在待恢复消息卡片，需先完成恢复决策后才可切换模式。")
                                : qsTr("以下会话标签中仍有未完成消息卡片，请在全部结束后再切换模式。")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radius.lg
                    color: Theme.isDark ? Qt.rgba(30 / 255, 86 / 255, 208 / 255, 0.12) : Qt.rgba(0 / 255, 47 / 255, 167 / 255, 0.06)
                    border.width: 1
                    border.color: Theme.isDark ? Qt.rgba(91 / 255, 141 / 255, 239 / 255, 0.22) : Qt.rgba(0 / 255, 47 / 255, 167 / 255, 0.12)
                    implicitHeight: blockerBannerColumn.implicitHeight + 18

                    ColumnLayout {
                        id: blockerBannerColumn
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("涉及 %1 个会话标签，共 %2 张消息卡片被拦截。").arg(root.pauseModeSwitchBlockers.length).arg(root.totalBlockedCount("totalCount"))
                            color: Theme.colors.foreground
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                visible: root.totalBlockedCount("recoverableCount") > 0
                                radius: Theme.radius.full
                                color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.12)
                                border.width: 1
                                border.color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.18)
                                implicitHeight: recoverableChipText.implicitHeight + 8
                                implicitWidth: recoverableChipText.implicitWidth + 14

                                Text {
                                    id: recoverableChipText
                                    anchors.centerIn: parent
                                    text: qsTr("待恢复 %1").arg(root.totalBlockedCount("recoverableCount"))
                                    color: Theme.colors.primary
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                visible: root.totalBlockedCount("processingCount") > 0
                                radius: Theme.radius.full
                                color: Theme.colors.primarySubtle
                                implicitHeight: processingChipText.implicitHeight + 8
                                implicitWidth: processingChipText.implicitWidth + 14

                                Text {
                                    id: processingChipText
                                    anchors.centerIn: parent
                                    text: qsTr("处理中 %1").arg(root.totalBlockedCount("processingCount"))
                                    color: Theme.colors.primary
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                visible: root.totalBlockedCount("pendingCount") > 0
                                radius: Theme.radius.full
                                color: Theme.colors.warningSubtleBg
                                implicitHeight: pendingChipText.implicitHeight + 8
                                implicitWidth: pendingChipText.implicitWidth + 14

                                Text {
                                    id: pendingChipText
                                    anchors.centerIn: parent
                                    text: qsTr("等待处理 %1").arg(root.totalBlockedCount("pendingCount"))
                                    color: Theme.colors.warning
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                visible: root.totalBlockedCount("pausedCount") > 0
                                radius: Theme.radius.full
                                color: Theme.isDark ? Qt.rgba(232 / 255, 237 / 255, 245 / 255, 0.08) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.06)
                                border.width: 1
                                border.color: Theme.isDark ? Qt.rgba(232 / 255, 237 / 255, 245 / 255, 0.12) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.08)
                                implicitHeight: pausedChipText.implicitHeight + 8
                                implicitWidth: pausedChipText.implicitWidth + 14

                                Text {
                                    id: pausedChipText
                                    anchors.centerIn: parent
                                    text: qsTr("暂停 %1").arg(root.totalBlockedCount("pausedCount"))
                                    color: Theme.colors.foreground
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                ScrollView {
                    id: pauseModeBlockerScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: Math.min(pauseModeBlockerListColumn.implicitHeight, 320)
                    clip: true

                    ColumnLayout {
                        id: pauseModeBlockerListColumn
                        width: pauseModeBlockerScroll.availableWidth
                        spacing: 10

                        Repeater {
                            model: root.pauseModeSwitchBlockers

                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                radius: Theme.radius.lg
                                color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(1, 1, 1, 0.92)
                                border.width: 1
                                border.color: Theme.colors.border
                                implicitHeight: blockerSessionColumn.implicitHeight + 18

                                ColumnLayout {
                                    id: blockerSessionColumn
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 10

                                        Rectangle {
                                            width: 30
                                            height: 30
                                            radius: 10
                                            color: Theme.colors.primarySubtle

                                            ColoredIcon {
                                                anchors.centerIn: parent
                                                source: Theme.icon("folder")
                                                iconSize: 15
                                                color: Theme.colors.primary
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.sessionName || qsTr("当前会话")
                                                color: Theme.colors.foreground
                                                font.pixelSize: 14
                                                font.weight: Font.DemiBold
                                                wrapMode: Text.Wrap
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.recoverableCount || 0) > 0
                                                    ? qsTr("%1 张消息卡片待恢复").arg(modelData.totalCount || 0)
                                                    : qsTr("%1 张消息卡片未完成").arg(modelData.totalCount || 0)
                                                color: Theme.colors.mutedForeground
                                                font.pixelSize: 11
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Rectangle {
                                            visible: (modelData.recoverableCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.12)
                                            border.width: 1
                                            border.color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.18)
                                            implicitHeight: blockerRecoverableText.implicitHeight + 8
                                            implicitWidth: blockerRecoverableText.implicitWidth + 14

                                            Text {
                                                id: blockerRecoverableText
                                                anchors.centerIn: parent
                                                text: qsTr("待恢复 %1").arg(modelData.recoverableCount || 0)
                                                color: Theme.colors.primary
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Rectangle {
                                            visible: (modelData.processingCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Theme.colors.primarySubtle
                                            implicitHeight: blockerProcessingText.implicitHeight + 8
                                            implicitWidth: blockerProcessingText.implicitWidth + 14

                                            Text {
                                                id: blockerProcessingText
                                                anchors.centerIn: parent
                                                text: qsTr("处理中 %1").arg(modelData.processingCount || 0)
                                                color: Theme.colors.primary
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Rectangle {
                                            visible: (modelData.pendingCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Theme.colors.warningSubtleBg
                                            implicitHeight: blockerPendingText.implicitHeight + 8
                                            implicitWidth: blockerPendingText.implicitWidth + 14

                                            Text {
                                                id: blockerPendingText
                                                anchors.centerIn: parent
                                                text: qsTr("等待处理 %1").arg(modelData.pendingCount || 0)
                                                color: Theme.colors.warning
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Rectangle {
                                            visible: (modelData.pausedCount || 0) > 0
                                            radius: Theme.radius.full
                                            color: Theme.isDark ? Qt.rgba(232 / 255, 237 / 255, 245 / 255, 0.08) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.06)
                                            border.width: 1
                                            border.color: Theme.isDark ? Qt.rgba(232 / 255, 237 / 255, 245 / 255, 0.12) : Qt.rgba(10 / 255, 22 / 255, 40 / 255, 0.08)
                                            implicitHeight: blockerPausedText.implicitHeight + 8
                                            implicitWidth: blockerPausedText.implicitWidth + 14

                                            Text {
                                                id: blockerPausedText
                                                anchors.centerIn: parent
                                                text: qsTr("暂停 %1").arg(modelData.pausedCount || 0)
                                                color: Theme.colors.foreground
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }

                                        Item { Layout.fillWidth: true }
                                    }

                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: blockerFooterText.implicitHeight + 16
                    radius: Theme.radius.lg
                    color: Theme.colors.warningSubtleBg
                    border.width: 1
                    border.color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.18)

                    Text {
                        id: blockerFooterText
                        anchors.fill: parent
                        anchors.margins: 10
                        text: root.totalBlockedCount("recoverableCount") > 0
                            ? qsTr("请先在启动恢复弹窗中选择恢复关闭前状态，或全部标记为失败。")
                            : qsTr("请先完成、删除或继续处理这些消息卡片。")
                        color: Theme.colors.foreground
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("当前已拦截切换。")
                        color: Theme.colors.mutedForeground
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }

                    Button {
                        text: qsTr("我知道了")
                        variant: "primary"
                        iconName: "check"
                        onClicked: root.closePauseModeBlockerDialog()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            IconButton {
                iconName: "arrow-left"
                iconSize: 18
                tooltip: qsTr("返回")
                onClicked: root.goBack()
            }

            Text {
                text: qsTr("设置")
                color: Theme.colors.foreground
                font.pixelSize: 22
                font.weight: Font.Bold
            }
        }

        ScrollView {
            id: settingsScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                id: settingsColumn
                spacing: 16

                Binding {
                    target: settingsColumn
                    property: "width"
                    value: settingsColumn.parent ? settingsColumn.parent.width : 400
                    when: settingsColumn.parent
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: appearanceCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: appearanceCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("monitor"); color: Theme.colors.icon }
                            Text { text: qsTr("外观"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("主题"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }

                            Row {
                                spacing: 4

                                Rectangle {
                                    width: darkRow.implicitWidth + 24; height: 32
                                    radius: Theme.radius.md
                                    color: Theme.isDark ? Theme.colors.primary : "transparent"
                                    border.width: Theme.isDark ? 0 : 1
                                    border.color: Theme.colors.border

                                    Row {
                                        id: darkRow
                                        anchors.centerIn: parent; spacing: 6
                                        ColoredIcon { anchors.verticalCenter: parent.verticalCenter; iconSize: 14; source: Theme.icon("moon"); color: Theme.isDark ? Theme.colors.textOnPrimary : Theme.colors.icon }
                                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("暗色"); font.pixelSize: 12; font.weight: Font.Medium; color: Theme.isDark ? Theme.colors.textOnPrimary : Theme.colors.foreground }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { Theme.setDark(true) } }
                                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                                }

                                Rectangle {
                                    width: lightRow.implicitWidth + 24; height: 32
                                    radius: Theme.radius.md
                                    color: !Theme.isDark ? Theme.colors.primary : "transparent"
                                    border.width: !Theme.isDark ? 0 : 1
                                    border.color: Theme.colors.border

                                    Row {
                                        id: lightRow
                                        anchors.centerIn: parent; spacing: 6
                                        ColoredIcon { anchors.verticalCenter: parent.verticalCenter; iconSize: 14; source: Theme.icon("sun"); color: !Theme.isDark ? Theme.colors.textOnPrimary : Theme.colors.icon }
                                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("亮色"); font.pixelSize: 12; font.weight: Font.Medium; color: !Theme.isDark ? Theme.colors.textOnPrimary : Theme.colors.foreground }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { Theme.setDark(false) } }
                                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("语言"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }

                            ComboBox {
                                model: [qsTr("简体中文"), "English"]
                                currentIndex: SettingsController.language === "zh_CN" ? 0 : 1
                                onActivated: function(index) {
                                    currentIndex = index
                                    var lang = currentIndex === 0 ? "zh_CN" : "en_US"
                                    Theme.setLanguage(lang)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: dataStorageCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: dataStorageCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("database"); color: Theme.colors.icon }
                            Text { text: qsTr("数据存储"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text { text: qsTr("应用数据目录"); color: Theme.colors.foreground; font.pixelSize: 13 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                TextField {
                                    id: customDataPathField
                                    Layout.fillWidth: true
                                    text: SettingsController.effectiveDataPath()
                                    readOnly: true
                                    size: "sm"
                                }

                                Button {
                                    text: qsTr("浏览...")
                                    variant: "secondary"
                                    size: "sm"
                                    onClicked: customDataPathDialog.open()
                                }

                                Button {
                                    text: qsTr("重置")
                                    variant: "ghost"
                                    size: "sm"
                                    onClicked: SettingsController.customDataPath = ""
                                    visible: SettingsController.customDataPath !== ""
                                }
                            }

                            Text {
                                text: qsTr("存储 AI/Shader 处理结果等应用数据")
                                color: Theme.colors.mutedForeground
                                font.pixelSize: 11
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text { text: qsTr("默认导出路径"); color: Theme.colors.foreground; font.pixelSize: 13 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                TextField {
                                    id: savePathField
                                    Layout.fillWidth: true
                                    text: SettingsController.defaultSavePath
                                    readOnly: true
                                    size: "sm"
                                }

                                Button {
                                    text: qsTr("浏览...")
                                    variant: "secondary"
                                    size: "sm"
                                    onClicked: defaultSavePathDialog.open()
                                }
                            }

                            Text {
                                text: qsTr("导出处理结果时的默认保存位置")
                                color: Theme.colors.mutedForeground
                                font.pixelSize: 11
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("自动保存结果"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }

                            Switch {
                                checked: SettingsController.autoSaveResult
                                onCheckedChanged: SettingsController.autoSaveResult = checked
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: reprocessCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: reprocessCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("refresh-cw"); color: Theme.colors.icon }
                            Text { text: qsTr("处理恢复"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        Text { 
                            text: qsTr("检测到未完成任务时，应用会统一进入启动恢复流程，并先锁定模式切换。")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radius.md
                            color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.03) : Qt.rgba(255 / 255, 255 / 255, 255 / 255, 0.92)
                            border.width: 1
                            border.color: Theme.colors.border
                            implicitHeight: recoveryStrategyColumn.implicitHeight + 20

                            ColumnLayout {
                                id: recoveryStrategyColumn
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("当前策略")
                                    color: Theme.colors.foreground
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("1. 上次退出时仍未完成的消息卡片会先显示为“待恢复”。\n2. 启动后会弹出统一恢复对话框，由您选择恢复关闭前状态，或全部标记为失败。\n3. 在恢复决策完成前，以及仍存在待恢复、处理中、等待中、暂停中的消息卡片时，都不允许切换任务控制模式。")
                                    color: Theme.colors.mutedForeground
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: tipText.implicitHeight + 16
                            radius: Theme.radius.sm
                            color: Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.1)
                            
                            Text {
                                id: tipText
                                anchors.fill: parent
                                anchors.margins: 8
                                text: qsTr("应用再次启动时，会统一弹出恢复对话框，让您选择恢复关闭前状态，或全部标记为失败。")
                                color: Theme.colors.primary
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: taskControlCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: taskControlCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("pause-circle"); color: Theme.colors.icon }
                            Text { text: qsTr("任务控制"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        Text { 
                            text: qsTr("设置暂停处理任务时的行为模式")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: mode0Row.implicitHeight + 16
                                radius: Theme.radius.md
                                color: SettingsController.pauseMode === 0 ? Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.1) : "transparent"
                                border.width: SettingsController.pauseMode === 0 ? 1 : 0
                                border.color: Theme.colors.primary

                                RowLayout {
                                    id: mode0Row
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12

                                    Rectangle {
                                        width: 18; height: 18; radius: 9
                                        color: SettingsController.pauseMode === 0 ? Theme.colors.primary : "transparent"
                                        border.width: SettingsController.pauseMode === 0 ? 0 : 2
                                        border.color: Theme.colors.border

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 6; height: 6; radius: 3
                                            color: Theme.colors.textOnPrimary
                                            visible: SettingsController.pauseMode === 0
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text { text: qsTr("单任务暂停"); color: Theme.colors.foreground; font.pixelSize: 13; font.weight: Font.Medium }
                                        Text { text: qsTr("暂停后继续处理其他消息"); color: Theme.colors.mutedForeground; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.tryChangePauseMode(0)
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: mode1Row.implicitHeight + 16
                                radius: Theme.radius.md
                                color: SettingsController.pauseMode === 1 ? Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.1) : "transparent"
                                border.width: SettingsController.pauseMode === 1 ? 1 : 0
                                border.color: Theme.colors.primary

                                RowLayout {
                                    id: mode1Row
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12

                                    Rectangle {
                                        width: 18; height: 18; radius: 9
                                        color: SettingsController.pauseMode === 1 ? Theme.colors.primary : "transparent"
                                        border.width: SettingsController.pauseMode === 1 ? 0 : 2
                                        border.color: Theme.colors.border

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 6; height: 6; radius: 3
                                            color: Theme.colors.textOnPrimary
                                            visible: SettingsController.pauseMode === 1
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text { text: qsTr("顺序暂停"); color: Theme.colors.foreground; font.pixelSize: 13; font.weight: Font.Medium }
                                        Text { text: qsTr("暂停后阻塞整个队列（默认）"); color: Theme.colors.mutedForeground; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.tryChangePauseMode(1)
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: mode2Row.implicitHeight + 16
                                radius: Theme.radius.md
                                color: SettingsController.pauseMode === 2 ? Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, 0.1) : "transparent"
                                border.width: SettingsController.pauseMode === 2 ? 1 : 0
                                border.color: Theme.colors.primary

                                RowLayout {
                                    id: mode2Row
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 12

                                    Rectangle {
                                        width: 18; height: 18; radius: 9
                                        color: SettingsController.pauseMode === 2 ? Theme.colors.primary : "transparent"
                                        border.width: SettingsController.pauseMode === 2 ? 0 : 2
                                        border.color: Theme.colors.border

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 6; height: 6; radius: 3
                                            color: Theme.colors.textOnPrimary
                                            visible: SettingsController.pauseMode === 2
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text { text: qsTr("自由选择"); color: Theme.colors.foreground; font.pixelSize: 13; font.weight: Font.Medium }
                                        Text { text: qsTr("默认暂停发送的消息，后续点击继续按钮手动选择处理消息的顺序"); color: Theme.colors.mutedForeground; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.tryChangePauseMode(2)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: audioCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: audioCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("volume"); color: Theme.colors.icon }
                            Text { text: qsTr("音频"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("默认音量"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }

                            Slider {
                                id: volumeSlider
                                from: 0
                                to: 100
                                value: SettingsController.volume
                                onMoved: SettingsController.volume = Math.round(value)
                                Layout.fillWidth: true
                            }

                            Text {
                                text: SettingsController.volume + "%"
                                color: Theme.colors.foreground
                                font.pixelSize: 13
                                Layout.preferredWidth: 40
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: videoCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: videoCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("video"); color: Theme.colors.icon }
                            Text { text: qsTr("视频"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            ColumnLayout {
                                spacing: 2
                                Text { text: qsTr("开/切自动播放"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
                                Text { 
                                    text: qsTr("点击视频进行放大查看（嵌入式和独立式）和点击左右导航按钮切换到视频时自动开始播放")
                                    color: Theme.colors.mutedForeground
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Switch {
                                id: videoAutoPlaySwitch
                                property bool updating: false
                                checked: SettingsController.videoAutoPlay
                                onToggled: {
                                    if (!updating) {
                                        SettingsController.videoAutoPlay = checked
                                    }
                                }

                                Connections {
                                    target: SettingsController
                                    function onVideoAutoPlayChanged() {
                                        videoAutoPlaySwitch.updating = true
                                        videoAutoPlaySwitch.checked = SettingsController.videoAutoPlay
                                        videoAutoPlaySwitch.updating = false
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            ColumnLayout {
                                spacing: 2
                                Text { text: qsTr("源/结自动播放"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
                                Text { 
                                    text: qsTr("放大查看（嵌入式和独立式）切换源件/结果时自动播放")
                                    color: Theme.colors.mutedForeground
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Switch {
                                id: videoAutoPlayOnSwitchSwitch
                                property bool updating: false
                                checked: SettingsController.videoAutoPlayOnSwitch
                                onToggled: {
                                    if (!updating) {
                                        SettingsController.videoAutoPlayOnSwitch = checked
                                    }
                                }

                                Connections {
                                    target: SettingsController
                                    function onVideoAutoPlayOnSwitchChanged() {
                                        videoAutoPlayOnSwitchSwitch.updating = true
                                        videoAutoPlayOnSwitchSwitch.checked = SettingsController.videoAutoPlayOnSwitch
                                        videoAutoPlayOnSwitchSwitch.updating = false
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            ColumnLayout {
                                spacing: 2
                                Text { text: qsTr("源/结恢复进度"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
                                Text { 
                                    text: qsTr("放大查看（嵌入式和独立式）切换源件/结果时恢复播放进度")
                                    color: Theme.colors.mutedForeground
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Switch {
                                id: videoRestorePositionSwitch
                                property bool updating: false
                                checked: SettingsController.videoRestorePosition
                                onToggled: {
                                    if (!updating) {
                                        SettingsController.videoRestorePosition = checked
                                    }
                                }

                                Connections {
                                    target: SettingsController
                                    function onVideoRestorePositionChanged() {
                                        videoRestorePositionSwitch.updating = true
                                        videoRestorePositionSwitch.checked = SettingsController.videoRestorePosition
                                        videoRestorePositionSwitch.updating = false
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: cacheCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: cacheCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 16

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColoredIcon { iconSize: 18; source: Theme.icon("trash-2"); color: Theme.colors.icon }
                            Text { text: qsTr("缓存管理"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }

                            Item { Layout.fillWidth: true }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                implicitWidth: totalSizeRow.implicitWidth + 16
                                implicitHeight: 28
                                radius: Theme.radius.sm
                                color: Theme.colors.primarySubtle

                                Row {
                                    id: totalSizeRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    ColoredIcon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        iconSize: 14
                                        source: Theme.icon("database")
                                        color: Theme.colors.primary
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("可清理: %1").arg(SettingsController.formatSize(SettingsController.totalCacheSize))
                                        color: Theme.colors.primary
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                    }
                                }
                            }

                            Button {
                                id: refreshButton
                                text: qsTr("刷新")
                                variant: "ghost"
                                size: "sm"
                                iconName: "refresh-cw"
                                onClicked: {
                                    try {
                                        SettingsController.refreshDataSize()
                                        refreshToast.show(true)
                                    } catch (e) {
                                        refreshToast.show(false)
                                    }
                                }
                            }

                            Rectangle {
                                id: refreshToast
                                implicitWidth: refreshToastRow.implicitWidth + 12
                                implicitHeight: 24
                                radius: Theme.radius.sm
                                color: refreshToast.isSuccess ? Theme.colors.successSubtle : Theme.colors.destructiveSubtle
                                opacity: 0
                                visible: opacity > 0

                                property bool isSuccess: true

                                function show(success) {
                                    isSuccess = success
                                    opacity = 1
                                    refreshToastTimer.restart()
                                }

                                Row {
                                    id: refreshToastRow
                                    anchors.centerIn: parent
                                    spacing: 4

                                    ColoredIcon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        iconSize: 12
                                        source: refreshToast.isSuccess ? Theme.icon("check") : Theme.icon("x")
                                        color: refreshToast.isSuccess ? Theme.colors.success : Theme.colors.destructive
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: refreshToast.isSuccess ? qsTr("已刷新") : qsTr("刷新失败")
                                        color: refreshToast.isSuccess ? Theme.colors.success : Theme.colors.destructive
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                    }
                                }

                                Timer {
                                    id: refreshToastTimer
                                    interval: 2000
                                    onTriggered: refreshToast.opacity = 0
                                }

                                Behavior on opacity {
                                    NumberAnimation { duration: 150 }
                                }

                                Behavior on color {
                                    ColorAnimation { duration: 150 }
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            visible: root.hasCacheClearSummary()
                            radius: Theme.radius.md
                            color: root.isCacheClearSuccessful()
                                ? Theme.colors.successSubtle
                                : Theme.colors.destructiveSubtle
                            border.width: 1
                            border.color: root.isCacheClearSuccessful()
                                ? Qt.rgba(Theme.colors.success.r, Theme.colors.success.g, Theme.colors.success.b, 0.24)
                                : Qt.rgba(Theme.colors.destructive.r, Theme.colors.destructive.g, Theme.colors.destructive.b, 0.24)
                            implicitHeight: cacheSummaryColumn.implicitHeight + 18

                            ColumnLayout {
                                id: cacheSummaryColumn
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    ColoredIcon {
                                        iconSize: 15
                                        source: root.isCacheClearSuccessful()
                                            ? Theme.icon("check-circle")
                                            : Theme.icon("x-circle")
                                        color: root.isCacheClearSuccessful()
                                            ? Theme.colors.success
                                            : Theme.colors.destructive
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.cacheClearStatusTitle()
                                        color: Theme.colors.foreground
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.formatCacheClearSummary()
                                    color: Theme.colors.mutedForeground
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: root.hasResidualCacheData()
                                    text: root.formatResidualEntryList()
                                    color: Theme.colors.destructive
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: root.hasResidualCacheData() || !root.isCacheClearSuccessful()
                                    text: root.cacheClearGuidanceText()
                                    color: Theme.colors.mutedForeground
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [
                                    {
                                        type: "aiImage",
                                        title: qsTr("AI推理 - 图像"),
                                        desc: qsTr("AI 超分辨率增强处理后的图像文件"),
                                        size: SettingsController.aiImageSize,
                                        fileCount: SettingsController.aiImageFileCount,
                                        icon: "cpu",
                                        accentColor: Theme.colors.primary
                                    },
                                    {
                                        type: "aiVideo",
                                        title: qsTr("AI推理 - 视频"),
                                        desc: qsTr("AI 超分辨率增强处理后的视频文件"),
                                        size: SettingsController.aiVideoSize,
                                        fileCount: SettingsController.aiVideoFileCount,
                                        icon: "video",
                                        accentColor: Theme.colors.primary
                                    },
                                    {
                                        type: "shaderImage",
                                        title: qsTr("Shader - 图像"),
                                        desc: qsTr("Shader 滤镜处理后的图像文件"),
                                        size: SettingsController.shaderImageSize,
                                        fileCount: SettingsController.shaderImageFileCount,
                                        icon: "image",
                                        accentColor: Theme.colors.success
                                    },
                                    {
                                        type: "shaderVideo",
                                        title: qsTr("Shader - 视频"),
                                        desc: qsTr("Shader 滤镜处理后的视频文件"),
                                        size: SettingsController.shaderVideoSize,
                                        fileCount: SettingsController.shaderVideoFileCount,
                                        icon: "film",
                                        accentColor: Theme.colors.warning
                                    },
                                    {
                                        type: "logs",
                                        title: qsTr("日志文件"),
                                        desc: qsTr("运行日志和崩溃日志"),
                                        size: SettingsController.logSize,
                                        fileCount: 0,
                                        icon: "file-text",
                                        accentColor: Theme.colors.mutedForeground
                                    },
                                    {
                                        type: "thumbnails",
                                        title: qsTr("缩略图缓存"),
                                        desc: qsTr("多媒体文件预览缩略图的磁盘缓存"),
                                        size: SettingsController.thumbnailDiskSize,
                                        fileCount: SettingsController.thumbnailCacheCount,
                                        icon: "image",
                                        accentColor: Theme.colors.primary
                                    }
                                ]

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 56
                                    radius: Theme.radius.md
                                    color: cacheItemMouse.containsMouse ? Theme.colors.accent : Theme.colors.surface
                                    border.width: 1
                                    border.color: cacheItemMouse.containsMouse ? Theme.colors.borderHover : Theme.colors.border

                                    Behavior on color { ColorAnimation { duration: 100 } }
                                    Behavior on border.color { ColorAnimation { duration: 100 } }

                                    MouseArea {
                                        id: cacheItemMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 12

                                        Rectangle {
                                            width: 32
                                            height: 32
                                            radius: Theme.radius.sm
                                            color: Qt.rgba(modelData.accentColor.r, modelData.accentColor.g, modelData.accentColor.b, 0.15)

                                            ColoredIcon {
                                                anchors.centerIn: parent
                                                iconSize: 16
                                                source: Theme.icon(modelData.icon)
                                                color: modelData.accentColor
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                text: modelData.title
                                                color: Theme.colors.foreground
                                                font.pixelSize: 13
                                                font.weight: Font.Medium
                                            }

                                            Text {
                                                text: modelData.desc
                                                color: Theme.colors.mutedForeground
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Text {
                                            text: SettingsController.formatSize(modelData.size)
                                            color: modelData.size > 0 ? Theme.colors.foreground : Theme.colors.mutedForeground
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                        }

                                        Button {
                                            text: qsTr("清理")
                                            variant: "ghost"
                                            size: "sm"
                                            iconName: "trash"
                                            enabled: modelData.size > 0
                                            onClicked: {
                                                var detailMsg = ""
                                                if (modelData.type === "aiImage") {
                                                    detailMsg = root.buildProcessingCacheClearMessage(
                                                        modelData.title,
                                                        modelData.fileCount,
                                                        SettingsController.formatSize(modelData.size),
                                                        SettingsController.getAIImagePath(),
                                                        qsTr("AI 推理"),
                                                        qsTr("图像"))
                                                } else if (modelData.type === "aiVideo") {
                                                    detailMsg = root.buildProcessingCacheClearMessage(
                                                        modelData.title,
                                                        modelData.fileCount,
                                                        SettingsController.formatSize(modelData.size),
                                                        SettingsController.getAIVideoPath(),
                                                        qsTr("AI 推理"),
                                                        qsTr("视频"))
                                                } else if (modelData.type === "shaderImage") {
                                                    detailMsg = root.buildProcessingCacheClearMessage(
                                                        modelData.title,
                                                        modelData.fileCount,
                                                        SettingsController.formatSize(modelData.size),
                                                        SettingsController.getShaderImagePath(),
                                                        qsTr("Shader"),
                                                        qsTr("图像"))
                                                } else if (modelData.type === "shaderVideo") {
                                                    detailMsg = root.buildProcessingCacheClearMessage(
                                                        modelData.title,
                                                        modelData.fileCount,
                                                        SettingsController.formatSize(modelData.size),
                                                        SettingsController.getShaderVideoPath(),
                                                        qsTr("Shader"),
                                                        qsTr("视频"))
                                                } else {
                                                    if (modelData.type === "logs") {
                                                        detailMsg = root.buildLogClearMessage(
                                                            modelData.title,
                                                            SettingsController.formatSize(modelData.size))
                                                    } else if (modelData.type === "thumbnails") {
                                                        detailMsg = root.buildThumbnailClearMessage(
                                                            modelData.title,
                                                            modelData.fileCount,
                                                            SettingsController.formatSize(modelData.size),
                                                            SettingsController.getThumbnailCachePath())
                                                    }
                                                }
                                                confirmClearDialog.showClearDialog(
                                                    modelData.type,
                                                    qsTr("确认清理 - %1").arg(modelData.title),
                                                    detailMsg
                                                )
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: bottomRow.implicitHeight + 12
                            radius: Theme.radius.md
                            color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.08)
                            border.width: 1
                            border.color: Qt.rgba(Theme.colors.warning.r, Theme.colors.warning.g, Theme.colors.warning.b, 0.2)

                            RowLayout {
                                id: bottomRow
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 8
                                anchors.topMargin: 6
                                anchors.bottomMargin: 6
                                spacing: 6

                                ColoredIcon {
                                    iconSize: 14
                                    source: Theme.icon("alert-triangle")
                                    color: Theme.colors.warning
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("清理后，数据不可恢复，谨慎清理。")
                                    color: Theme.colors.warning
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    verticalAlignment: Text.AlignVCenter
                                }

                                Button {
                                    id: clearAllButton
                                    text: qsTr("清理全部")
                                    variant: "destructive"
                                    size: "sm"
                                    iconName: "trash-2"
                                    enabled: SettingsController.totalCacheSize > 0
                                    onClicked: {
                                        var totalFiles = SettingsController.aiImageFileCount + 
                                                         SettingsController.aiVideoFileCount + 
                                                         SettingsController.shaderImageFileCount + 
                                                         SettingsController.shaderVideoFileCount +
                                                         SettingsController.thumbnailCacheCount
                                        confirmClearDialog.showClearDialog(
                                            "all",
                                            qsTr("确认清理全部数据"),
                                            root.buildClearAllMessage(totalFiles)
                                        )
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: aboutCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: aboutCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("info"); color: Theme.colors.icon }
                            Text { text: qsTr("关于"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            spacing: 12

                            Rectangle {
                                width: 44
                                height: 44
                                radius: 11
                                clip: true
                                color: "transparent"
                                
                                Image {
                                    anchors.fill: parent
                                    source: "qrc:/icons/app_icon.png"
                                    sourceSize.width: 44
                                    sourceSize.height: 44
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                }
                            }

                            ColumnLayout {
                                spacing: 2
                                Text { text: qsTr("EnhanceVision v0.1.0"); color: Theme.colors.foreground; font.pixelSize: 14; font.weight: Font.DemiBold }
                                Text { text: qsTr("图像处理与AI推理画质增强工具"); color: Theme.colors.mutedForeground; font.pixelSize: 12 }
                                Text { text: qsTr("Built with Qt6 + NCNN"); color: Theme.colors.mutedForeground; font.pixelSize: 11; opacity: 0.7 }
                            }
                        }
                    }
                }
            }
        }
    }



    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
