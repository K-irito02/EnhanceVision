import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import EnhanceVision.Controllers
import "../styles"
import "../controls"

Rectangle {
    id: root

    signal goBack()

    color: Theme.colors.background

    FolderDialog {
        id: folderDialog
        title: qsTr("选择默认保存路径")
        currentFolder: "file:///" + SettingsController.defaultSavePath
        onAccepted: {
            var path = selectedFolder.toString().replace("file:///", "")
            SettingsController.defaultSavePath = path
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
                    implicitHeight: behaviorCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: behaviorCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("folder"); color: Theme.colors.icon }
                            Text { text: qsTr("行为"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("默认保存路径"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.minimumWidth: 80 }

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
                                onClicked: folderDialog.open()
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
                                property bool updating: false
                                checked: SettingsController.autoReprocessAllEnabled
                                onCheckedChanged: {
                                    if (!updating && pressed) {
                                        SettingsController.autoReprocessAllEnabled = checked
                                    }
                                }
                                
                                Connections {
                                    target: SettingsController
                                    function onAutoReprocessAllEnabledChanged() {
                                        allSwitch.updating = true
                                        allSwitch.checked = SettingsController.autoReprocessAllEnabled
                                        allSwitch.updating = false
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
                                property bool updating: false
                                checked: SettingsController.autoReprocessShaderEnabled
                                onCheckedChanged: {
                                    if (!updating && pressed) {
                                        SettingsController.autoReprocessShaderEnabled = checked
                                    }
                                }
                                
                                Connections {
                                    target: SettingsController
                                    function onAutoReprocessShaderEnabledChanged() {
                                        shaderSwitch.updating = true
                                        shaderSwitch.checked = SettingsController.autoReprocessShaderEnabled
                                        shaderSwitch.updating = false
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
                                property bool updating: false
                                checked: SettingsController.autoReprocessAIEnabled
                                onCheckedChanged: {
                                    if (!updating && pressed) {
                                        SettingsController.autoReprocessAIEnabled = checked
                                    }
                                }
                                
                                Connections {
                                    target: SettingsController
                                    function onAutoReprocessAIEnabledChanged() {
                                        aiSwitch.updating = true
                                        aiSwitch.checked = SettingsController.autoReprocessAIEnabled
                                        aiSwitch.updating = false
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
                    implicitHeight: perfCol.implicitHeight + 32
                    color: Theme.colors.card
                    border.width: 1
                    border.color: Theme.colors.cardBorder
                    radius: Theme.radius.lg

                    ColumnLayout {
                        id: perfCol
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14

                        RowLayout {
                            spacing: 8
                            ColoredIcon { iconSize: 18; source: Theme.icon("cpu"); color: Theme.colors.icon }
                            Text { text: qsTr("缓存"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: qsTr("缓存管理"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.preferredWidth: 120 }
                            Button {
                                text: qsTr("清除缩略图缓存")
                                variant: "secondary"
                                size: "sm"
                                iconName: "trash"
                                onClicked: {
                                    var cachePath = Qt.standardPaths(Qt.StandardPaths.CacheLocation) + "/thumbnails"
                                    fileController.clearCache(cachePath)
                                    clearCacheToast.show()
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
                                Text { text: "EnhanceVision v0.1.0"; color: Theme.colors.foreground; font.pixelSize: 14; font.weight: Font.DemiBold }
                                Text { text: qsTr("高性能图像与视频增强工具"); color: Theme.colors.mutedForeground; font.pixelSize: 12 }
                                Text { text: "Built with Qt6 + NCNN"; color: Theme.colors.mutedForeground; font.pixelSize: 11; opacity: 0.7 }
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

        function show() {
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
}
