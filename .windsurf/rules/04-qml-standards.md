---
alwaysApply: false
globs: ['**/*.qml', '**/*.js']
description: 'QML代码规范 - 编写或审查 QML/JavaScript 代码时参考'
trigger: glob
---
# QML 代码规范

## 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| QML 文件 | PascalCase.qml | `Sidebar.qml`, `MediaViewer.qml` |
| JavaScript 文件 | camelCase.js | `helpers.js`, `formatters.js` |
| 组件 id | camelCase | `id: sidebar`, `id: previewImage` |
| 属性名 | camelCase | `property string fileName` |
| 信号名 | camelCase | `signal fileSelected(string path)` |
| 函数名 | camelCase | `function updatePreview()` |
| 私有属性 | _ 前缀 | `property var _internalState` |

## 文件结构

### 组件文件结构

```qml
// 1. 导入语句
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision 1.0

// 2. 根组件
Item {
    id: root
    
    // 3. 公开属性（可从外部设置）
    property string title: ""
    property alias currentIndex: listView.currentIndex
    
    // 4. 私有属性
    property var _internalData: null
    property bool _isProcessing: false
    
    // 5. 信号声明
    signal clicked()
    signal itemSelected(int index, string path)
    
    // 6. 信号处理器
    Connections {
        target: SessionController
        function onSessionChanged() {
            root._refreshData()
        }
    }
    
    // 7. JavaScript 函数
    function _refreshData() {
        // ...
    }
    
    // 8. 子组件
    ColumnLayout {
        anchors.fill: parent
        
        // ...
    }
}
```

### 页面文件结构

```qml
// pages/MainPage.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision 1.0
import "../components"
import "../controls"

Page {
    id: root
    
    // 页面状态
    property bool isLoading: false
    
    header: ToolBar {
        // 标题栏
    }
    
    background: Rectangle {
        color: Theme.colors.background
    }
    
    ColumnLayout {
        anchors.fill: parent
        
        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: 240
        }
        
        // ...
    }
}
```

## 属性绑定规范

### 基本绑定

```qml
// ✅ 正确：直接绑定
Text {
    text: SessionController.activeSessionName
    color: Theme.colors.textPrimary
}

// ✅ 正确：表达式绑定
Item {
    visible: model.status === "ready" && !root.isLoading
    opacity: enabled ? 1.0 : 0.5
}

// ❌ 避免：不必要的复杂绑定
Text {
    text: {
        if (SessionController.activeSessionName) {
            return SessionController.activeSessionName
        } else {
            return "Untitled"
        }
    }
}

// ✅ 正确：使用 || 提供默认值
Text {
    text: SessionController.activeSessionName || qsTr("Untitled")
}
```

### alias 属性

```qml
Item {
    id: root
    
    // ✅ 正确：alias 用于暴露内部属性
    property alias currentText: textField.text
    property alias currentIndex: listView.currentIndex
    
    TextField {
        id: textField
    }
    
    ListView {
        id: listView
    }
}
```

### 属性绑定断开与重连

```qml
Item {
    id: root
    property int value: 0
    
    function resetValue() {
        // ❌ 错误：这会永久断开绑定
        value = 0
        
        // ✅ 正确：临时修改后恢复绑定
        value = Qt.binding(function() { return someOtherValue })
    }
}
```

### 单向控制模式（父子组件通信）

当父组件需要单向控制子组件，而子组件不应反向影响父组件时：

```qml
// ❌ 错误：双向绑定导致子组件修改破坏绑定
// App.qml
MainPage {
    processingMode: root.processingMode
    onProcessingModeChanged: root.processingMode = processingMode
}
ControlPanel {
    processingMode: root.processingMode
    onProcessingModeChanged: root.processingMode = processingMode
}

// ✅ 正确：单向控制 - 子组件使用独立内部状态
// App.qml
MainPage {
    processingMode: root.processingMode
    onProcessingModeChanged: root.processingMode = processingMode
}
ControlPanel {
    processingMode: root.processingMode  // 只接收，不发信号
}

// ControlPanel.qml
Rectangle {
    id: root
    property int processingMode: 0      // 外部传入
    property int displayMode: 0         // 内部显示状态
    
    // 监听外部更新
    onProcessingModeChanged: {
        displayMode = processingMode
    }
    
    Component.onCompleted: {
        displayMode = processingMode
    }
    
    // UI 使用 displayMode，按钮点击只修改 displayMode
    Button {
        onClicked: displayMode = 0  // 不影响外部 binding
    }
}
```

**关键原则**：
- 父组件通过属性向子组件传递状态
- 子组件使用内部属性管理自己的显示状态
- 子组件通过信号通知父组件（如果需要），但不应直接修改父组件绑定的属性

## 信号处理规范

### 信号声明

```qml
Item {
    id: root
    
    // ✅ 正确：带参数的信号
    signal selected(string path, int index)
    signal errorOccurred(string message, int code)
    
    // ✅ 正确：无参数信号
    signal clicked()
    signal canceled()
}
```

### 信号发射

```qml
Item {
    id: root
    signal selected(string path, int index)
    
    MouseArea {
        onClicked: function(mouse) {
            // ✅ 正确：发射信号
            root.selected(model.filePath, model.index)
        }
    }
    
    Button {
        text: qsTr("Cancel")
        onClicked: root.canceled()  // 无参数信号
    }
}
```

### 信号连接

```qml
Item {
    id: root
    
    // ✅ 正确：Connections 组件
    Connections {
        target: ProcessingController
        function onProgressChanged(progress) {
            progressBar.value = progress
        }
        function onFinished(result) {
            root._showResult(result)
        }
    }
    
    // ✅ 正确：组件内信号处理
    Button {
        onClicked: function() {
            console.log("Button clicked")
        }
    }
}
```

## 组件设计规范

### 可复用组件

```qml
// components/MediaThumbnail.qml
import QtQuick
import QtQuick.Controls

Item {
    id: root
    
    // 公开 API
    property string source: ""
    property string title: ""
    property int status: 0  // 0: pending, 1: processing, 2: done, 3: error
    property bool selected: false
    
    signal clicked()
    signal doubleClicked()
    
    implicitWidth: 160
    implicitHeight: 120
    
    // 内部实现
    Rectangle {
        anchors.fill: parent
        color: root.selected ? Theme.colors.primaryLight : Theme.colors.surface
        radius: 8
        border.width: root.selected ? 2 : 0
        border.color: Theme.colors.primary
        
        // 缩略图
        Image {
            id: thumbnailImage
            anchors.fill: parent
            anchors.margins: 4
            source: root.source ? "image://thumbnail/" + root.source : ""
            fillMode: Image.PreserveAspectCrop
            cache: false
        }
        
        // 状态指示器
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 4
            width: 8
            height: 8
            radius: 4
            color: {
                switch (root.status) {
                    case 0: return Theme.colors.warning
                    case 1: return Theme.colors.info
                    case 2: return Theme.colors.success
                    case 3: return Theme.colors.error
                    default: return "transparent"
                }
            }
        }
        
        // 标题
        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 4
            text: root.title
            elide: Text.ElideRight
            color: Theme.colors.textSecondary
            font.pixelSize: 12
        }
        
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: function(mouse) {
                root.clicked()
            }
            onDoubleClicked: function(mouse) {
                root.doubleClicked()
            }
        }
    }
}
```

### 组件实例化

```qml
// ✅ 正确：使用组件
MediaThumbnail {
    source: model.filePath
    title: model.fileName
    status: model.status
    selected: model.index === root.currentIndex
    onClicked: root.currentIndex = model.index
    onDoubleClicked: root.openViewer(model.index)
}

// ❌ 避免：内联定义大型组件
Item {
    // 不要在这里定义 100+ 行的组件
}
```

## 列表与模型规范

### ListView 使用

```qml
ListView {
    id: listView
    model: FileController.fileModel
    
    // ✅ 正确：使用 delegate 组件
    delegate: FileDelegate {
        width: listView.width
        height: 48
        
        fileName: model.fileName
        filePath: model.filePath
        status: model.status
        
        onClicked: listView.currentIndex = model.index
    }
    
    // ✅ 正确：使用 ScrollBar
    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
    }
    
    // ✅ 正确：虚拟化（默认启用）
    cacheBuffer: 100  // 预缓存像素
}
```

### Model 数据访问

```qml
// ✅ 正确：通过 model.roleName 访问
Text {
    text: model.fileName
}

// ✅ 正确：通过 model.index 访问索引
Item {
    property int itemIndex: model.index
}

// ❌ 避免：直接访问 model 对象
Component.onCompleted: console.log(model)  // 调试用
```

## 动画规范

### 属性动画

```qml
// ✅ 正确：使用 Behavior 自动动画
Item {
    width: expanded ? 300 : 60
    
    Behavior on width {
        NumberAnimation {
            duration: Theme.animation.durationNormal
            easing.type: Easing.OutCubic
        }
    }
}

// ✅ 正确：使用过渡动画
Item {
    states: [
        State {
            name: "expanded"
            PropertyChanges { target: sidebar; width: 300 }
        },
        State {
            name: "collapsed"
            PropertyChanges { target: sidebar; width: 60 }
        }
    ]
    
    transitions: Transition {
        NumberAnimation {
            properties: "width"
            duration: Theme.animation.durationNormal
            easing.type: Easing.OutCubic
        }
    }
}
```

### 动画性能

```qml
// ✅ 正确：只动画 transform 和 opacity（GPU 合成）
Item {
    transform: Scale { xScale: pressed ? 0.95 : 1.0 }
    opacity: enabled ? 1.0 : 0.5
    
    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }
}

// ❌ 避免：动画 width/height（触发 layout）
Rectangle {
    width: expanded ? 300 : 60
    
    Behavior on width {
        NumberAnimation { duration: 300 }  // 可能导致卡顿
    }
}

// ✅ 正确：使用 transform: Scale 替代 width 动画
Rectangle {
    width: 300
    transform: Scale {
        origin.x: 0
        xScale: expanded ? 1.0 : 0.2
    }
}
```

## JavaScript 使用规范

### 内联 JavaScript

```qml
// ✅ 正确：简单逻辑内联
Button {
    text: qsTr("Add")
    onClicked: function() {
        FileController.addFiles()
    }
}

// ✅ 正确：复杂逻辑使用函数
Button {
    text: qsTr("Process")
    onClicked: root._startProcessing()
}

function _startProcessing() {
    if (!FileController.hasFiles()) {
        errorDialog.show(qsTr("No files selected"))
        return
    }
    ProcessingController.startProcessing()
}
```

### 外部 JavaScript

```qml
// utils/Helpers.js
.pragma library

function formatFileSize(bytes) {
    if (bytes < 1024) return bytes + " B"
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
    return (bytes / (1024 * 1024)).toFixed(1) + " MB"
}

function formatDuration(seconds) {
    const mins = Math.floor(seconds / 60)
    const secs = Math.floor(seconds % 60)
    return mins + ":" + secs.toString().padStart(2, "0")
}
```

```qml
// 使用
import "../utils/Helpers.js" as Helpers

Text {
    text: Helpers.formatFileSize(model.fileSize)
}
```

## 国际化规范

### 字符串翻译

```qml
// ✅ 正确：使用 qsTr()
Text {
    text: qsTr("Welcome to EnhanceVision")
}

// ✅ 正确：带参数的翻译
Text {
    text: qsTr("Processing %1 of %2 files").arg(currentIndex + 1).arg(totalCount)
}

// ✅ 正确：复数形式
Text {
    text: qsTr("%n file(s) selected", "", selectedCount)
}

// ❌ 错误：硬编码字符串
Text {
    text: "Welcome"  // 无法翻译
}
```

### 翻译上下文

```qml
// ✅ 正确：使用 qsTranslate 区分上下文
Text {
    text: qsTranslate("FileList", "Open")
}

// 另一个上下文
Text {
    text: qsTranslate("Settings", "Open")
}
```

## 资源引用规范

### QRC 资源

```qml
// ✅ 正确：使用 qrc:// 路径
Image {
    source: "qrc:/icons/file.svg"
}

// ✅ 正确：使用 Theme.icon() 函数（支持主题切换）
ColoredIcon {
    source: Theme.icon("file")
}

// ✅ 正确：相对路径（在 qrc 中）
Image {
    source: "../icons/file.svg"
}

// ❌ 避免：绝对文件系统路径
Image {
    source: "E:/project/icons/file.svg"  // 不可移植
}
```

### 主题相关图标

```qml
// ✅ 正确：通过 Theme.icon() 获取主题适配的图标
IconButton {
    iconName: "external-link"
    // Theme.icon() 会根据 isDark 属性返回正确的图标路径
}

// ✅ 正确：直接使用 ColoredIcon
ColoredIcon {
    source: Theme.icon("play")
    color: Theme.colors.primary
}
```

### 图像提供者

```qml
// ✅ 正确：使用 Image Provider
Image {
    source: "image://preview/current"
    cache: false  // 禁用缓存，实时更新
}

// ✅ 正确：带参数的 Image Provider
Image {
    source: "image://thumbnail/" + model.filePath
}
```

## 关键原则

- **声明式优先**：使用属性绑定而非命令式更新
- **组件化**：将可复用的 UI 封装为独立组件
- **性能意识**：避免在绑定中执行复杂计算
- **国际化**：所有用户可见字符串使用 qsTr()
- **代码分离**：复杂逻辑放在 C++ 或外部 JS 文件
