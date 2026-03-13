import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

/**
 * @brief 预览面板组件
 * 
 * 显示媒体文件的预览效果，参考功能设计文档 0.2 节 3.6 多媒体查看模块
 */
Rectangle {
    id: root
    
    // ========== 视觉属性 ==========
    color: Theme.colors.card
    border {
        width: 1
        color: Theme.colors.border
    }
    radius: Theme.radius.md
    
    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing._3
        spacing: Theme.spacing._2
        
        // ========== 顶部：模式切换和标题 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2
            
            Text {
                text: qsTr("预览")
                color: Theme.colors.foreground
                font.pixelSize: 14
                font.bold: true
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            // 原效果/处理效果切换
            Button {
                text: qsTr("原效果")
                flat: true
                onClicked: {
                    // TODO: 切换显示原效果
                    console.log("显示原效果")
                }
            }
            
            Button {
                text: qsTr("处理效果")
                flat: true
                onClicked: {
                    // TODO: 切换显示处理效果
                    console.log("显示处理效果")
                }
            }
        }
        
        // ========== 分隔线 ==========
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }
        
        // ========== 预览区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.colors.background
            radius: Theme.radius.sm
            
            // 预览内容占位
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.spacing._2
                
                Rectangle {
                    width: 200
                    height: 150
                    radius: Theme.radius.md
                    color: Theme.colors.primarySubtle
                    border {
                        width: 2
                        color: Theme.colors.primary
                    }
                    
                    Text {
                        anchors.centerIn: parent
                        text: "🖼️"
                        font.pixelSize: 48
                    }
                }
                
                Text {
                    text: qsTr("选择文件以预览")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 13
                }
            }
        }
    }
}
