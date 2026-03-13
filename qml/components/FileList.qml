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
        
        // ========== 顶部：标题和按钮 ==========
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing._2
            
            Text {
                text: qsTr("文件列表")
                color: Theme.colors.foreground
                font.pixelSize: 14
                font.bold: true
            }
            
            Item {
                Layout.fillWidth: true
            }
            
            Button {
                text: qsTr("添加文件")
                onClicked: fileDialog.open()
            }
        }
        
        // ========== 分隔线 ==========
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.colors.border
        }
        
        // ========== 文件列表视图 ==========
        DropArea {
            id: dropArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 拖拽进入时显示高亮边框
            Rectangle {
                id: dropHighlight
                anchors.fill: parent
                color: "transparent"
                border {
                    width: dropArea.containsDrag ? 2 : 0
                    color: Theme.colors.primary
                }
                radius: Theme.radius.sm
                visible: dropArea.containsDrag
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
                anchors.margins: Theme.spacing._1
                clip: true
                spacing: Theme.spacing._2
                orientation: ListView.Horizontal
                model: fileModel
                
                // 文件项委托
                delegate: Item {
                    id: fileItem
                    width: 140
                    height: fileListView.height
                    
                    // 文件卡片
                    Rectangle {
                        id: fileCard
                        anchors.fill: parent
                        anchors.margins: Theme.spacing._1
                        radius: Theme.radius.sm
                        color: Theme.colors.surface
                        border {
                            width: 1
                            color: Theme.colors.border
                        }
                        
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacing._2
                            spacing: Theme.spacing._1
                            
                            // 文件预览/缩略图
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 80
                                radius: Theme.radius.sm
                                color: Theme.colors.muted
                                clip: true
                                
                                // 显示缩略图
                                Item {
                                    id: thumbnailContainer
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    
                                    property var thumbnailImage: model.thumbnail
                                    
                                    // 如果有缩略图，直接显示
                                    Canvas {
                                        id: thumbnailCanvas
                                        anchors.fill: parent
                                        visible: !thumbnailContainer.thumbnailImage.isNull
                                        
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);
                                            
                                            if (!thumbnailContainer.thumbnailImage.isNull) {
                                                // 计算缩放比例，保持宽高比
                                                var imgWidth = thumbnailContainer.thumbnailImage.width;
                                                var imgHeight = thumbnailContainer.thumbnailImage.height;
                                                var scale = Math.min(width / imgWidth, height / imgHeight);
                                                var scaledWidth = imgWidth * scale;
                                                var scaledHeight = imgHeight * scale;
                                                var x = (width - scaledWidth) / 2;
                                                var y = (height - scaledHeight) / 2;
                                                
                                                ctx.drawImage(thumbnailContainer.thumbnailImage, x, y, scaledWidth, scaledHeight);
                                            }
                                        }
                                    }
                                    
                                    // 如果没有缩略图，显示占位图标
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: Qt.rgba(0, 0, 0, 0.5)
                                        visible: thumbnailContainer.thumbnailImage.isNull
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: model.mediaType === 0 ? "🖼️" : "🎬"
                                            font.pixelSize: 20
                                        }
                                    }
                                }
                            }
                            
                            // 文件名
                            Text {
                                Layout.fillWidth: true
                                text: model.fileName
                                color: Theme.colors.foreground
                                font.pixelSize: 11
                                elide: Text.ElideMiddle
                                maximumLineCount: 2
                                wrapMode: Text.WordWrap
                            }
                            
                            // 文件大小
                            Text {
                                Layout.fillWidth: true
                                text: model.fileSizeDisplay
                                color: Theme.colors.mutedForeground
                                font.pixelSize: 10
                            }
                            
                            // 状态指示器
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 4
                                radius: 2
                                color: {
                                    switch(model.status) {
                                        case 0: return Theme.colors.muted;    // Pending
                                        case 1: return Theme.colors.primary;  // Processing
                                        case 2: return Theme.colors.success;  // Completed
                                        case 3: return Theme.colors.destructive;  // Failed
                                        case 4: return Theme.colors.mutedForeground;  // Cancelled
                                        default: return Theme.colors.muted;
                                    }
                                }
                            }
                        }
                        
                        // 删除按钮
                        IconButton {
                            id: deleteButton
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 4
                            anchors.rightMargin: 4
                            iconSource: "×"
                            tooltip: qsTr("删除文件")
                            width: 24
                            height: 24
                            onClicked: {
                                fileController.removeFileAt(index)
                            }
                        }
                    }
                    
                    // 鼠标悬停效果
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            console.log("选中文件:", model.fileName)
                        }
                    }
                }
                
                // 空状态提示
                Rectangle {
                    id: emptyState
                    visible: fileListView.count === 0
                    anchors.centerIn: parent
                    color: "transparent"
                    
                    ColumnLayout {
                        spacing: Theme.spacing._2
                        
                        Text {
                            text: qsTr("暂无文件")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            Layout.alignment: Qt.AlignHCenter
                        }
                        
                        Text {
                            text: qsTr("点击\"添加文件\"或拖拽文件到此处")
                            color: Theme.colors.mutedForeground
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
