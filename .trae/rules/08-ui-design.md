---
alwaysApply: false
globs: ['**/qml/styles/**', '**/Theme.qml', '**/Colors.qml']
description: 'UI设计规范 - 涉及主题、颜色、字体、间距、组件样式时参考'
trigger: glob
---
# UI 设计规范

## 主题系统

### 主题切换

```qml
// styles/Theme.qml
pragma Singleton
import QtQuick

QtObject {
    property bool darkMode: SettingsController.theme === "dark"
    
    readonly property var colors: darkMode ? darkColors : lightColors
    
    readonly property var lightColors: QtObject {
        readonly property color background: "#FFFFFF"
        readonly property color surface: "#F5F5F5"
        readonly property color primary: "#1976D2"
        readonly property color primaryLight: "#E3F2FD"
        readonly property color secondary: "#757575"
        readonly property color textPrimary: "#212121"
        readonly property color textSecondary: "#757575"
        readonly property color divider: "#E0E0E0"
        readonly property color error: "#D32F2F"
        readonly property color success: "#388E3C"
        readonly property color warning: "#F57C00"
        readonly property color info: "#1976D2"
    }
    
    readonly property var darkColors: QtObject {
        readonly property color background: "#121212"
        readonly property color surface: "#1E1E1E"
        readonly property color primary: "#90CAF9"
        readonly property color primaryLight: "#1E3A5F"
        readonly property color secondary: "#BDBDBD"
        readonly property color textPrimary: "#FFFFFF"
        readonly property color textSecondary: "#BDBDBD"
        readonly property color divider: "#424242"
        readonly property color error: "#EF5350"
        readonly property color success: "#66BB6A"
        readonly property color warning: "#FFA726"
        readonly property color info: "#64B5F6"
    }
    
    readonly property var animation: QtObject {
        readonly property int durationFast: 100
        readonly property int durationNormal: 200
        readonly property int durationSlow: 300
    }
    
    readonly property var spacing: QtObject {
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 16
        readonly property int lg: 24
        readonly property int xl: 32
    }
    
    readonly property var radius: QtObject {
        readonly property int sm: 4
        readonly property int md: 8
        readonly property int lg: 12
        readonly property int full: 9999
    }
}
```

### 使用主题

```qml
import QtQuick
import "../styles"

Rectangle {
    color: Theme.colors.background
    
    Text {
        color: Theme.colors.textPrimary
    }
    
    Rectangle {
        color: Theme.colors.primary
        radius: Theme.radius.md
    }
}
```

## 颜色系统

### 语义颜色

| 名称 | 用途 | 浅色模式 | 深色模式 |
|------|------|----------|----------|
| background | 页面背景 | #FFFFFF | #121212 |
| surface | 卡片/面板背景 | #F5F5F5 | #1E1E1E |
| primary | 主要操作/强调 | #1976D2 | #90CAF9 |
| primaryLight | 主要操作悬停 | #E3F2FD | #1E3A5F |
| textPrimary | 主要文本 | #212121 | #FFFFFF |
| textSecondary | 次要文本 | #757575 | #BDBDBD |
| divider | 分隔线 | #E0E0E0 | #424242 |
| error | 错误状态 | #D32F2F | #EF5350 |
| success | 成功状态 | #388E3C | #66BB6A |
| warning | 警告状态 | #F57C00 | #FFA726 |

### 状态颜色

```qml
// 按钮状态
Rectangle {
    color: mouseArea.pressed ? Theme.colors.primaryLight :
           mouseArea.containsMouse ? Theme.colors.primaryLight :
           Theme.colors.primary
}
```

## 字体系统

### 字体定义

```qml
// styles/Typography.qml
pragma Singleton
import QtQuick

QtObject {
    readonly property string fontFamily: "Microsoft YaHei, Segoe UI, sans-serif"
    
    readonly property var heading1: QtObject {
        readonly property int pixelSize: 32
        readonly property int weight: Font.Bold
    }
    
    readonly property var heading2: QtObject {
        readonly property int pixelSize: 24
        readonly property int weight: Font.Bold
    }
    
    readonly property var heading3: QtObject {
        readonly property int pixelSize: 20
        readonly property int weight: Font.DemiBold
    }
    
    readonly property var body1: QtObject {
        readonly property int pixelSize: 14
        readonly property int weight: Font.Normal
    }
    
    readonly property var body2: QtObject {
        readonly property int pixelSize: 12
        readonly property int weight: Font.Normal
    }
    
    readonly property var caption: QtObject {
        readonly property int pixelSize: 10
        readonly property int weight: Font.Normal
    }
    
    readonly property var button: QtObject {
        readonly property int pixelSize: 14
        readonly property int weight: Font.Medium
    }
}
```

### 使用字体

```qml
Text {
    text: qsTr("Title")
    font.family: Typography.fontFamily
    font.pixelSize: Typography.heading2.pixelSize
    font.weight: Typography.heading2.weight
    color: Theme.colors.textPrimary
}
```

## 间距系统

### 间距值

| 名称 | 值 | 用途 |
|------|-----|------|
| xs | 4px | 紧凑间距 |
| sm | 8px | 小间距 |
| md | 16px | 标准间距 |
| lg | 24px | 大间距 |
| xl | 32px | 超大间距 |

### 使用间距

```qml
Column {
    spacing: Theme.spacing.md
    
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.spacing.md
    }
}
```

## 圆角系统

### 圆角值

| 名称 | 值 | 用途 |
|------|-----|------|
| sm | 4px | 小按钮、标签 |
| md | 8px | 卡片、输入框 |
| lg | 12px | 大卡片、对话框 |
| full | 9999px | 圆形按钮、徽章 |

## 组件样式

### 按钮

```qml
// controls/Button.qml
import QtQuick
import QtQuick.Controls
import "../styles"

Button {
    id: root
    
    property string variant: "primary"  // primary, secondary, text
    
    implicitWidth: 88
    implicitHeight: 36
    
    background: Rectangle {
        radius: Theme.radius.md
        color: {
            if (root.variant === "primary") {
                return root.pressed ? Theme.colors.primaryLight :
                       root.hovered ? Theme.colors.primaryLight :
                       Theme.colors.primary
            } else if (root.variant === "secondary") {
                return root.pressed ? Theme.colors.surface :
                       root.hovered ? Theme.colors.surface :
                       "transparent"
            } else {
                return "transparent"
            }
        }
        border.width: root.variant === "secondary" ? 1 : 0
        border.color: Theme.colors.divider
    }
    
    contentItem: Text {
        text: root.text
        font: Typography.button
        color: root.variant === "primary" ? "#FFFFFF" : Theme.colors.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
```

### 输入框

```qml
// controls/TextField.qml
import QtQuick
import QtQuick.Controls
import "../styles"

TextField {
    id: root
    
    implicitWidth: 200
    implicitHeight: 40
    
    background: Rectangle {
        radius: Theme.radius.md
        color: Theme.colors.surface
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.colors.primary : Theme.colors.divider
    }
    
    font: Typography.body1
    color: Theme.colors.textPrimary
    placeholderTextColor: Theme.colors.textSecondary
    
    leftPadding: Theme.spacing.md
    rightPadding: Theme.spacing.md
}
```

### 卡片

```qml
// components/Card.qml
import QtQuick
import "../styles"

Rectangle {
    id: root
    
    property alias content: contentItem.children
    
    implicitWidth: 280
    implicitHeight: contentItem.implicitHeight + Theme.spacing.md * 2
    
    color: Theme.colors.surface
    radius: Theme.radius.lg
    
    // 阴影效果
    layer.enabled: true
    layer.effect: DropShadow {
        horizontalOffset: 0
        verticalOffset: 2
        radius: 8
        samples: 16
        color: Theme.darkMode ? "#40000000" : "#20000000"
    }
    
    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: Theme.spacing.md
        implicitHeight: childrenRect.height
    }
}
```

## 图标

### 图标资源

```
resources/icons/
├── file.svg
├── folder.svg
├── settings.svg
├── play.svg
├── pause.svg
├── zoom-in.svg
├── zoom-out.svg
└── ...
```

### 图标使用

```qml
Image {
    source: "qrc:/icons/file.svg"
    sourceSize.width: 24
    sourceSize.height: 24
    
    // 图标颜色（通过 ColorOverlay）
    layer.enabled: true
    layer.effect: ColorOverlay {
        color: Theme.colors.textSecondary
    }
}
```

## 动画规范

### 动画时长

| 名称 | 时长 | 用途 |
|------|------|------|
| durationFast | 100ms | 微交互（悬停、按下） |
| durationNormal | 200ms | 标准过渡 |
| durationSlow | 300ms | 大动作（展开、关闭） |

### 缓动函数

```qml
// 标准缓动
Easing.OutCubic      // 大多数动画
Easing.InOutCubic    // 往返动画
Easing.OutBack       // 弹性效果
```

### 动画示例

```qml
Rectangle {
    id: card
    
    scale: mouseArea.pressed ? 0.98 : 1.0
    opacity: mouseArea.containsMouse ? 1.0 : 0.9
    
    Behavior on scale {
        NumberAnimation { duration: Theme.animation.durationFast; easing.type: Easing.OutCubic }
    }
    
    Behavior on opacity {
        NumberAnimation { duration: Theme.animation.durationNormal }
    }
}
```

## 图像渲染优化

### 缩略图抗锯齿

```qml
// 缩略图组件 - 启用抗锯齿
Image {
    id: thumbImage
    anchors.fill: parent
    source: thumbnailPath
    fillMode: Image.PreserveAspectCrop
    asynchronous: true
    smooth: true                                    // 图像平滑
    sourceSize: Qt.size(displayWidth * 2, displayHeight * 2)  // 高分辨率源
    visible: status === Image.Ready
    layer.enabled: true
    layer.samples: 4                                // 多重采样抗锯齿
    layer.effect: MultiEffect {
        maskEnabled: true
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
        maskSource: thumbMask
    }
}

// 圆角遮罩 - 启用几何抗锯齿
Item {
    id: thumbMask
    visible: false
    layer.enabled: true
    layer.samples: 4
    width: thumbRect.width
    height: thumbRect.height

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color: "white"
        antialiasing: true                          // 几何抗锯齿
    }
}
```

### 大图预览优化

```qml
// 大图预览 - 启用 mipmap
Image {
    source: imagePath
    fillMode: Image.PreserveAspectFit
    asynchronous: true
    smooth: true
    mipmap: true                                   // 多级纹理，提升缩放质量
}
```

### 悬停边框动画

```qml
// 缩略图悬停效果 - 平滑边框过渡
Rectangle {
    id: hoverBorder
    anchors.fill: parent
    anchors.margins: 2
    radius: Theme.radius.md
    color: "transparent"
    border.width: 2
    border.color: mouseArea.containsMouse ? Theme.colors.primary : "transparent"
    z: 5
    
    // 平滑颜色过渡动画
    Behavior on border.color { 
        ColorAnimation { duration: Theme.animation.fast } 
    }
}

// 卡片悬停效果 - 边框和背景同时变化
Rectangle {
    id: card
    color: mouseArea.containsMouse ? Theme.colors.surfaceHover : Theme.colors.surface
    border.width: 1
    border.color: mouseArea.containsMouse ? Theme.colors.primary : Theme.colors.border
    
    Behavior on color { ColorAnimation { duration: Theme.animation.fast } }
    Behavior on border.color { ColorAnimation { duration: Theme.animation.fast } }
}
```

### 抗锯齿技术对比

| 技术 | 属性 | 用途 | 性能影响 |
|------|------|------|---------|
| 图像平滑 | `smooth: true` | 图像缩放平滑 | 极小（GPU 加速） |
| 几何抗锯齿 | `antialiasing: true` | 圆角边缘平滑 | 极小 |
| 多重采样 | `layer.samples: 4` | 遮罩边缘抗锯齿 | 小 |
| 多级纹理 | `mipmap: true` | 大图缩放质量 | 小 |

## 响应式布局

### 断点

```qml
// 断点定义
readonly property var breakpoints: QtObject {
    readonly property int sm: 640
    readonly property int md: 768
    readonly property int lg: 1024
    readonly property int xl: 1280
}

// 响应式判断
property bool isMobile: root.width < breakpoints.md
property bool isTablet: root.width >= breakpoints.md && root.width < breakpoints.lg
property bool isDesktop: root.width >= breakpoints.lg
```

### 响应式布局

```qml
RowLayout {
    spacing: Theme.spacing.md
    
    Sidebar {
        Layout.fillHeight: true
        Layout.preferredWidth: isMobile ? 0 : (isTablet ? 200 : 240)
        visible: !isMobile || sidebarOpen
    }
    
    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        // 主内容
    }
}
```

## 无障碍

### 键盘导航

```qml
Item {
    focus: true
    
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
            root.activate()
        } else if (event.key === Qt.Key_Escape) {
            root.cancel()
        }
    }
}
```

### 焦点指示

```qml
Rectangle {
    border.width: root.activeFocus ? 2 : 0
    border.color: Theme.colors.primary
    
    // 焦点环
    Rectangle {
        anchors.fill: parent
        anchors.margins: -2
        radius: parent.radius + 2
        border.width: root.activeFocus ? 2 : 0
        border.color: Theme.colors.primary
        color: "transparent"
        visible: root.activeFocus
    }
}
```
