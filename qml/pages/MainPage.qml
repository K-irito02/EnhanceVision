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
 * 主应用页面，三层布局（从上到下）：
 * (3) 已发送消息展示区 - 占满剩余空间
 * (2) 上传多媒体文件预览区 - 水平缩略图条
 * (1) 底部功能按钮区 - 模式选择、上传、发送
 * 参考功能设计文档 0.2 和 UI 设计文档 08-ui-design.md
 */
Rectangle {
    id: root
    color: Theme.colors.background
    
    // ========== 属性定义 ==========
    property int processingMode: 0  // 0: Shader, 1: AI
    property bool hasFiles: typeof fileModel !== "undefined" ? fileModel.count > 0 : pendingFilesModel.count > 0
    property bool hasMessages: typeof messageModel !== "undefined" ? messageModel.count > 0 : true
    
    // ========== 信号 ==========
    signal expandControlPanel()
    
    // ========== 拖放区域（全页面） ==========
    DropArea {
        id: pageDropArea
        anchors.fill: parent
        
        onDropped: function(drop) {
            if (drop.hasUrls) {
                if (typeof fileController !== "undefined")
                    fileController.addFiles(drop.urls)
                else
                    _addDemoFiles(drop.urls)
            }
        }
        
        // 拖拽高亮覆盖层
        Rectangle {
            anchors.fill: parent
            color: Theme.colors.primarySubtle
            opacity: pageDropArea.containsDrag ? 0.3 : 0
            z: 500
            visible: opacity > 0
            
            Behavior on opacity { NumberAnimation { duration: 150 } }
            
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: pageDropArea.containsDrag
                
                ColoredIcon {
                    Layout.alignment: Qt.AlignHCenter
                    source: Theme.icon("upload")
                    iconSize: 48
                    color: Theme.colors.primary
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("释放以添加文件")
                    color: Theme.colors.primary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }
        }
    }
    
    // ========== 主布局 ==========
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // ========== (3) 消息展示区 ==========
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
                    text: qsTr("Ctrl+O 添加文件，Ctrl+N 新会话，支持拖放文件")
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
        
        // ========== (2) 底部待处理文件预览区 ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.hasFiles ? 100 : 0
            visible: root.hasFiles
            color: Theme.colors.card
            
            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: Theme.animation.normal; easing.type: Easing.OutCubic }
            }
            
            // 顶部分隔线
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.colors.border
            }
            
            // 使用 MediaThumbnailStrip 展示待处理文件
            MediaThumbnailStrip {
                id: pendingStrip
                anchors.fill: parent
                anchors.margins: 10
                mediaModel: typeof fileModel !== "undefined" ? fileModel : pendingFilesModel
                thumbSize: 80
                thumbSpacing: 8
                expandable: false
                onlyCompleted: false
                messageMode: false
                
                onViewFile: function(index) {
                    _openPendingFileViewer(index)
                }
                onSaveFile: function(index) {
                    console.log("保存待处理文件:", index)
                }
                onDeleteFile: function(index) {
                    if (typeof fileController !== "undefined")
                        fileController.removeFileAt(index)
                    else
                        pendingFilesModel.remove(index)
                }
            }
        }
        
        // ========== (1) 底部操作栏 ==========
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
                                    _sendForProcessing()
                                }
                            }
                        }
                        
                        Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
                    }
                }
            }
        }
    }
    
    // ========== 待处理文件查看器 ==========
    MediaViewerWindow {
        id: pendingViewerWindow
        messageMode: false
    }
    
    // ========== 临时待处理文件模型（无 C++ fileModel 时使用） ==========
    ListModel {
        id: pendingFilesModel
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
            if (typeof fileController !== "undefined") {
                fileController.addFiles(fileDialog.selectedFiles)
            } else {
                _addDemoFiles(fileDialog.selectedFiles)
            }
        }
    }
    
    // ========== 快捷键 ==========
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: fileDialog.open()
    }
    
    // ========== 内部方法 ==========
    function _sendForProcessing() {
        console.log("发送处理任务，模式:", root.processingMode === 0 ? "Shader" : "AI")
        if (typeof sessionController !== "undefined") {
            sessionController.ensureActiveSession()
        }
        if (typeof processingController !== "undefined") {
            processingController.sendToProcessing(root.processingMode, {})
        }
    }
    
    function _openPendingFileViewer(index) {
        var model = typeof fileModel !== "undefined" ? fileModel : pendingFilesModel
        var files = []
        for (var i = 0; i < model.count; i++) {
            var item = model.get ? model.get(i) : null
            if (item) {
                files.push({
                    "filePath": item.filePath || "",
                    "fileName": item.fileName || ("file_" + (i+1)),
                    "mediaType": item.mediaType !== undefined ? item.mediaType : 0,
                    "thumbnail": item.thumbnail || "",
                    "resultPath": ""
                })
            }
        }
        pendingViewerWindow.mediaFiles = files
        pendingViewerWindow.openAt(index)
    }
    
    function _addDemoFiles(urls) {
        console.log("[MainPage] Adding demo files, count:", urls.length)
        for (var i = 0; i < urls.length; i++) {
            var url = urls[i]
            var path = url.toString()
            console.log("[MainPage] Adding file:", i, "raw path:", path)
            
            // 转换 URL 为本地文件路径
            var localPath = url.toLocalFile ? url.toLocalFile() : path
            var name = localPath.split(/[/\\]/).pop()
            var isVideo = /\.(mp4|avi|mkv|mov|flv)$/i.test(name)
            
            console.log("[MainPage] File details - localPath:", localPath, "name:", name, "isVideo:", isVideo)
            
            pendingFilesModel.append({
                "filePath": localPath,
                "fileName": name,
                "mediaType": isVideo ? 1 : 0,
                "thumbnail": "",  // 留空让 MediaThumbnailStrip 通过 provider 加载
                "status": 0,
                "resultPath": ""
            })
        }
    }
    
    Behavior on color { ColorAnimation { duration: Theme.animation.normal } }
}
