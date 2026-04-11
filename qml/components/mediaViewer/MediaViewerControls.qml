import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import EnhanceVision.Controllers
import "../../styles"
import "../../controls"
import "../../utils" as Utils

Rectangle {
    id: root

    property var viewer: null
    property var videoPlayer: null
    property var audioOutput: null
    property var playbackController: null

    readonly property string responsiveMode: Utils.ResponsiveUtils.getMode(width)
    readonly property bool shouldCollapseSpeed: Utils.ResponsiveUtils.shouldCollapseSpeedButtons(width)
    readonly property bool shouldCollapseSettings: Utils.ResponsiveUtils.shouldCollapseSettings(width)
    readonly property bool shouldCollapseVolume: Utils.ResponsiveUtils.shouldCollapseVolumeSlider(width)
    readonly property int displayPosition: {
        if (root.playbackController && root.playbackController._switchMode === 2 && SettingsController.videoRestorePosition) {
            return root.playbackController.frozenPosition || 0
        }
        if (root.videoPlayer && root.videoPlayer.position !== undefined && root.videoPlayer.position >= 0) {
            return root.videoPlayer.position
        }
        return 0
    }

    height: 80
    color: Theme.colors.card

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.colors.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: root.viewer ? root.viewer._formatTime(root.displayPosition) : "00:00"
                color: Theme.colors.mutedForeground
                font.pixelSize: root.responsiveMode === "compact" ? 10 : 12
            }

            Slider {
                Layout.fillWidth: true
                from: 0
                to: root.videoPlayer && root.videoPlayer.duration > 0 ? root.videoPlayer.duration : 1
                value: root.displayPosition
                enabled: (!root.playbackController || !root.playbackController.isLoading) && root.videoPlayer && root.videoPlayer.duration > 0
                onMoved: if (root.videoPlayer && (!root.playbackController || !root.playbackController.isLoading)) root.videoPlayer.position = value
            }

            Text {
                text: root.viewer ? root.viewer._formatTime(root.videoPlayer ? root.videoPlayer.duration : 0) : "00:00"
                color: Theme.colors.mutedForeground
                font.pixelSize: root.responsiveMode === "compact" ? 10 : 12
                Layout.minimumWidth: 45
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            IconButton {
                iconName: "skip-back"
                iconSize: 16
                btnSize: 32
                tooltip: qsTr("-10s")
                onClicked: if (root.videoPlayer) root.videoPlayer.position = Math.max(0, root.videoPlayer.position - 10000)
            }

            IconButton {
                iconName: root.videoPlayer && root.videoPlayer.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                iconSize: 16
                btnSize: 32
                tooltip: qsTr("播放/暂停")
                onClicked: if (root.videoPlayer) root.videoPlayer.togglePlay()
            }

            IconButton {
                iconName: "skip-forward"
                iconSize: 16
                btnSize: 32
                tooltip: qsTr("+10s")
                onClicked: if (root.videoPlayer) root.videoPlayer.position = Math.min(root.videoPlayer.duration, root.videoPlayer.position + 10000)
            }

            Item { width: 8 }

            Row {
                visible: !root.shouldCollapseSpeed
                spacing: 4

                Repeater {
                    model: [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]

                    Rectangle {
                        id: speedButton
                        required property real modelData
                        required property int index
                        width: speedLabel.implicitWidth + 12
                        height: 26
                        radius: 5
                        property bool isSelected: {
                            var currentRate = root.videoPlayer ? root.videoPlayer.playbackRate : 1.0
                            return Math.abs(currentRate - modelData) < 0.01
                        }

                        function speedColor() {
                            if (Theme.isDark) {
                                var darkColors = [
                                    Qt.rgba(0.22, 0.36, 0.56, 1.0),
                                    Qt.rgba(0.18, 0.42, 0.75, 1.0),
                                    Qt.rgba(0.14, 0.35, 0.82, 1.0),
                                    Qt.rgba(0.12, 0.27, 0.65, 1.0),
                                    Qt.rgba(0.08, 0.18, 0.48, 1.0),
                                    Qt.rgba(0.04, 0.10, 0.35, 1.0)
                                ]
                                return darkColors[index]
                            }
                            var lightColors = [
                                Qt.rgba(0.75, 0.85, 0.98, 1.0),
                                Qt.rgba(0.36, 0.55, 0.94, 1.0),
                                Qt.rgba(0.15, 0.35, 0.80, 1.0),
                                Qt.rgba(0.00, 0.18, 0.65, 1.0),
                                Qt.rgba(0.00, 0.12, 0.50, 1.0),
                                Qt.rgba(0.00, 0.07, 0.35, 1.0)
                            ]
                            return lightColors[index]
                        }

                        color: {
                            if (isSelected) {
                                return speedColor()
                            }
                            if (speedMouse.pressed) {
                                return Theme.isDark ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.12)
                            }
                            return Theme.isDark
                                   ? (speedMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06))
                                   : (speedMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(0, 0, 0, 0.04))
                        }
                        border.width: 1
                        border.color: isSelected ? "transparent" : Theme.colors.mediaControlBorder

                        Text {
                            id: speedLabel
                            anchors.centerIn: parent
                            text: speedButton.modelData.toFixed(1) + "x"
                            color: speedButton.isSelected ? "#FFFFFF" : Theme.colors.mediaControlTextMuted
                            font.pixelSize: 11
                            font.weight: speedButton.isSelected ? Font.Bold : Font.Normal
                        }

                        MouseArea {
                            id: speedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (root.videoPlayer) root.videoPlayer.playbackRate = speedButton.modelData
                        }

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                }
            }

            IconButton {
                visible: root.shouldCollapseSpeed
                iconName: "sliders"
                iconSize: 16
                btnSize: 28
                iconColor: Theme.colors.icon
                tooltip: qsTr("播放速度")
                onClicked: speedMenu.popup(this, 0, height)

                Menu {
                    id: speedMenu

                    Instantiator {
                        model: [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]

                        MenuItem {
                            text: modelData.toFixed(1) + "x"
                            checkable: true
                            checked: {
                                var currentRate = root.videoPlayer ? root.videoPlayer.playbackRate : 1.0
                                return Math.abs(currentRate - modelData) < 0.01
                            }
                            onTriggered: if (root.videoPlayer) root.videoPlayer.playbackRate = modelData
                        }

                        onObjectAdded: function(index, object) {
                            speedMenu.insertItem(index, object)
                        }
                        onObjectRemoved: function(index, object) {
                            speedMenu.removeItem(object)
                        }
                    }
                }
            }

            Row {
                visible: !root.shouldCollapseSettings
                spacing: 4

                Repeater {
                    model: [
                        {
                            key: "videoAutoPlay",
                            enabled: SettingsController.videoAutoPlay,
                            onIcon: "autoplay-on-open-on",
                            offIcon: "autoplay-on-open-off",
                            tooltip: qsTr("开/切自动播放")
                        },
                        {
                            key: "videoAutoPlayOnSwitch",
                            enabled: SettingsController.videoAutoPlayOnSwitch,
                            onIcon: "autoplay-on-switch-on",
                            offIcon: "autoplay-on-switch-off",
                            tooltip: qsTr("源/结自动播放")
                        },
                        {
                            key: "videoRestorePosition",
                            enabled: SettingsController.videoRestorePosition,
                            onIcon: "restore-position-on",
                            offIcon: "restore-position-off",
                            tooltip: qsTr("源/结恢复进度")
                        }
                    ]

                    Rectangle {
                        id: toggleButton
                        required property var modelData
                        width: 32
                        height: 26
                        radius: 5
                        color: modelData.enabled ? Theme.colors.primary
                                                 : (Theme.isDark
                                                    ? (toggleMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06))
                                                    : (toggleMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(0, 0, 0, 0.04)))
                        border.width: modelData.enabled ? 0 : 1
                        border.color: Theme.colors.mediaControlBorder

                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon(modelData.enabled ? modelData.onIcon : modelData.offIcon)
                            iconSize: 16
                            color: modelData.enabled ? "#FFFFFF" : Theme.colors.mediaControlIcon
                        }

                        MouseArea {
                            id: toggleMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: SettingsController[modelData.key] = !SettingsController[modelData.key]
                        }

                        ToolTip.visible: toggleMouse.containsMouse
                        ToolTip.text: modelData.tooltip
                        ToolTip.delay: 500

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }
                }
            }

            IconButton {
                visible: root.shouldCollapseSettings
                iconName: "settings"
                iconSize: 16
                btnSize: 28
                iconColor: Theme.colors.icon
                tooltip: qsTr("播放设置")
                onClicked: settingsMenu.popup(this, 0, height)

                Menu {
                    id: settingsMenu
                    MenuItem {
                        text: qsTr("开/切自动播放")
                        checkable: true
                        checked: SettingsController.videoAutoPlay
                        onTriggered: SettingsController.videoAutoPlay = !SettingsController.videoAutoPlay
                    }
                    MenuItem {
                        text: qsTr("源/结自动播放")
                        checkable: true
                        checked: SettingsController.videoAutoPlayOnSwitch
                        onTriggered: SettingsController.videoAutoPlayOnSwitch = !SettingsController.videoAutoPlayOnSwitch
                    }
                    MenuItem {
                        text: qsTr("源/结恢复进度")
                        checkable: true
                        checked: SettingsController.videoRestorePosition
                        onTriggered: SettingsController.videoRestorePosition = !SettingsController.videoRestorePosition
                    }
                }
            }

            Item { Layout.fillWidth: true }

            IconButton {
                iconName: root.audioOutput && root.audioOutput.volume > 0 ? "volume-2" : "volume-x"
                iconSize: 16
                btnSize: 28
                iconColor: Theme.colors.mediaControlIcon
                tooltip: qsTr("静音")
                onClicked: {
                    if (!root.viewer) {
                        return
                    }
                    if (root.viewer.sharedVolume > 0) {
                        root.viewer._volumeBeforeMute = root.viewer.sharedVolume
                        root.viewer.sharedVolume = 0
                        SettingsController.volume = 0
                    } else {
                        root.viewer.sharedVolume = root.viewer._volumeBeforeMute
                        SettingsController.volume = Math.round(root.viewer._volumeBeforeMute * 100)
                    }
                }
            }

            Slider {
                visible: !root.shouldCollapseVolume
                implicitWidth: 80
                from: 0
                to: 1
                value: root.viewer ? root.viewer.sharedVolume : 0.5
                onMoved: {
                    if (!root.viewer) {
                        return
                    }
                    root.viewer.sharedVolume = value
                    SettingsController.volume = Math.round(value * 100)
                }
            }

            IconButton {
                visible: root.shouldCollapseVolume
                iconName: "volume"
                iconSize: 16
                btnSize: 28
                iconColor: Theme.colors.mediaControlIcon
                tooltip: qsTr("音量")
                onClicked: volumePopup.open()

                Popup {
                    id: volumePopup
                    x: parent.width / 2 - width / 2
                    y: -height - 8
                    width: 160
                    height: 40
                    modal: false
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    contentItem: Slider {
                        anchors.fill: parent
                        anchors.margins: 8
                        from: 0
                        to: 1
                        value: root.viewer ? root.viewer.sharedVolume : 0.5
                        onMoved: {
                            if (!root.viewer) {
                                return
                            }
                            root.viewer.sharedVolume = value
                            SettingsController.volume = Math.round(value * 100)
                        }
                    }

                    background: Rectangle {
                        color: Theme.colors.card
                        radius: 8
                        border.width: 1
                        border.color: Theme.colors.border
                    }

                    enter: Transition { NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150 } }
                    exit: Transition { NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150 } }
                }
            }
        }
    }
}
