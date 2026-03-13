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
    
    // ========== 视觉属性 ==========
    color: Theme.colors.background
    
    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing._6
        spacing: Theme.spacing._4
        
        // ========== 顶部：返回按钮和标题 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._3
            
            IconButton {
                iconSource: "←"
                tooltip: qsTr("返回")
                onClicked: {
                    if (parent && parent.parent && parent.parent.currentPage !== undefined) {
                        parent.parent.currentPage = 0
                    }
                }
            }
            
            Text {
                text: qsTr("设置")
                color: Theme.colors.foreground
                font.pixelSize: 24
                font.bold: true
            }
        }
        
        // ========== 设置内容区域 ==========
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            ColumnLayout {
                id: settingsColumn
                spacing: Theme.spacing._4
                
                // 使用 Binding 避免递归布局
                Binding {
                    target: settingsColumn
                    property: "width"
                    value: settingsColumn.parent.width
                    when: settingsColumn.parent
                }
                
                // ========== 外观设置 ==========
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.colors.card
                    border {
                        width: 1
                        color: Theme.colors.border
                    }
                    radius: Theme.radius.lg
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing._4
                        spacing: Theme.spacing._3
                        
                        Text {
                            text: qsTr("外观")
                            color: Theme.colors.foreground
                            font.pixelSize: 16
                            font.bold: true
                        }
                        
                        // 主题切换
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing._2
                            
                            Text {
                                text: qsTr("主题:")
                                color: Theme.colors.foreground
                                font.pixelSize: 13
                                Layout.preferredWidth: 80
                            }
                            
                            Button {
                                text: Theme.isDark ? qsTr("暗色") : qsTr("亮色")
                                onClicked: Theme.toggle()
                            }
                        }
                        
                        // 语言切换
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing._2
                            
                            Text {
                                text: qsTr("语言:")
                                color: Theme.colors.foreground
                                font.pixelSize: 13
                                Layout.preferredWidth: 80
                            }
                            
                            ComboBox {
                                model: [qsTr("简体中文"), qsTr("English")]
                                currentIndex: 0
                            }
                        }
                    }
                }
                
                // ========== 性能设置 ==========
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.colors.card
                    border {
                        width: 1
                        color: Theme.colors.border
                    }
                    radius: Theme.radius.lg
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing._4
                        spacing: Theme.spacing._3
                        
                        Text {
                            text: qsTr("性能")
                            color: Theme.colors.foreground
                            font.pixelSize: 16
                            font.bold: true
                        }
                        
                        // 最大并发任务
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing._2
                            
                            Text {
                                text: qsTr("最大并发任务:")
                                color: Theme.colors.foreground
                                font.pixelSize: 13
                                Layout.preferredWidth: 120
                            }
                            
                            ComboBox {
                                model: ["1", "2", "3", "4"]
                                currentIndex: 1
                            }
                        }
                        
                        // 清除缓存按钮
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacing._2
                            
                            Text {
                                text: qsTr("缓存管理:")
                                color: Theme.colors.foreground
                                font.pixelSize: 13
                                Layout.preferredWidth: 120
                            }
                            
                            Button {
                                text: qsTr("清除缩略图缓存")
                            }
                        }
                    }
                }
                
                // ========== 关于 ==========
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.colors.card
                    border {
                        width: 1
                        color: Theme.colors.border
                    }
                    radius: Theme.radius.lg
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing._4
                        spacing: Theme.spacing._2
                        
                        Text {
                            text: qsTr("关于")
                            color: Theme.colors.foreground
                            font.pixelSize: 16
                            font.bold: true
                        }
                        
                        Text {
                            text: qsTr("EnhanceVision v1.0.0")
                            color: Theme.colors.foreground
                            font.pixelSize: 14
                        }
                        
                        Text {
                            text: qsTr("高性能图像与视频增强工具")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
