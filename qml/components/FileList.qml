import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../styles"
import "../controls"

/**
 * @brief 文件列表组件
 * 
 * 管理导入的媒体文件，参考功能设计文档 0.2 节 3.3 文件导入模块
 */
Rectangle {
    id: root
    
    // ========== 视觉属性 ==========
    color: Theme.colors.card
    border.width: 1
    border.color: Theme.colors.cardBorder
    radius: Theme.radius.lg
    
    // ========== 内部布局 ==========
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        
        // ========== 顶部：标题和按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Image {
                width: 16; height: 16
                source: Theme.icon("folder")
                sourceSize: Qt.size(16, 16)
                smooth: true
            }
            
            Text {
                text: qsTr("文件列表")
                color: Theme.colors.foreground
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("添加文件")
                iconName: "file-plus"
                variant: "secondary"
                size: "sm"
                onClicked: fileDialog.open()
            }
        }
        
        // ========== 文件列表视图 ==========
        DropArea {
            id: dropArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 拖拽高亮
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: dropArea.containsDrag ? 2 : 0
                border.color: Theme.colors.primary
                radius: Theme.radius.md
                visible: dropArea.containsDrag
                
                Rectangle {
                    anchors.fill: parent
                    color: Theme.colors.primarySubtle
                    radius: parent.radius
                    opacity: 0.5
                }
            }
            
            // 文件选择对话框
            FileDialog {
                id: fileDialog
                title: qsTr("选择媒体文件")
                fileMode: FileDialog.OpenFiles
                nameFilters: [
                    qsTr("所有支持的文件 (*.jpg *.jpeg *.png *.bmp *.webp *.tiff *.tif *.mp4 *.avi *.mkv *.mov *.flv)"),
                    qsTr("图片文件 (*.jpg *.jpeg *.png *.bmp *.webp *.tiff *.tif)"),
                    qsTr("视频文件 (*.mp4 *.avi *.mkv *.mov *.flv)"),
                    qsTr("所有文件 (*.*)")
                ]
                
                onAccepted: {
                    fileController.addFiles(fileDialog.selectedFiles)
                }
            }
            
            // 处理拖拽事件
            onDropped: function(drop) {
                if (drop.hasUrls) {
                    fileController.addFiles(drop.urls)
                }
            }
            
            ListView {
                id: fileListView
                anchors.fill: parent
                clip: true
                spacing: 6
                orientation: ListView.Horizontal
                model: fileModel
                
                // 文件项委托
                delegate: Rectangle {
                    id: fileCard
                    width: 130
                    height: fileListView.height - 4
                    radius: Theme.radius.md
                    color: cardMouse.containsMouse ? Theme.colors.surfaceHover : Theme.colors.surface
                    border.width: 1
                    border.color: cardMouse.containsMouse ? Theme.colors.primary : Theme.colors.border
                    
                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
                    
                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: console.log("选中文件:", model.fileName)
                    }
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6
                        
                        // 文件缩略图区域
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            radius: Theme.radius.sm
                            color: Theme.colors.muted
                            clip: true
                            
                            Image {
                                anchors.centerIn: parent
                                width: 24; height: 24
                                source: model.mediaType === 0 ? Theme.icon("image") : Theme.icon("video")
                                sourceSize: Qt.size(24, 24)
                                smooth: true
                            }
                        }
                        
                        // 文件名
                        Text {
                            Layout.fillWidth: true
                            text: model.fileName
                            color: Theme.colors.foreground
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            elide: Text.ElideMiddle
                            maximumLineCount: 1
                        }
                        
                        // 文件大小
                        Text {
                            Layout.fillWidth: true
                            text: model.fileSizeDisplay
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 10
                        }
                        
                        // 状态指示条
                        Rectangle {
                            Layout.fillWidth: true
                            height: 3
                            radius: 1.5
                            color: {
                                switch(model.status) {
                                    case 0: return Theme.colors.muted
                                    case 1: return Theme.colors.primary
                                    case 2: return Theme.colors.success
                                    case 3: return Theme.colors.destructive
                                    default: return Theme.colors.muted
                                }
                            }
                        }
                    }
                    
                    // 删除按钮
                    IconButton {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 2
                        iconName: "x"
                        iconSize: 10
                        btnSize: 20
                        visible: cardMouse.containsMouse
                        danger: true
                        onClicked: fileController.removeFileAt(index)
                    }
                }
                
                // 空状态提示
                Item {
                    visible: fileListView.count === 0
                    anchors.fill: parent
                    
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Image {
                            Layout.alignment: Qt.AlignHCenter
                            width: 28; height: 28
                            source: Theme.icon("upload")
                            sourceSize: Qt.size(28, 28)
                            smooth: true
                            opacity: 0.5
                        }
                        
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("拖拽文件到此处或点击添加")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
