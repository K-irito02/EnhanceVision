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
            if (clearType === "ai") {
                success = SettingsController.clearAIProcessedData()
            } else if (clearType === "shaderImage") {
                success = SettingsController.clearShaderImageData()
            } else if (clearType === "shaderVideo") {
                success = SettingsController.clearShaderVideoData()
            } else if (clearType === "logs") {
                success = SettingsController.clearLogs()
            } else if (clearType === "all") {
                success = SettingsController.clearAllCache()
            }
            clearCacheToast.show(success ? qsTr("清理完成") : qsTr("清理失败"))
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
                                onCurrentIndexChanged: {
                                    var lang = currentIndex === 0 ? "zh_CN" : "en_US"
                                    SettingsController.language = lang
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
                                    text: SettingsController.customDataPath !== "" ? SettingsController.customDataPath : qsTr("使用系统默认路径")
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
                            text: qsTr("应用启动时自动恢复未完成的处理任务")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: qsTr("全部开启"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
                            Item { Layout.fillWidth: true }
                            Switch {
                                id: allSwitch
                                checked: SettingsController.autoReprocessAllEnabled
                                onToggled: {
                                    SettingsController.autoReprocessAllEnabled = checked
                                }
                                
                                Connections {
                                    target: SettingsController
                                    function onAutoReprocessAllEnabledChanged() {
                                        allSwitch.checked = SettingsController.autoReprocessAllEnabled
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: qsTr("Shader 模式"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
                            Item { Layout.fillWidth: true }
                            Switch {
                                id: shaderSwitch
                                checked: SettingsController.autoReprocessShaderEnabled
                                onToggled: {
                                    SettingsController.autoReprocessShaderEnabled = checked
                                }
                                
                                Connections {
                                    target: SettingsController
                                    function onAutoReprocessShaderEnabledChanged() {
                                        shaderSwitch.checked = SettingsController.autoReprocessShaderEnabled
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: qsTr("AI 推理模式"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }
                            Item { Layout.fillWidth: true }
                            Switch {
                                id: aiSwitch
                                checked: SettingsController.autoReprocessAIEnabled
                                onToggled: {
                                    SettingsController.autoReprocessAIEnabled = checked
                                }
                                
                                Connections {
                                    target: SettingsController
                                    function onAutoReprocessAIEnabledChanged() {
                                        aiSwitch.checked = SettingsController.autoReprocessAIEnabled
                                    }
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
                                text: qsTr("关闭后，应用中途关闭时的处理任务不会自动恢复，您可以手动点击\"重新处理\"按钮进行处理")
                                color: Theme.colors.primary
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
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
                                        source: Theme.icon("hard-drive")
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
                                    SettingsController.refreshDataSize()
                                    refreshToast.show()
                                }
                            }

                            Rectangle {
                                id: refreshToast
                                implicitWidth: refreshToastRow.implicitWidth + 12
                                implicitHeight: 24
                                radius: Theme.radius.sm
                                color: Theme.colors.successSubtle
                                opacity: 0
                                visible: opacity > 0

                                function show() {
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
                                        source: Theme.icon("check")
                                        color: Theme.colors.success
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: qsTr("已刷新")
                                        color: Theme.colors.success
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
                            }

                            Item { Layout.fillWidth: true }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [
                                    {
                                        type: "ai",
                                        title: qsTr("AI 处理结果"),
                                        desc: qsTr("AI 增强处理后的图像和视频文件"),
                                        size: SettingsController.aiProcessedSize,
                                        icon: "cpu",
                                        accentColor: Theme.colors.primary
                                    },
                                    {
                                        type: "shaderImage",
                                        title: qsTr("Shader 图像结果"),
                                        desc: qsTr("Shader 滤镜处理后的图像文件"),
                                        size: SettingsController.shaderImageSize,
                                        icon: "image",
                                        accentColor: Theme.colors.success
                                    },
                                    {
                                        type: "shaderVideo",
                                        title: qsTr("Shader 视频结果"),
                                        desc: qsTr("Shader 滤镜处理后的视频文件"),
                                        size: SettingsController.shaderVideoSize,
                                        icon: "film",
                                        accentColor: Theme.colors.warning
                                    },
                                    {
                                        type: "logs",
                                        title: qsTr("日志文件"),
                                        desc: qsTr("运行日志和崩溃日志"),
                                        size: SettingsController.logSize,
                                        icon: "file-text",
                                        accentColor: Theme.colors.mutedForeground
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
                                                confirmClearDialog.showClearDialog(
                                                    modelData.type,
                                                    qsTr("确认清理"),
                                                    qsTr("确定要清理 %1 吗？\n\n清理后需要重新处理才能恢复结果。").arg(modelData.title)
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
                                    source: Theme.icon("alert-circle")
                                    color: Theme.colors.warning
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("清理后可重新生成处理结果，但需要重新处理。")
                                    color: Theme.colors.warning
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    verticalAlignment: Text.AlignVCenter
                                }

                                Button {
                                    text: qsTr("清理全部")
                                    variant: "destructive"
                                    size: "sm"
                                    iconName: "trash-2"
                                    enabled: SettingsController.totalCacheSize > 0
                                    onClicked: {
                                        confirmClearDialog.showClearDialog(
                                            "all",
                                            qsTr("确认清理全部"),
                                            qsTr("确定要清理所有可清理数据吗？\n\n共 %1，清理后需要重新处理才能恢复结果。").arg(SettingsController.formatSize(SettingsController.totalCacheSize))
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
                                width: 44; height: 44; radius: 11
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: Theme.colors.brandGradientStart }
                                    GradientStop { position: 1.0; color: Theme.colors.brandGradientEnd }
                                }
                                Text { anchors.centerIn: parent; text: "E"; color: Theme.colors.textOnPrimary; font.pixelSize: 22; font.weight: Font.Bold }
                            }

                            ColumnLayout {
                                spacing: 2
                                Text { text: qsTr("EnhanceVision v0.1.0"); color: Theme.colors.foreground; font.pixelSize: 14; font.weight: Font.DemiBold }
                                Text { text: qsTr("高性能图像与视频增强工具"); color: Theme.colors.mutedForeground; font.pixelSize: 12 }
                                Text { text: qsTr("Built with Qt6 + NCNN"); color: Theme.colors.mutedForeground; font.pixelSize: 11; opacity: 0.7 }
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: clearCacheToast
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 24
        width: toastText.implicitWidth + 32
        height: 36
        radius: Theme.radius.md
        color: Theme.colors.card
        border.width: 1
        border.color: Theme.colors.cardBorder
        opacity: 0
        visible: opacity > 0

        property alias message: toastText.text

        function show(msg) {
            message = msg
            opacity = 1
            toastTimer.restart()
        }

        Text {
            id: toastText
            anchors.centerIn: parent
            text: qsTr("缓存已清除")
            color: Theme.colors.foreground
            font.pixelSize: 13
        }

        Timer {
            id: toastTimer
            interval: 2000
            onTriggered: clearCacheToast.opacity = 0
        }

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }

    Connections {
        target: SettingsController
        function onDataSizeChanged() {
            SettingsController.refreshDataSize()
        }
    }
}
