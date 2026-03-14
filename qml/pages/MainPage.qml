import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../styles"
import "../components"
import "../controls"

/**
 * @brief 主页面组件
 * 
 * 主应用页面，包含消息展示区、底部文件预览和操作按钮
 * 参考功能设计文档 0.2 和 UI 设计文档 08-ui-design.md
 */
Rectangle {
    id: root
    color: Theme.colors.background
    
    // ========== 属性定义 ==========
    property int processingMode: 0  // 0: Shader, 1: AI
    property bool hasFiles: fileModel.count > 0
    property bool hasMessages: processingModel.count > 0
    
    // ========== 信号 ==========
    signal expandControlPanel()
    
    // ========== 主布局 ==========
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // ========== 中间消息展示区 ==========
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 空状态欢迎界面
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 24
                visible: !root.hasMessages && !root.hasFiles
                
                // 欢迎图标
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 80
                    height: 80
                    radius: 20
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#002FA7" }
                        GradientStop { position: 1.0; color: "#1E56D0" }
                    }
                    
                    ColoredIcon {
                        anchors.centerIn: parent
                        source: Theme.icon("sparkles")
                        iconSize: 36
                        color: "#FFFFFF"
                    }
                }
                
                // 欢迎标题
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("开始新的处理任务")
                    color: Theme.colors.foreground
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
                
                // 欢迎描述
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("添加文件并选择处理模式来开始")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 14
                }
                
                // 添加文件按钮
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: addFileRow.implicitWidth + 32
                    height: 44
                    radius: Theme.radius.lg
                    color: Theme.colors.primary
                    
                    Row {
                        id: addFileRow
                        anchors.centerIn: parent
                        spacing: 8
                        
                        ColoredIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            source: Theme.icon("upload")
                            iconSize: 18
                            color: "#FFFFFF"
                        }
                        
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("添加文件")
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileDialog.open()
                    }
                    
                    Behavior on scale { NumberAnimation { duration: 100 } }
                }
                
                // 快捷键提示
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Ctrl+O 添加文件，Ctrl+N 新会话")
                    color: Theme.colors.mutedForeground
                    font.pixelSize: 12
                    opacity: 0.7
                }
            }
            
            // 消息列表（有消息时显示）
            MessageList {
                anchors.fill: parent
                anchors.margins: 12
                visible: root.hasMessages
            }
        }
        
        // ========== 底部待处理文件预览区 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.hasFiles ? 100 : 0
            visible: root.hasFiles
            color: Theme.colors.card
            
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.border
            }
            
            // 文件缩略图列表
            ListView {
                id: pendingFileList
                anchors.fill: parent
                anchors.margins: 10
                orientation: ListView.Horizontal
                spacing: 8
                clip: true
                model: fileModel
                
                delegate: Rectangle {
                    width: 80
                    height: 80
                    radius: Theme.radius.md
                    color: fileMouse.containsMouse ? Theme.colors.surfaceHover : Theme.colors.surface
                    border.width: 1
                    border.color: fileMouse.containsMouse ? Theme.colors.primary : Theme.colors.border
                    
                    MouseArea {
                        id: fileMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                    
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4
                        
                        // 缩略图
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: Theme.radius.sm
                            color: Theme.colors.muted
                            clip: true
                            
                            ColoredIcon {
                                anchors.centerIn: parent
                                source: model.mediaType === 0 ? Theme.icon("image") : Theme.icon("video")
                                iconSize: 20
                                color: Theme.colors.mutedForeground
                            }
                        }
                        
                        // 文件名
                        Text {
                            Layout.fillWidth: true
                            text: model.fileName
                            color: Theme.colors.foreground
                            font.pixelSize: 9
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    
                    // 删除按钮
                    IconButton {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: -4
                        visible: fileMouse.containsMouse
                        iconName: "x"
                        iconSize: 10
                        btnSize: 18
                        danger: true
                        onClicked: fileController.removeFileAt(index)
                    }
                    
                    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
                }
            }
        }
        
        // ========== 底部操作栏 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.colors.card
            
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.border
            }
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12
                
                // ========== 模式选择 ==========
                RowLayout {
                    spacing: 4
                    
                    // Shader 模式按钮
                    Rectangle {
                        width: shaderRow.implicitWidth + 16
                        height: 36
                        radius: Theme.radius.md
                        color: root.processingMode === 0 ? Theme.colors.primary : "transparent"
                        border.width: root.processingMode === 0 ? 0 : 1
                        border.color: Theme.colors.border
                        
                        Row {
                            id: shaderRow
                            anchors.centerIn: parent
                            spacing: 6
                            
                            ColoredIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                source: Theme.icon("sliders")
                                iconSize: 14
                                color: root.processingMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Shader"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: root.processingMode === 0 ? "#FFFFFF" : Theme.colors.foreground
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.processingMode = 0
                                root.expandControlPanel()
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                    
                    // AI 模式按钮
                    Rectangle {
                        width: aiRow.implicitWidth + 16
                        height: 36
                        radius: Theme.radius.md
                        color: root.processingMode === 1 ? Theme.colors.primary : "transparent"
                        border.width: root.processingMode === 1 ? 0 : 1
                        border.color: Theme.colors.border
                        
                        Row {
                            id: aiRow
                            anchors.centerIn: parent
                            spacing: 6
                            
                            ColoredIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                source: Theme.icon("sparkles")
                                iconSize: 14
                                color: root.processingMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                            }
                            
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("AI推理")
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: root.processingMode === 1 ? "#FFFFFF" : Theme.colors.foreground
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.processingMode = 1
                                root.expandControlPanel()
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // ========== 右侧操作按钮 ==========
                RowLayout {
                    spacing: 8
                    
                    // 上传按钮
                    IconButton {
                        iconName: "upload"
                        iconSize: 18
                        btnSize: 40
                        tooltip: qsTr("添加文件")
                        onClicked: fileDialog.open()
                    }
                    
                    // 发送按钮
                    Rectangle {
                        width: 40
                        height: 40
                        radius: Theme.radius.md
                        color: root.hasFiles ? Theme.colors.primary : Theme.colors.muted
                        enabled: root.hasFiles
                        
                        ColoredIcon {
                            anchors.centerIn: parent
                            source: Theme.icon("send")
                            iconSize: 18
                            color: root.hasFiles ? "#FFFFFF" : Theme.colors.mutedForeground
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: root.hasFiles ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                if (root.hasFiles) {
                                    console.log("发送处理任务")
                                }
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }
            }
        }
    }
    
    // ========== 文件选择对话框 ==========
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
            if (sessionController) {
                sessionController.ensureActiveSession()
            }
            fileController.addFiles(fileDialog.selectedFiles)
        }
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
