import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"
import "../controls"

/**
 * @brief 预览面板组件
 * 
 * 显示媒体文件的预览效果，参考功能设计文档 0.2 节 3.6 多媒体查看模块
 */
Rectangle {
    id: root
    
    // ========== 属性 ==========
    property bool showOriginal: true
    
    // ========== 视觉属性 ==========
    color: Theme.colors.card
    border.width: 1
    border.color: Theme.colors.cardBorder
    radius: Theme.radius.lg
    
    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10
        
        // ========== 顶部：标题和切换按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Image {
                width: 16; height: 16
                source: Theme.icon("eye")
                sourceSize: Qt.size(16, 16)
                smooth: true
            }
            
            Text {
                text: qsTr("预览")
                color: Theme.colors.foreground
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            
            Item { Layout.fillWidth: true }
            
            // 切换按钮组
            Row {
                spacing: 2
                
                // 原效果按钮
                Rectangle {
                    width: origText.implicitWidth + 20
                    height: 28
                    radius: Theme.radius.sm
                    color: root.showOriginal ? Theme.colors.primary : "transparent"
                    border.width: root.showOriginal ? 0 : 1
                    border.color: Theme.colors.border
                    
                    Text {
                        id: origText
                        anchors.centerIn: parent
                        text: qsTr("原图")
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: root.showOriginal ? "#FFFFFF" : Theme.colors.mutedForeground
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showOriginal = true
                    }
                    
                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                }
                
                // 处理效果按钮
                Rectangle {
                    width: procText.implicitWidth + 20
                    height: 28
                    radius: Theme.radius.sm
                    color: !root.showOriginal ? Theme.colors.primary : "transparent"
                    border.width: !root.showOriginal ? 0 : 1
                    border.color: Theme.colors.border
                    
                    Text {
                        id: procText
                        anchors.centerIn: parent
                        text: qsTr("效果")
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: !root.showOriginal ? "#FFFFFF" : Theme.colors.mutedForeground
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showOriginal = false
                    }
                    
                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                }
            }
        }
        
        // ========== 预览区域 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.colors.background
            radius: Theme.radius.md
            border.width: 1
            border.color: Theme.colors.border
            
            // 空状态占位
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 16
                
                // 图标容器
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 72; height: 72
                    radius: 36
                    color: Theme.colors.primarySubtle
                    
                    Image {
                        anchors.centerIn: parent
                        width: 28; height: 28
                        source: Theme.icon("image")
                        sourceSize: Qt.size(28, 28)
                        smooth: true
                    }
                }
                
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("选择文件以预览")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }
                
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("支持 JPG、PNG、BMP、WebP、MP4 等格式")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 12
                    opacity: 0.7
                }
            }
        }
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
