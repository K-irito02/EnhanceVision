import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window
import "../styles"
import "../controls"
import EnhanceVision.Utils

ColumnLayout {
    id: root

    property real brightness: 0.0
    property real contrast: 1.0
    property real saturation: 1.0
    property real hue: 0.0
    property real sharpness: 0.0
    property real blur: 0.0
    property real denoise: 0.0
    property real exposure: 0.0
    property real gamma: 1.0
    property real temperature: 0.0
    property real tint: 0.0
    property real vignette: 0.0
    property real highlights: 0.0
    property real shadows: 0.0

    property var customCategories: []
    property var customPresets: []

    signal paramChanged(string name, real value)

    spacing: 6

    property string selectedCategory: "all"
    property var builtinCategories: ["basic", "tone", "vintage", "cinema", "bw", "scene", "art", "social"]
    property var categoryNames: ({
        "all": qsTr("全部"),
        "basic": qsTr("基础"),
        "tone": qsTr("色调"),
        "vintage": qsTr("复古"),
        "cinema": qsTr("电影"),
        "bw": qsTr("黑白"),
        "scene": qsTr("场景"),
        "art": qsTr("艺术"),
        "social": qsTr("社交")
    })

    property int editingCategoryIndex: -1
    property int editingPresetIndex: -1
    property int draggedCategoryIndex: -1

    property int draggedPresetIndex: -1

    property var currentPreset: null
    property string currentPresetName: ""

    function isCurrentPresetActive() {
        if (!currentPreset) return false
        
        // 使用 !== undefined 检查，避免值为 0 时被错误替换为默认值
        var presetBrightness = currentPreset.brightness !== undefined ? currentPreset.brightness : 0.0
        var presetContrast = currentPreset.contrast !== undefined ? currentPreset.contrast : 1.0
        var presetSaturation = currentPreset.saturation !== undefined ? currentPreset.saturation : 1.0
        var presetHue = currentPreset.hue !== undefined ? currentPreset.hue : 0.0
        var presetSharpness = currentPreset.sharpness !== undefined ? currentPreset.sharpness : 0.0
        var presetBlur = currentPreset.blur !== undefined ? currentPreset.blur : 0.0
        var presetDenoise = currentPreset.denoise !== undefined ? currentPreset.denoise : 0.0
        var presetExposure = currentPreset.exposure !== undefined ? currentPreset.exposure : 0.0
        var presetGamma = currentPreset.gamma !== undefined ? currentPreset.gamma : 1.0
        var presetTemperature = currentPreset.temperature !== undefined ? currentPreset.temperature : 0.0
        var presetTint = currentPreset.tint !== undefined ? currentPreset.tint : 0.0
        var presetVignette = currentPreset.vignette !== undefined ? currentPreset.vignette : 0.0
        var presetHighlights = currentPreset.highlights !== undefined ? currentPreset.highlights : 0.0
        var presetShadows = currentPreset.shadows !== undefined ? currentPreset.shadows : 0.0
        
        return Math.abs(brightness - presetBrightness) < 0.001 &&
               Math.abs(contrast - presetContrast) < 0.001 &&
               Math.abs(saturation - presetSaturation) < 0.001 &&
               Math.abs(hue - presetHue) < 0.001 &&
               Math.abs(sharpness - presetSharpness) < 0.001 &&
               Math.abs(blur - presetBlur) < 0.001 &&
               Math.abs(denoise - presetDenoise) < 0.001 &&
               Math.abs(exposure - presetExposure) < 0.001 &&
               Math.abs(gamma - presetGamma) < 0.001 &&
               Math.abs(temperature - presetTemperature) < 0.001 &&
               Math.abs(tint - presetTint) < 0.001 &&
               Math.abs(vignette - presetVignette) < 0.001 &&
               Math.abs(highlights - presetHighlights) < 0.001 &&
               Math.abs(shadows - presetShadows) < 0.001
    }

    ColumnLayout {
        id: presetSection
        Layout.fillWidth: true
        Layout.rightMargin: 0
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.rightMargin: 0
            height: presetHeader.implicitHeight + 12
            radius: Theme.radius.md
            color: Theme.colors.primarySubtle
            border.width: 1
            border.color: Qt.rgba(0.12, 0.34, 0.82, 0.15)

            RowLayout {
                id: presetHeader
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 8

                ColoredIcon {
                    source: Theme.icon("sparkles")
                    iconSize: 16
                    color: Theme.colors.primary
                }

                Text {
                    text: qsTr("快速预设")
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: Theme.colors.foreground
                }

                Item { Layout.fillWidth: true; Layout.minimumWidth: 10 }

                Rectangle {
                    id: addCategoryBtn
                    width: addCategoryRow.implicitWidth + 12
                    height: 24
                    radius: Theme.radius.sm
                    color: addCategoryMouse.containsMouse ? Qt.rgba(0.12, 0.34, 0.82, 0.15) : "transparent"
                    border.width: 1
                    border.color: addCategoryMouse.containsMouse ? Theme.colors.primary : Theme.colors.border

                    Row {
                        id: addCategoryRow
                        anchors.centerIn: parent
                        spacing: 3

                        ColoredIcon {
                            source: Theme.icon("folder-plus")
                            iconSize: 11
                            color: addCategoryMouse.containsMouse ? Theme.colors.primary : Theme.colors.foreground
                        }

                        Text {
                            text: qsTr("类别")
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: addCategoryMouse.containsMouse ? Theme.colors.primary : Theme.colors.foreground
                        }
                    }

                    MouseArea {
                        id: addCategoryMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: addCategoryDialog.show()
                    }

                    ToolTip.visible: addCategoryMouse.containsMouse
                    ToolTip.text: qsTr("新建类别")
                    ToolTip.delay: 500

                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                }

                Rectangle {
                    id: saveStyleBtn
                    width: saveRow.implicitWidth + 12
                    height: 24
                    radius: Theme.radius.sm
                    color: saveMouse.containsMouse ? Theme.colors.primary : "transparent"
                    border.width: 1
                    border.color: saveMouse.containsMouse ? Theme.colors.primary : Theme.colors.border
                    visible: true

                    Row {
                        id: saveRow
                        anchors.centerIn: parent
                        spacing: 3

                        ColoredIcon {
                            source: Theme.icon("plus")
                            iconSize: 11
                            color: saveMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                        }

                        Text {
                            text: qsTr("风格")
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: saveMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                        }
                    }

                    MouseArea {
                        id: saveMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: savePresetDialog.show()
                    }

                    ToolTip.visible: saveMouse.containsMouse
                    ToolTip.text: qsTr("保存风格")
                    ToolTip.delay: 500

                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                }
            }
        }

        Text {
            text: qsTr("类别")
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: Theme.colors.mutedForeground
            Layout.leftMargin: 2
        }

        Flow {
            id: categoryFlow
            Layout.fillWidth: true
            Layout.rightMargin: 0
            spacing: 6
            flow: Flow.LeftToRight
            layoutDirection: Qt.LeftToRight

            Rectangle {
                width: allCategoryText.implicitWidth + 16
                height: 28
                radius: 14
                color: selectedCategory === "all" ? Theme.colors.primary : (allCategoryMouse.containsMouse ? Qt.rgba(0.12, 0.34, 0.82, 0.12) : "transparent")
                border.width: selectedCategory === "all" ? 0 : 1
                border.color: allCategoryMouse.containsMouse ? Theme.colors.primary : Theme.colors.border

                Text {
                    id: allCategoryText
                    anchors.centerIn: parent
                    text: qsTr("全部")
                    font.pixelSize: 12
                    font.weight: selectedCategory === "all" ? Font.DemiBold : Font.Medium
                    color: selectedCategory === "all" ? "#FFFFFF" : Theme.colors.foreground
                }

                MouseArea {
                    id: allCategoryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: selectedCategory = "all"
                }

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            Repeater {
                id: customCategoryRepeater
                model: root.customCategories

                Rectangle {
                    id: customCatRect
                    width: customCatText.implicitWidth + 16
                    height: 28
                    radius: 14
                    color: selectedCategory === ("custom_" + index) ? Theme.colors.primary : (customCatMouse.containsMouse ? Qt.rgba(0.12, 0.34, 0.82, 0.12) : Theme.colors.accent)
                    border.width: selectedCategory === ("custom_" + index) ? 0 : 1
                    border.color: customCatMouse.containsMouse ? Theme.colors.primary : Theme.colors.border

                    Text {
                        id: customCatText
                        anchors.centerIn: parent
                        text: modelData.name
                        font.pixelSize: 12
                        font.weight: selectedCategory === ("custom_" + index) ? Font.DemiBold : Font.Medium
                        color: selectedCategory === ("custom_" + index) ? "#FFFFFF" : Theme.colors.foreground
                    }

                    MouseArea {
                        id: customCatMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton

                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                categoryContextMenu.categoryIndex = index
                                var globalPos = customCatMouse.mapToItem(null, mouse.x, mouse.y)
                                categoryContextMenu.showAt(globalPos.x, globalPos.y)
                            } else {
                                selectedCategory = "custom_" + index
                            }
                        }
                    }

                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }

            Repeater {
                model: builtinCategories

                Rectangle {
                    width: categoryText.implicitWidth + 16
                    height: 28
                    radius: 14
                    color: selectedCategory === modelData ? Theme.colors.primary : (categoryMouse.containsMouse ? Qt.rgba(0.12, 0.34, 0.82, 0.12) : "transparent")
                    border.width: selectedCategory === modelData ? 0 : 1
                    border.color: categoryMouse.containsMouse ? Theme.colors.primary : Theme.colors.border

                    Text {
                        id: categoryText
                        anchors.centerIn: parent
                        text: categoryNames[modelData]
                        font.pixelSize: 12
                        font.weight: selectedCategory === modelData ? Font.DemiBold : Font.Medium
                        color: selectedCategory === modelData ? "#FFFFFF" : Theme.colors.foreground
                    }

                    MouseArea {
                        id: categoryMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: selectedCategory = modelData
                    }

                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
            opacity: 0.5
        }

        Text {
            text: qsTr("风格")
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: Theme.colors.mutedForeground
            Layout.leftMargin: 2
        }

        ScrollView {
            id: presetScrollView
            Layout.fillWidth: true
            Layout.rightMargin: 0
            Layout.preferredHeight: Math.min(presetFlow.implicitHeight + 8, 100)
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            Flow {
                id: presetFlow
                width: presetScrollView.availableWidth
                spacing: 6
                flow: Flow.LeftToRight
                layoutDirection: Qt.LeftToRight

                Repeater {
                    id: customPresetRepeater
                    model: root.customPresets

                    Rectangle {
                        id: customPresetRect
                        property bool isActive: currentPreset && currentPreset.name === modelData.name && isCurrentPresetActive()
                        width: presetTextCustom.implicitWidth + 18
                        height: 28
                        radius: Theme.radius.sm
                        color: isActive ? Theme.colors.primary : (presetMouseCustom.containsMouse ? Theme.colors.primary : Theme.colors.accent)
                        border.width: 1
                        border.color: isActive ? Theme.colors.primary : (presetMouseCustom.containsMouse ? Theme.colors.primary : Theme.colors.border)
                        visible: selectedCategory === "all" || modelData.category === selectedCategory || (modelData.category && modelData.category.startsWith("custom_") && selectedCategory === modelData.category)

                        Text {
                            id: presetTextCustom
                            anchors.centerIn: parent
                            text: modelData.name
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: (presetMouseCustom.containsMouse || customPresetRect.isActive) ? "#FFFFFF" : Theme.colors.foreground
                        }

                        MouseArea {
                            id: presetMouseCustom
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton

                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    presetContextMenu.presetIndex = index
                                    var globalPos = presetMouseCustom.mapToItem(null, mouse.x, mouse.y)
                                    presetContextMenu.showAt(globalPos.x, globalPos.y)
                                } else {
                                    applyPreset(modelData)
                                }
                            }
                        }

                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }

                Repeater {
                    model: [
                        { name: qsTr("柔和"), brightness: 0.05, contrast: 1.05, saturation: 1.1, sharpness: 0.2, category: "basic" },
                        { name: qsTr("鲜艳"), brightness: 0.0, contrast: 1.15, saturation: 1.3, sharpness: 0.3, category: "basic" },
                        { name: qsTr("明亮"), brightness: 0.15, contrast: 1.05, saturation: 1.05, exposure: 0.1, category: "basic" },
                        { name: qsTr("暗调"), brightness: -0.1, contrast: 1.1, saturation: 0.95, exposure: -0.1, category: "basic" },
                        { name: qsTr("高对比"), brightness: 0.0, contrast: 1.4, saturation: 1.1, category: "basic" },
                        { name: qsTr("低对比"), brightness: 0.05, contrast: 0.85, saturation: 0.95, category: "basic" },
                        { name: qsTr("清透"), brightness: 0.08, contrast: 1.08, saturation: 0.95, highlights: -0.1, category: "basic" },
                        { name: qsTr("冷调"), brightness: 0.0, contrast: 1.1, saturation: 0.9, temperature: -0.2, category: "tone" },
                        { name: qsTr("暖调"), brightness: 0.0, contrast: 1.05, saturation: 1.1, temperature: 0.18, category: "tone" },
                        { name: qsTr("青橙"), brightness: 0.0, contrast: 1.1, saturation: 1.1, temperature: 0.1, tint: -0.08, category: "tone" },
                        { name: qsTr("粉嫩"), brightness: 0.05, contrast: 0.95, saturation: 0.9, temperature: 0.05, tint: 0.12, category: "tone" },
                        { name: qsTr("冰蓝"), brightness: 0.0, contrast: 1.05, saturation: 0.85, temperature: -0.25, tint: -0.05, category: "tone" },
                        { name: qsTr("金秋"), brightness: 0.0, contrast: 1.1, saturation: 1.15, temperature: 0.2, tint: 0.05, category: "tone" },
                        { name: qsTr("森林"), brightness: -0.02, contrast: 1.08, saturation: 1.05, temperature: -0.05, tint: -0.1, category: "tone" },
                        { name: qsTr("复古"), brightness: 0.0, contrast: 0.95, saturation: 0.8, temperature: 0.15, vignette: 0.2, category: "vintage" },
                        { name: qsTr("胶片"), brightness: 0.0, contrast: 1.1, saturation: 0.9, vignette: 0.3, temperature: 0.08, category: "vintage" },
                        { name: qsTr("褪色"), brightness: 0.1, contrast: 0.85, saturation: 0.7, category: "vintage" },
                        { name: qsTr("老照片"), brightness: 0.05, contrast: 0.9, saturation: 0.5, temperature: 0.15, vignette: 0.35, category: "vintage" },
                        { name: qsTr("柯达"), brightness: 0.02, contrast: 1.08, saturation: 1.0, temperature: 0.1, highlights: 0.05, category: "vintage" },
                        { name: qsTr("富士"), brightness: 0.0, contrast: 1.05, saturation: 0.95, temperature: -0.05, tint: 0.03, category: "vintage" },
                        { name: qsTr("宝丽来"), brightness: 0.08, contrast: 0.92, saturation: 0.85, temperature: 0.08, vignette: 0.25, category: "vintage" },
                        { name: qsTr("年代感"), brightness: -0.03, contrast: 0.95, saturation: 0.75, temperature: 0.12, vignette: 0.2, gamma: 1.05, category: "vintage" },
                        { name: qsTr("电影"), brightness: -0.05, contrast: 1.2, saturation: 0.85, shadows: 0.15, highlights: -0.1, category: "cinema" },
                        { name: qsTr("银幕"), brightness: -0.02, contrast: 1.15, saturation: 0.8, temperature: -0.05, vignette: 0.15, category: "cinema" },
                        { name: qsTr("好莱坞"), brightness: 0.0, contrast: 1.18, saturation: 0.9, temperature: 0.05, shadows: 0.1, category: "cinema" },
                        { name: qsTr("黑色电影"), brightness: -0.1, contrast: 1.3, saturation: 0.3, vignette: 0.3, category: "cinema" },
                        { name: qsTr("科幻"), brightness: -0.05, contrast: 1.2, saturation: 0.7, temperature: -0.15, tint: 0.05, category: "cinema" },
                        { name: qsTr("赛博朋克"), brightness: -0.08, contrast: 1.25, saturation: 1.2, temperature: -0.1, tint: 0.15, category: "cinema" },
                        { name: qsTr("末日"), brightness: -0.12, contrast: 1.15, saturation: 0.6, temperature: 0.1, vignette: 0.25, category: "cinema" },
                        { name: qsTr("黑白"), saturation: 0.0, contrast: 1.15, category: "bw" },
                        { name: qsTr("高调黑白"), saturation: 0.0, contrast: 1.0, brightness: 0.15, exposure: 0.1, category: "bw" },
                        { name: qsTr("低调黑白"), saturation: 0.0, contrast: 1.25, brightness: -0.1, shadows: 0.15, category: "bw" },
                        { name: qsTr("银盐"), saturation: 0.0, contrast: 1.2, sharpness: 0.3, category: "bw" },
                        { name: qsTr("戏剧黑白"), saturation: 0.0, contrast: 1.4, brightness: -0.05, vignette: 0.2, category: "bw" },
                        { name: qsTr("人像"), brightness: 0.05, contrast: 1.0, saturation: 1.05, sharpness: 0.15, denoise: 0.2, category: "scene" },
                        { name: qsTr("风景"), brightness: 0.0, contrast: 1.2, saturation: 1.25, sharpness: 0.25, category: "scene" },
                        { name: qsTr("日落"), brightness: 0.0, contrast: 1.1, saturation: 1.2, temperature: 0.25, highlights: 0.1, category: "scene" },
                        { name: qsTr("清晨"), brightness: 0.1, contrast: 0.95, saturation: 0.9, temperature: -0.1, exposure: 0.05, category: "scene" },
                        { name: qsTr("夜景"), brightness: -0.1, contrast: 1.15, saturation: 0.8, shadows: 0.2, category: "scene" },
                        { name: qsTr("星空"), brightness: -0.15, contrast: 1.3, saturation: 1.1, shadows: 0.25, temperature: -0.1, category: "scene" },
                        { name: qsTr("海洋"), brightness: 0.05, contrast: 1.1, saturation: 1.15, temperature: -0.12, category: "scene" },
                        { name: qsTr("都市"), brightness: -0.03, contrast: 1.18, saturation: 0.95, sharpness: 0.2, category: "scene" },
                        { name: qsTr("建筑"), brightness: 0.0, contrast: 1.15, saturation: 0.9, sharpness: 0.35, category: "scene" },
                        { name: qsTr("美食"), brightness: 0.08, contrast: 1.05, saturation: 1.2, temperature: 0.08, category: "scene" },
                        { name: qsTr("HDR"), brightness: 0.0, contrast: 1.25, saturation: 1.15, highlights: -0.2, shadows: 0.25, category: "art" },
                        { name: qsTr("梦幻"), brightness: 0.1, contrast: 0.9, saturation: 1.05, blur: 0.15, category: "art" },
                        { name: qsTr("朦胧"), brightness: 0.08, contrast: 0.85, saturation: 0.9, blur: 0.25, vignette: 0.1, category: "art" },
                        { name: qsTr("水墨"), saturation: 0.15, contrast: 1.2, brightness: 0.05, category: "art" },
                        { name: qsTr("哥特"), brightness: -0.15, contrast: 1.35, saturation: 0.5, vignette: 0.35, category: "art" },
                        { name: qsTr("波普"), brightness: 0.05, contrast: 1.3, saturation: 1.5, category: "art" },
                        { name: qsTr("素描"), saturation: 0.0, contrast: 1.5, sharpness: 0.5, brightness: 0.1, category: "art" },
                        { name: qsTr("ins风"), brightness: 0.05, contrast: 1.08, saturation: 1.1, temperature: 0.05, vignette: 0.08, category: "social" },
                        { name: qsTr("日系"), brightness: 0.1, contrast: 0.92, saturation: 0.85, temperature: -0.05, exposure: 0.05, category: "social" },
                        { name: qsTr("韩系"), brightness: 0.08, contrast: 0.95, saturation: 0.9, highlights: 0.05, category: "social" },
                        { name: qsTr("港风"), brightness: -0.02, contrast: 1.12, saturation: 0.95, temperature: 0.08, vignette: 0.12, category: "social" },
                        { name: qsTr("莫兰迪"), brightness: 0.05, contrast: 0.9, saturation: 0.65, category: "social" },
                        { name: qsTr("奶油"), brightness: 0.1, contrast: 0.88, saturation: 0.8, temperature: 0.05, category: "social" },
                        { name: qsTr("高级灰"), brightness: 0.0, contrast: 1.0, saturation: 0.5, temperature: -0.03, category: "social" }
                    ]

                    Rectangle {
                        property bool isActive: currentPreset && currentPreset.name === modelData.name && isCurrentPresetActive()
                        width: presetText.implicitWidth + 18
                        height: 28
                        radius: Theme.radius.sm
                        color: isActive ? Theme.colors.primary : (presetMouse.containsMouse ? Theme.colors.primary : "transparent")
                        border.width: 1
                        border.color: isActive ? Theme.colors.primary : (presetMouse.containsMouse ? Theme.colors.primary : Theme.colors.border)
                        visible: selectedCategory === "all" || modelData.category === selectedCategory

                        Text {
                            id: presetText
                            anchors.centerIn: parent
                            text: modelData.name
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: (presetMouse.containsMouse || parent.isActive) ? "#FFFFFF" : Theme.colors.foreground
                        }

                        MouseArea {
                            id: presetMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: applyPreset(modelData)
                        }

                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }
            }
        }
    }

    Popup {
        id: categoryContextMenu
        property int categoryIndex: -1
        
        parent: Overlay.overlay
        padding: 6
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        function showAt(globalX, globalY) {
            var menuWidth = 150
            var menuHeight = 80
            var margin = 8
            
            var finalX = globalX + 4
            var finalY = globalY + 4
            
            if (finalX + menuWidth + margin > parent.width) {
                finalX = globalX - menuWidth - 4
            }
            if (finalY + menuHeight + margin > parent.height) {
                finalY = globalY - menuHeight - 4
            }
            
            finalX = Math.max(margin, Math.min(finalX, parent.width - menuWidth - margin))
            finalY = Math.max(margin, Math.min(finalY, parent.height - menuHeight - margin))
            
            categoryContextMenu.x = finalX
            categoryContextMenu.y = finalY
            categoryContextMenu.open()
        }

        background: Rectangle {
            implicitWidth: 150
            color: Theme.colors.popover
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            Rectangle {
                anchors.fill: parent
                anchors.margins: -10
                color: "transparent"
                border.width: 10
                border.color: "transparent"
                z: -1

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 10
                    radius: Theme.radius.lg + 3
                    color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                    z: -1
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 3

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: renameCategoryMouse.containsMouse ? Theme.colors.primary : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    ColoredIcon {
                        source: Theme.icon("pencil")
                        iconSize: 16
                        color: renameCategoryMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                    }

                    Text {
                        text: qsTr("重命名")
                        color: renameCategoryMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    id: renameCategoryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        categoryContextMenu.close()
                        renameCategoryDialog.categoryIndex = categoryContextMenu.categoryIndex
                        renameCategoryDialog.show()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: deleteCategoryMouse.containsMouse ? Theme.colors.destructive : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    ColoredIcon {
                        source: Theme.icon("trash-2")
                        iconSize: 16
                        color: deleteCategoryMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                    }

                    Text {
                        text: qsTr("删除")
                        color: deleteCategoryMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    id: deleteCategoryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        categoryContextMenu.close()
                        deleteCategoryDialog.categoryIndex = categoryContextMenu.categoryIndex
                        deleteCategoryDialog.show()
                    }
                }
            }
        }
    }

    Popup {
        id: presetContextMenu
        property int presetIndex: -1
        
        parent: Overlay.overlay
        padding: 6
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        
        function showAt(globalX, globalY) {
            var menuWidth = 150
            var menuHeight = 80
            var margin = 8
            
            var finalX = globalX + 4
            var finalY = globalY + 4
            
            if (finalX + menuWidth + margin > parent.width) {
                finalX = globalX - menuWidth - 4
            }
            if (finalY + menuHeight + margin > parent.height) {
                finalY = globalY - menuHeight - 4
            }
            
            finalX = Math.max(margin, Math.min(finalX, parent.width - menuWidth - margin))
            finalY = Math.max(margin, Math.min(finalY, parent.height - menuHeight - margin))
            
            presetContextMenu.x = finalX
            presetContextMenu.y = finalY
            presetContextMenu.open()
        }

        background: Rectangle {
            implicitWidth: 150
            color: Theme.colors.popover
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            Rectangle {
                anchors.fill: parent
                anchors.margins: -10
                color: "transparent"
                border.width: 10
                border.color: "transparent"
                z: -1

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 10
                    radius: Theme.radius.lg + 3
                    color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.4 : 0.1)
                    z: -1
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 3

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: renamePresetMouse.containsMouse ? Theme.colors.primary : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    ColoredIcon {
                        source: Theme.icon("pencil")
                        iconSize: 16
                        color: renamePresetMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                    }

                    Text {
                        text: qsTr("重命名")
                        color: renamePresetMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    id: renamePresetMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        presetContextMenu.close()
                        renamePresetDialog.presetIndex = presetContextMenu.presetIndex
                        renamePresetDialog.show()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 3
                Layout.rightMargin: 3
                height: 34
                radius: Theme.radius.sm
                color: deletePresetMouse.containsMouse ? Theme.colors.destructive : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.animation.fast } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    ColoredIcon {
                        source: Theme.icon("trash-2")
                        iconSize: 16
                        color: deletePresetMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                    }

                    Text {
                        text: qsTr("删除")
                        color: deletePresetMouse.containsMouse ? "#FFFFFF" : Theme.colors.destructive
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }
                }

                MouseArea {
                    id: deletePresetMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        presetContextMenu.close()
                        var presets = root.customPresets.slice()
                        presets.splice(presetContextMenu.presetIndex, 1)
                        root.customPresets = presets
                    }
                }
            }
        }
    }

    Window {
        id: deleteCategoryDialog
        title: qsTr("删除类别")
        width: 380
        height: deleteCategoryContent.implicitHeight + 40 + 32
        color: "transparent"
        flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        modality: Qt.NonModal
        transientParent: null
        property int categoryIndex: -1

        function updateExcludeRegions() {
            deleteCategoryHelper.clearExcludeRegions()
            var closeBtn = deleteCategoryTitleBar.closeButton
            if (closeBtn && closeBtn.width > 0) {
                var posInWindow = closeBtn.mapToItem(deleteCategoryBg, 0, 0)
                deleteCategoryHelper.addExcludeRegion(posInWindow.x, posInWindow.y, closeBtn.width, closeBtn.height)
            }
        }

        SubWindowHelper {
            id: deleteCategoryHelper
            Component.onCompleted: {
                deleteCategoryHelper.setWindow(deleteCategoryDialog)
            }
        }

        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)

        onVisibleChanged: {
            if (visible) {
                var mainWindow = root.Window.window
                if (mainWindow) {
                    x = mainWindow.x + (mainWindow.width - width) / 2
                    y = mainWindow.y + (mainWindow.height - height) / 2
                }
                requestActivate()
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(function() {
                    if (presetsInfoText) {
                        presetsInfoText.updateText()
                    }
                })
            }
        }

        Rectangle {
            id: deleteCategoryBg
            anchors.fill: parent
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            DialogTitleBar {
                id: deleteCategoryTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                titleText: qsTr("删除类别")
                windowRef: deleteCategoryDialog
                onCloseClicked: deleteCategoryDialog.close()
            }

            ColumnLayout {
                id: deleteCategoryContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: deleteCategoryTitleBar.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("确定要删除该类别吗？此操作无法撤销。")
                    font.pixelSize: 13
                    color: Theme.colors.mutedForeground
                    wrapMode: Text.WordWrap
                }

                Text {
                    id: presetsInfoText
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    wrapMode: Text.WordWrap
                    
                    Component.onCompleted: updateText()
                    
                    function updateText() {
                        var categoryId = "custom_" + deleteCategoryDialog.categoryIndex
                        var count = 0
                        var names = []
                        for (var i = 0; i < root.customPresets.length; i++) {
                            if (root.customPresets[i].category === categoryId) {
                                count++
                                names.push(root.customPresets[i].name)
                            }
                        }
                        
                        if (count > 0) {
                            presetsInfoText.text = qsTr("该类别下有 %1 个风格将一并删除").arg(count)
                            presetsInfoText.color = Theme.colors.destructive
                            presetsList.visible = true
                            presetsList.model = names
                        } else {
                            presetsInfoText.text = qsTr("该类别下没有任何风格")
                            presetsInfoText.color = Theme.colors.mutedForeground
                            presetsList.visible = false
                        }
                    }
                }

                ColumnLayout {
                    id: presetsList
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    spacing: 4
                    visible: false
                    property var model: []

                    Repeater {
                        model: presetsList.model

                        Text {
                            text: "• " + modelData
                            font.pixelSize: 12
                            color: Theme.colors.foreground
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 12

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("取消")
                        variant: "ghost"
                        onClicked: deleteCategoryDialog.close()
                    }

                    Button {
                        text: qsTr("删除")
                        variant: "destructive"
                        onClicked: {
                            if (deleteCategoryDialog.categoryIndex >= 0) {
                                var categoryId = "custom_" + deleteCategoryDialog.categoryIndex
                                var categories = root.customCategories.slice()
                                categories.splice(deleteCategoryDialog.categoryIndex, 1)
                                root.customCategories = categories
                                
                                var presets = []
                                for (var i = 0; i < root.customPresets.length; i++) {
                                    if (root.customPresets[i].category !== categoryId) {
                                        presets.push(root.customPresets[i])
                                    }
                                }
                                root.customPresets = presets
                                
                                if (selectedCategory === categoryId) {
                                    selectedCategory = "all"
                                }
                                deleteCategoryDialog.hide()
                            }
                        }
                    }
                }
            }
        }

        function hasPresetsInCategory() {
            if (deleteCategoryDialog.categoryIndex < 0) return false
            var categoryId = "custom_" + deleteCategoryDialog.categoryIndex
            for (var i = 0; i < root.customPresets.length; i++) {
                if (root.customPresets[i].category === categoryId) {
                    return true
                }
            }
            return false
        }

        function getPresetsCountInCategory() {
            if (deleteCategoryDialog.categoryIndex < 0) return 0
            var categoryId = "custom_" + deleteCategoryDialog.categoryIndex
            var count = 0
            for (var i = 0; i < root.customPresets.length; i++) {
                if (root.customPresets[i].category === categoryId) {
                    count++
                }
            }
            return count
        }

        function getPresetsInCategory() {
            if (deleteCategoryDialog.categoryIndex < 0) return []
            var categoryId = "custom_" + deleteCategoryDialog.categoryIndex
            var result = []
            for (var i = 0; i < root.customPresets.length; i++) {
                if (root.customPresets[i].category === categoryId) {
                    result.push(root.customPresets[i].name)
                }
            }
            return result
        }
    }

    Window {
        id: addCategoryDialog
        title: qsTr("新建类别")
        width: 320
        height: addCategoryContent.implicitHeight + 40 + 32
        color: "transparent"
        flags: Qt.Window | Qt.FramelessWindowHint
        modality: Qt.ApplicationModal
        transientParent: null

        function updateExcludeRegions() {
            addCategoryHelper.clearExcludeRegions()
            var closeBtn = addCategoryTitleBar.closeButton
            if (closeBtn && closeBtn.width > 0) {
                var posInWindow = closeBtn.mapToItem(addCategoryBg, 0, 0)
                addCategoryHelper.addExcludeRegion(posInWindow.x, posInWindow.y, closeBtn.width, closeBtn.height)
            }
        }

        SubWindowHelper {
            id: addCategoryHelper
            Component.onCompleted: {
                addCategoryHelper.setWindow(addCategoryDialog)
            }
        }

        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)

        onVisibleChanged: {
            if (visible) {
                var mainWindow = root.Window.window
                if (mainWindow) {
                    x = mainWindow.x + (mainWindow.width - width) / 2
                    y = mainWindow.y + (mainWindow.height - height) / 2
                }
                newCategoryInput.forceActiveFocus()
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
            }
        }

        Rectangle {
            id: addCategoryBg
            anchors.fill: parent
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            DialogTitleBar {
                id: addCategoryTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                titleText: qsTr("新建类别")
                windowRef: addCategoryDialog
                onCloseClicked: addCategoryDialog.close()
            }

            ColumnLayout {
                id: addCategoryContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: addCategoryTitleBar.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 12

                Text {
                    text: qsTr("类别名称")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.colors.mutedForeground
                }

                TextField {
                    id: newCategoryInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("输入类别名称")
                    font.pixelSize: 13
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("取消")
                        variant: "ghost"
                        onClicked: addCategoryDialog.close()
                    }

                    Button {
                        text: qsTr("创建")
                        onClicked: {
                            if (newCategoryInput.text.trim() !== "") {
                                var categories = root.customCategories.slice()
                                categories.push({ name: newCategoryInput.text.trim() })
                                root.customCategories = categories
                                newCategoryInput.text = ""
                                addCategoryDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Window {
        id: renameCategoryDialog
        title: qsTr("重命名类别")
        width: 320
        height: renameCategoryContent.implicitHeight + 40 + 32
        color: "transparent"
        flags: Qt.Window | Qt.FramelessWindowHint
        modality: Qt.ApplicationModal
        transientParent: null
        property int categoryIndex: -1

        function updateExcludeRegions() {
            renameCategoryHelper.clearExcludeRegions()
            var closeBtn = renameCategoryTitleBar.closeButton
            if (closeBtn && closeBtn.width > 0) {
                var posInWindow = closeBtn.mapToItem(renameCategoryBg, 0, 0)
                renameCategoryHelper.addExcludeRegion(posInWindow.x, posInWindow.y, closeBtn.width, closeBtn.height)
            }
        }

        SubWindowHelper {
            id: renameCategoryHelper
            Component.onCompleted: {
                renameCategoryHelper.setWindow(renameCategoryDialog)
            }
        }

        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)

        onVisibleChanged: {
            if (visible) {
                var mainWindow = root.Window.window
                if (mainWindow) {
                    x = mainWindow.x + (mainWindow.width - width) / 2
                    y = mainWindow.y + (mainWindow.height - height) / 2
                }
                if (categoryIndex >= 0 && categoryIndex < root.customCategories.length) {
                    renameCategoryInput.text = root.customCategories[categoryIndex].name
                }
                renameCategoryInput.forceActiveFocus()
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
            }
        }

        Rectangle {
            id: renameCategoryBg
            anchors.fill: parent
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            DialogTitleBar {
                id: renameCategoryTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                titleText: qsTr("重命名类别")
                windowRef: renameCategoryDialog
                onCloseClicked: renameCategoryDialog.close()
            }

            ColumnLayout {
                id: renameCategoryContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: renameCategoryTitleBar.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 12

                Text {
                    text: qsTr("类别名称")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.colors.mutedForeground
                }

                TextField {
                    id: renameCategoryInput
                    Layout.fillWidth: true
                    font.pixelSize: 13
                    placeholderText: qsTr("输入类别名称")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("取消")
                        variant: "ghost"
                        onClicked: renameCategoryDialog.close()
                    }

                    Button {
                        text: qsTr("保存")
                        onClicked: {
                            if (renameCategoryInput.text.trim() !== "" && renameCategoryDialog.categoryIndex >= 0) {
                                var categories = root.customCategories.slice()
                                categories[renameCategoryDialog.categoryIndex].name = renameCategoryInput.text.trim()
                                root.customCategories = categories
                                renameCategoryDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Window {
        id: renamePresetDialog
        title: qsTr("重命名风格")
        width: 320
        height: renamePresetContent.implicitHeight + 40 + 32
        color: "transparent"
        flags: Qt.Window | Qt.FramelessWindowHint
        modality: Qt.ApplicationModal
        transientParent: null
        property int presetIndex: -1

        function updateExcludeRegions() {
            renamePresetHelper.clearExcludeRegions()
            var closeBtn = renamePresetTitleBar.closeButton
            if (closeBtn && closeBtn.width > 0) {
                var posInWindow = closeBtn.mapToItem(renamePresetBg, 0, 0)
                renamePresetHelper.addExcludeRegion(posInWindow.x, posInWindow.y, closeBtn.width, closeBtn.height)
            }
        }

        SubWindowHelper {
            id: renamePresetHelper
            Component.onCompleted: {
                renamePresetHelper.setWindow(renamePresetDialog)
            }
        }

        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)

        onVisibleChanged: {
            if (visible) {
                var mainWindow = root.Window.window
                if (mainWindow) {
                    x = mainWindow.x + (mainWindow.width - width) / 2
                    y = mainWindow.y + (mainWindow.height - height) / 2
                }
                if (presetIndex >= 0 && presetIndex < root.customPresets.length) {
                    renamePresetInput.text = root.customPresets[presetIndex].name
                }
                renamePresetInput.forceActiveFocus()
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
            }
        }

        Rectangle {
            id: renamePresetBg
            anchors.fill: parent
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            DialogTitleBar {
                id: renamePresetTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                titleText: qsTr("重命名风格")
                windowRef: renamePresetDialog
                onCloseClicked: renamePresetDialog.close()
            }

            ColumnLayout {
                id: renamePresetContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: renamePresetTitleBar.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 12

                Text {
                    text: qsTr("风格名称")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.colors.mutedForeground
                }

                TextField {
                    id: renamePresetInput
                    Layout.fillWidth: true
                    font.pixelSize: 13
                    placeholderText: qsTr("输入风格名称")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("取消")
                        variant: "ghost"
                        onClicked: renamePresetDialog.close()
                    }

                    Button {
                        text: qsTr("保存")
                        onClicked: {
                            if (renamePresetInput.text.trim() !== "" && renamePresetDialog.presetIndex >= 0) {
                                var presets = root.customPresets.slice()
                                presets[renamePresetDialog.presetIndex].name = renamePresetInput.text.trim()
                                root.customPresets = presets
                                renamePresetDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Window {
        id: savePresetDialog
        title: qsTr("保存风格")
        width: 340
        height: savePresetContent.implicitHeight + 40 + 32
        color: "transparent"
        flags: Qt.Window | Qt.FramelessWindowHint
        modality: Qt.ApplicationModal
        transientParent: null

        function updateExcludeRegions() {
            savePresetHelper.clearExcludeRegions()
            var closeBtn = savePresetTitleBar.closeButton
            if (closeBtn && closeBtn.width > 0) {
                var posInWindow = closeBtn.mapToItem(savePresetBg, 0, 0)
                savePresetHelper.addExcludeRegion(posInWindow.x, posInWindow.y, closeBtn.width, closeBtn.height)
            }
        }

        SubWindowHelper {
            id: savePresetHelper
            Component.onCompleted: {
                savePresetHelper.setWindow(savePresetDialog)
            }
        }

        onWidthChanged: Qt.callLater(updateExcludeRegions)
        onHeightChanged: Qt.callLater(updateExcludeRegions)

        onVisibleChanged: {
            if (visible) {
                var mainWindow = root.Window.window
                if (mainWindow) {
                    x = mainWindow.x + (mainWindow.width - width) / 2
                    y = mainWindow.y + (mainWindow.height - height) / 2
                }
                presetNameInput.forceActiveFocus()
                categorySection.dropdownExpanded = false
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
                Qt.callLater(updateExcludeRegions)
            }
        }

        Rectangle {
            id: savePresetBg
            anchors.fill: parent
            color: Theme.colors.card
            border.width: 1
            border.color: Theme.colors.border
            radius: Theme.radius.lg

            DialogTitleBar {
                id: savePresetTitleBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                titleText: qsTr("保存风格")
                windowRef: savePresetDialog
                onCloseClicked: savePresetDialog.close()
            }

            ColumnLayout {
                id: savePresetContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: savePresetTitleBar.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 12

                Text {
                    text: qsTr("风格名称")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.colors.mutedForeground
                }

                TextField {
                    id: presetNameInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("例如：我的风格")
                    font.pixelSize: 13
                }

                Text {
                    text: qsTr("所属类别")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.colors.mutedForeground
                }

                Item {
                    id: categorySection
                    Layout.fillWidth: true
                    height: categorySelector.height

                    property bool dropdownExpanded: false
                    property real dropdownHeight: 80

                    Rectangle {
                        id: categorySelector
                        width: parent.width
                        height: 40
                        radius: Theme.radius.md
                        color: Theme.colors.input
                        border.width: 1
                        border.color: {
                            if (!categorySelector.enabled) return Theme.colors.border
                            if (categoryInput.activeFocus) return Theme.colors.ring
                            if (categorySelector.hovered) return Theme.colors.borderHover
                            return Theme.colors.inputBorder
                        }

                        property string selectedCategory: ""
                        property int selectedIndex: -1
                        property bool hovered: false

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: parent.radius + 2
                            border.width: categoryInput.activeFocus ? 2 : 0
                            border.color: Theme.colors.ring
                            color: "transparent"
                            visible: categoryInput.activeFocus
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            TextField {
                                id: categoryInput
                                Layout.fillWidth: true
                                Layout.leftMargin: 0
                                text: categorySelector.selectedCategory
                                font.pixelSize: 13
                                placeholderText: qsTr("输入或选择类别")
                                background: null
                                leftPadding: 12
                                rightPadding: 4
                                onTextChanged: {
                                    categorySelector.selectedCategory = text
                                }
                            }

                            Rectangle {
                                id: dropdownBtn
                                Layout.preferredWidth: 28
                                Layout.fillHeight: true
                                Layout.topMargin: 4
                                Layout.bottomMargin: 4
                                Layout.rightMargin: 4
                                radius: Theme.radius.sm
                                color: Theme.colors.accent

                                ColoredIcon {
                                    anchors.centerIn: parent
                                    source: Theme.icon("chevron-down")
                                    iconSize: 12
                                    color: Theme.colors.mutedForeground
                                    rotation: categorySection.dropdownExpanded ? 180 : 0
                                    Behavior on rotation { NumberAnimation { duration: 150 } }
                                }

                                MouseArea {
                                    id: dropdownBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        categorySection.dropdownExpanded = !categorySection.dropdownExpanded
                                    }
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                            onContainsMouseChanged: categorySelector.hovered = containsMouse
                        }
                    }

                    Rectangle {
                        id: categoryDropdownPanel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: categorySelector.bottom
                        anchors.topMargin: 4
                        height: categorySection.dropdownExpanded ? categorySection.dropdownHeight : 0
                        radius: Theme.radius.md
                        color: Theme.isDark ? "#1E1E2E" : "#FFFFFF"
                        border.width: 1
                        border.color: Theme.colors.border
                        visible: height > 0
                        clip: true
                        z: 10

                        Behavior on height {
                            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                        }

                        Flow {
                            id: categoryDropdownFlow
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 6
                            flow: Flow.LeftToRight

                            Repeater {
                                model: getCategoryModelWithoutCustom()

                                Rectangle {
                                    width: categoryItemText.implicitWidth + 16
                                    height: 28
                                    radius: Theme.radius.sm
                                    color: categoryItemMouse.containsMouse ? Theme.colors.primary : Theme.colors.accent

                                    Text {
                                        id: categoryItemText
                                        anchors.centerIn: parent
                                        text: modelData
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: categoryItemMouse.containsMouse ? "#FFFFFF" : Theme.colors.foreground
                                    }

                                    MouseArea {
                                        id: categoryItemMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            categorySelector.selectedCategory = modelData
                                            categorySelector.selectedIndex = index
                                            categorySection.dropdownExpanded = false
                                        }
                                    }

                                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Button {
                        text: qsTr("取消")
                        variant: "ghost"
                        onClicked: savePresetDialog.close()
                    }

                    Button {
                        text: qsTr("保存")
                        onClicked: {
                            if (presetNameInput.text.trim() !== "") {
                                var categoryText = categorySelector.selectedCategory.trim()
                                var categoryId = "custom"
                                var builtinCategoriesList = ["basic", "tone", "vintage", "cinema", "bw", "scene", "art", "social"]
                                var categoryNamesList = [qsTr("基础"), qsTr("色调"), qsTr("复古"), qsTr("电影"), qsTr("黑白"), qsTr("场景"), qsTr("艺术"), qsTr("社交")]
                                
                                var foundIndex = -1
                                for (var i = 0; i < categoryNamesList.length; i++) {
                                    if (categoryText === categoryNamesList[i]) {
                                        foundIndex = i
                                        break
                                    }
                                }
                                
                                if (foundIndex >= 0) {
                                    categoryId = builtinCategoriesList[foundIndex]
                                } else if (categoryText !== "") {
                                    var existingCategoryIndex = -1
                                    for (var j = 0; j < root.customCategories.length; j++) {
                                        if (root.customCategories[j].name === categoryText) {
                                            existingCategoryIndex = j
                                            break
                                        }
                                    }
                                    
                                    if (existingCategoryIndex === -1) {
                                        var newCategories = root.customCategories.slice()
                                        newCategories.push({ name: categoryText })
                                        root.customCategories = newCategories
                                        categoryId = "custom_" + (newCategories.length - 1)
                                    } else {
                                        categoryId = "custom_" + existingCategoryIndex
                                    }
                                }
                                
                                var newPreset = {
                                    name: presetNameInput.text.trim(),
                                    category: categoryId,
                                    categoryName: categoryText,
                                    brightness: root.brightness,
                                    contrast: root.contrast,
                                    saturation: root.saturation,
                                    hue: root.hue,
                                    sharpness: root.sharpness,
                                    blur: root.blur,
                                    denoise: root.denoise,
                                    exposure: root.exposure,
                                    gamma: root.gamma,
                                    temperature: root.temperature,
                                    tint: root.tint,
                                    vignette: root.vignette,
                                    highlights: root.highlights,
                                    shadows: root.shadows
                                }
                                var presets = [newPreset].concat(root.customPresets)
                                root.customPresets = presets
                                presetNameInput.text = ""
                                categorySelector.selectedCategory = ""
                                categorySelector.selectedIndex = -1
                                savePresetDialog.close()
                            }
                        }
                    }
                }
            }
        }
    }

    function getCategoryModelWithoutCustom() {
        var items = []
        items.push(qsTr("基础"))
        items.push(qsTr("色调"))
        items.push(qsTr("复古"))
        items.push(qsTr("电影"))
        items.push(qsTr("黑白"))
        items.push(qsTr("场景"))
        items.push(qsTr("艺术"))
        items.push(qsTr("社交"))
        for (var i = 0; i < root.customCategories.length; i++) {
            items.push(root.customCategories[i].name)
        }
        return items
    }

    function getCategoryId(index) {
        if (index === 0) return "custom"
        var builtinCategories = ["basic", "tone", "vintage", "cinema", "bw", "scene", "art", "social"]
        if (index <= builtinCategories.length) {
            return builtinCategories[index - 1]
        }
        return "custom_" + (index - builtinCategories.length - 1)
    }

    function getCategoryModel() {
        var items = [qsTr("自定义")]
        items.push(qsTr("基础"))
        items.push(qsTr("色调"))
        items.push(qsTr("复古"))
        items.push(qsTr("电影"))
        items.push(qsTr("黑白"))
        items.push(qsTr("场景"))
        items.push(qsTr("艺术"))
        items.push(qsTr("社交"))
        for (var i = 0; i < root.customCategories.length; i++) {
            items.push(root.customCategories[i].name)
        }
        return items
    }

    ColumnLayout {
        id: paramsContent
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        ParamGroup {
            id: basicGroup
            Layout.fillWidth: true
            title: qsTr("基础调整")
            icon: "adjust-basic"
            hasModifiedValues: Math.abs(brightness) > 0.001 || Math.abs(contrast - 1.0) > 0.001 || Math.abs(saturation - 1.0) > 0.001 || Math.abs(hue) > 0.001

            ColumnLayout {
                spacing: 8

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("亮度")
                    from: -1.0
                    to: 1.0
                    externalValue: root.brightness
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.brightness = newValue
                        root.paramChanged("brightness", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("对比度")
                    from: 0.0
                    to: 3.0
                    externalValue: root.contrast
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.contrast = newValue
                        root.paramChanged("contrast", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("饱和度")
                    from: 0.0
                    to: 3.0
                    externalValue: root.saturation
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.saturation = newValue
                        root.paramChanged("saturation", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("色相")
                    from: -0.5
                    to: 0.5
                    externalValue: root.hue
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.hue = newValue
                        root.paramChanged("hue", newValue)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 6

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        width: resetText.implicitWidth + 12
                        height: 18
                        radius: Theme.radius.xs
                        color: resetBasicMouse.containsMouse ? Theme.colors.destructiveSubtle : "transparent"
                        visible: basicGroup.hasModifiedValues

                        Text {
                            id: resetText
                            anchors.centerIn: parent
                            text: qsTr("重置")
                            font.pixelSize: 9
                            color: Theme.colors.destructive
                        }

                        MouseArea {
                            id: resetBasicMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                brightness = 0.0
                                contrast = 1.0
                                saturation = 1.0
                                hue = 0.0
                                root.paramChanged("brightness", 0.0)
                                root.paramChanged("contrast", 1.0)
                                root.paramChanged("saturation", 1.0)
                                root.paramChanged("hue", 0.0)
                            }
                        }
                    }
                }
            }
        }

        ParamGroup {
            id: detailGroup
            Layout.fillWidth: true
            title: qsTr("细节增强")
            icon: "detail-enhance"
            hasModifiedValues: Math.abs(sharpness) > 0.001 || Math.abs(blur) > 0.001 || Math.abs(denoise) > 0.001

            ColumnLayout {
                spacing: 8

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("锐度")
                    from: 0.0
                    to: 2.0
                    externalValue: root.sharpness
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.sharpness = newValue
                        root.paramChanged("sharpness", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("模糊")
                    from: 0.0
                    to: 1.0
                    externalValue: root.blur
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.blur = newValue
                        root.paramChanged("blur", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("降噪")
                    from: 0.0
                    to: 1.0
                    externalValue: root.denoise
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.denoise = newValue
                        root.paramChanged("denoise", newValue)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 6

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        width: resetText2.implicitWidth + 12
                        height: 18
                        radius: Theme.radius.xs
                        color: resetDetailMouse.containsMouse ? Theme.colors.destructiveSubtle : "transparent"
                        visible: detailGroup.hasModifiedValues

                        Text {
                            id: resetText2
                            anchors.centerIn: parent
                            text: qsTr("重置")
                            font.pixelSize: 9
                            color: Theme.colors.destructive
                        }

                        MouseArea {
                            id: resetDetailMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                sharpness = 0.0
                                blur = 0.0
                                denoise = 0.0
                                root.paramChanged("sharpness", 0.0)
                                root.paramChanged("blur", 0.0)
                                root.paramChanged("denoise", 0.0)
                            }
                        }
                    }
                }
            }
        }

        ParamGroup {
            id: lightGroup
            Layout.fillWidth: true
            title: qsTr("光影调整")
            icon: "light-shadow"
            hasModifiedValues: Math.abs(exposure) > 0.001 || Math.abs(gamma - 1.0) > 0.001 || Math.abs(highlights) > 0.001 || Math.abs(shadows) > 0.001

            ColumnLayout {
                spacing: 4

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("曝光")
                    from: -2.0
                    to: 2.0
                    externalValue: root.exposure
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.exposure = newValue
                        root.paramChanged("exposure", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("伽马")
                    from: 0.3
                    to: 3.0
                    externalValue: root.gamma
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.gamma = newValue
                        root.paramChanged("gamma", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("高光")
                    from: -1.0
                    to: 1.0
                    externalValue: root.highlights
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.highlights = newValue
                        root.paramChanged("highlights", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("阴影")
                    from: -1.0
                    to: 1.0
                    externalValue: root.shadows
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.shadows = newValue
                        root.paramChanged("shadows", newValue)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 6

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        width: resetText3.implicitWidth + 12
                        height: 18
                        radius: Theme.radius.xs
                        color: resetLightMouse.containsMouse ? Theme.colors.destructiveSubtle : "transparent"
                        visible: lightGroup.hasModifiedValues

                        Text {
                            id: resetText3
                            anchors.centerIn: parent
                            text: qsTr("重置")
                            font.pixelSize: 9
                            color: Theme.colors.destructive
                        }

                        MouseArea {
                            id: resetLightMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                exposure = 0.0
                                gamma = 1.0
                                highlights = 0.0
                                shadows = 0.0
                                root.paramChanged("exposure", 0.0)
                                root.paramChanged("gamma", 1.0)
                                root.paramChanged("highlights", 0.0)
                                root.paramChanged("shadows", 0.0)
                            }
                        }
                    }
                }
            }
        }

        ParamGroup {
            id: colorGroup
            Layout.fillWidth: true
            title: qsTr("色彩调整")
            icon: "color-palette"
            hasModifiedValues: Math.abs(temperature) > 0.001 || Math.abs(tint) > 0.001 || Math.abs(vignette) > 0.001

            ColumnLayout {
                spacing: 4

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("色温")
                    from: -1.0
                    to: 1.0
                    externalValue: root.temperature
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.temperature = newValue
                        root.paramChanged("temperature", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("色调")
                    from: -1.0
                    to: 1.0
                    externalValue: root.tint
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.tint = newValue
                        root.paramChanged("tint", newValue)
                    }
                }

                ParamSlider {
                    Layout.fillWidth: true
                    paramName: qsTr("晕影")
                    from: 0.0
                    to: 1.0
                    externalValue: root.vignette
                    stepSize: 0.01
                    decimals: 2
                    onParamValueChanged: function(newValue) {
                        root.vignette = newValue
                        root.paramChanged("vignette", newValue)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 6

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        width: resetText4.implicitWidth + 12
                        height: 18
                        radius: Theme.radius.xs
                        color: resetColorMouse.containsMouse ? Theme.colors.destructiveSubtle : "transparent"
                        visible: colorGroup.hasModifiedValues

                        Text {
                            id: resetText4
                            anchors.centerIn: parent
                            text: qsTr("重置")
                            font.pixelSize: 9
                            color: Theme.colors.destructive
                        }

                        MouseArea {
                            id: resetColorMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                temperature = 0.0
                                tint = 0.0
                                vignette = 0.0
                                root.paramChanged("temperature", 0.0)
                                root.paramChanged("tint", 0.0)
                                root.paramChanged("vignette", 0.0)
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.preferredHeight: 8 }
    }

    function hasAnyModification() {
        return brightness !== 0.0 || contrast !== 1.0 || saturation !== 1.0 || hue !== 0.0 ||
               sharpness !== 0.0 || blur !== 0.0 || denoise !== 0.0 ||
               exposure !== 0.0 || gamma !== 1.0 || highlights !== 0.0 || shadows !== 0.0 ||
               temperature !== 0.0 || tint !== 0.0 || vignette !== 0.0
    }

    function applyPreset(preset) {
        currentPreset = preset
        currentPresetName = preset.name
        
        var newBrightness = preset.brightness !== undefined ? preset.brightness : 0.0
        var newContrast = preset.contrast !== undefined ? preset.contrast : 1.0
        var newSaturation = preset.saturation !== undefined ? preset.saturation : 1.0
        var newSharpness = preset.sharpness !== undefined ? preset.sharpness : 0.0
        var newTemperature = preset.temperature !== undefined ? preset.temperature : 0.0
        var newVignette = preset.vignette !== undefined ? preset.vignette : 0.0
        var newHue = preset.hue !== undefined ? preset.hue : 0.0
        var newBlur = preset.blur !== undefined ? preset.blur : 0.0
        var newDenoise = preset.denoise !== undefined ? preset.denoise : 0.0
        var newExposure = preset.exposure !== undefined ? preset.exposure : 0.0
        var newGamma = preset.gamma !== undefined ? preset.gamma : 1.0
        var newTint = preset.tint !== undefined ? preset.tint : 0.0
        var newHighlights = preset.highlights !== undefined ? preset.highlights : 0.0
        var newShadows = preset.shadows !== undefined ? preset.shadows : 0.0
        
        root.paramChanged("brightness", newBrightness)
        root.paramChanged("contrast", newContrast)
        root.paramChanged("saturation", newSaturation)
        root.paramChanged("hue", newHue)
        root.paramChanged("sharpness", newSharpness)
        root.paramChanged("blur", newBlur)
        root.paramChanged("denoise", newDenoise)
        root.paramChanged("exposure", newExposure)
        root.paramChanged("gamma", newGamma)
        root.paramChanged("temperature", newTemperature)
        root.paramChanged("tint", newTint)
        root.paramChanged("vignette", newVignette)
        root.paramChanged("highlights", newHighlights)
        root.paramChanged("shadows", newShadows)
        
        brightness = newBrightness
        contrast = newContrast
        saturation = newSaturation
        sharpness = newSharpness
        temperature = newTemperature
        vignette = newVignette
        hue = newHue
        blur = newBlur
        denoise = newDenoise
        exposure = newExposure
        gamma = newGamma
        tint = newTint
        highlights = newHighlights
        shadows = newShadows
        
        }

    function resetAll() {
        currentPreset = null
        currentPresetName = ""
        brightness = 0.0
        contrast = 1.0
        saturation = 1.0
        hue = 0.0
        sharpness = 0.0
        blur = 0.0
        denoise = 0.0
        exposure = 0.0
        gamma = 1.0
        temperature = 0.0
        tint = 0.0
        vignette = 0.0
        highlights = 0.0
        shadows = 0.0
    }
}

