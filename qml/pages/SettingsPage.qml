import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 设置页面组件
 * 
 * 提供应用程序设置功能，参考功能设计文档 0.2 节 3.12 设置中心模块
 */
Rectangle {
    id: root

    // ========== 信号 ==========
    signal goBack()

    // ========== 视觉属性 ==========
    color: Theme.colors.background

    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 20

        // ========== 顶部：返回按钮和标题 ==========
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

        // ========== 设置内容区域 ==========
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

                // ========== 外观设置 ==========
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
                            Image { width: 18; height: 18; source: Theme.icon("monitor"); sourceSize: Qt.size(18, 18); smooth: true }
                            Text { text: qsTr("外观"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        // 主题切换
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("主题"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.preferredWidth: 100 }

                            // 主题选择按钮组
                            Row {
                                spacing: 4

                                Rectangle {
                                    width: darkLabel.implicitWidth + 24; height: 32
                                    radius: Theme.radius.md
                                    color: Theme.isDark ? Theme.colors.primary : "transparent"
                                    border.width: Theme.isDark ? 0 : 1
                                    border.color: Theme.colors.border

                                    Row {
                                        anchors.centerIn: parent; spacing: 6
                                        Image { anchors.verticalCenter: parent.verticalCenter; width: 14; height: 14; source: Theme.icon("moon"); sourceSize: Qt.size(14, 14); smooth: true }
                                        Text { id: darkLabel; anchors.verticalCenter: parent.verticalCenter; text: qsTr("暗色"); font.pixelSize: 12; font.weight: Font.Medium; color: Theme.isDark ? "#FFFFFF" : Theme.colors.foreground }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.setDark(true) }
                                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                                }

                                Rectangle {
                                    width: lightLabel.implicitWidth + 24; height: 32
                                    radius: Theme.radius.md
                                    color: !Theme.isDark ? Theme.colors.primary : "transparent"
                                    border.width: !Theme.isDark ? 0 : 1
                                    border.color: Theme.colors.border

                                    Row {
                                        anchors.centerIn: parent; spacing: 6
                                        Image { anchors.verticalCenter: parent.verticalCenter; width: 14; height: 14; source: Theme.icon("sun"); sourceSize: Qt.size(14, 14); smooth: true }
                                        Text { id: lightLabel; anchors.verticalCenter: parent.verticalCenter; text: qsTr("亮色"); font.pixelSize: 12; font.weight: Font.Medium; color: !Theme.isDark ? "#FFFFFF" : Theme.colors.foreground }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Theme.setDark(false) }
                                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                                }
                            }
                        }

                        // 语言切换
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Text { text: qsTr("语言"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.preferredWidth: 100 }

                            ComboBox {
                                model: ["简体中文", "English"]
                                currentIndex: Theme.language === "zh_CN" ? 0 : 1
                                onCurrentIndexChanged: {
                                    Theme.setLanguage(currentIndex === 0 ? "zh_CN" : "en_US")
                                }
                            }
                        }
                    }
                }

                // ========== 性能设置 ==========
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
                            Image { width: 18; height: 18; source: Theme.icon("cpu"); sourceSize: Qt.size(18, 18); smooth: true }
                            Text { text: qsTr("性能"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        // 最大并发任务
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: qsTr("最大并发任务"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.preferredWidth: 100 }
                            ComboBox { model: ["1", "2", "3", "4"]; currentIndex: 1 }
                        }

                        // 清除缓存
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Text { text: qsTr("缓存管理"); color: Theme.colors.foreground; font.pixelSize: 13; Layout.preferredWidth: 100 }
                            Button { text: qsTr("清除缩略图缓存"); variant: "secondary"; size: "sm"; iconName: "trash" }
                        }
                    }
                }

                // ========== 关于 ==========
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
                            Image { width: 18; height: 18; source: Theme.icon("info"); sourceSize: Qt.size(18, 18); smooth: true }
                            Text { text: qsTr("关于"); color: Theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.colors.border }

                        RowLayout {
                            spacing: 12

                            // 品牌图标
                            Rectangle {
                                width: 44; height: 44; radius: 11
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#002FA7" }
                                    GradientStop { position: 1.0; color: "#1A56DB" }
                                }
                                Text { anchors.centerIn: parent; text: "E"; color: "#FFFFFF"; font.pixelSize: 22; font.weight: Font.Bold }
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

    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
