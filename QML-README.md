# EnhanceVision QML 前端

EnhanceVision 图像视频画质增强工具的 QML 前端部分，基于 Qt Quick 实现。

> **注意**：项目总览、构建指南请参考主 [README.md](README.md)。本文档专注于 QML 开发细节。

## 架构说明

```
┌─────────────────────────────────────────────────────────────┐
│                    QML UI 层                                │
│  Pages / Components / Controls / Shaders / Styles           │
│  · 声明式 UI 定义                                            │
│  · 属性绑定与动画                                            │
│  · ShaderEffect 实时滤镜                                     │
└─────────────────────────────────────────────────────────────┘
                          ↕ Q_PROPERTY / 信号槽
┌─────────────────────────────────────────────────────────────┐
│                    C++ 业务层                               │
│  Controllers / Models / Providers / Services                │
└─────────────────────────────────────────────────────────────┘
```

**关键优势**：
- **零拷贝图像传输**：QQuickImageProvider 直接访问 C++ 图像数据
- **GPU 加速渲染**：Qt Scene Graph 自动使用 GPU 渲染
- **实时 Shader 滤镜**：ShaderEffect 直接在 GPU 上处理图像

## 目录结构

```
qml/
├── main.qml                # QML 入口
├── App.qml                 # 应用根组件
├── pages/                  # 页面（完整的功能视图）
│   ├── MainPage.qml        # 主页面
│   └── SettingsPage.qml    # 设置页面
├── components/             # 组件（可复用的 UI 块）
│   ├── mediaViewer/        # 媒体查看器共享内核
│   │   ├── MediaThumbnailAdapter.qml    # 缩略图适配器
│   │   ├── MediaThumbnailContextMenu.qml # 缩略图上下文菜单
│   │   ├── MediaThumbnailDelegate.qml   # 缩略图委托
│   │   ├── MediaViewerCanvas.qml        # 查看器画布
│   │   ├── MediaViewerControls.qml      # 查看器控制栏
│   │   ├── MediaViewerHelpers.js        # 辅助函数
│   │   ├── MediaViewerShell.qml         # 查看器外壳
│   │   └── MediaViewerThumbnailBar.qml  # 缩略图栏
│   ├── Sidebar.qml         # 侧边栏
│   ├── TitleBar.qml        # 标题栏
│   ├── FileList.qml        # 文件列表
│   ├── PreviewPane.qml     # 预览面板
│   ├── ControlPanel.qml    # 控制面板
│   ├── EmbeddedMediaViewer.qml  # 内嵌媒体查看器
│   ├── MediaViewer.qml     # 媒体查看器
│   ├── MediaViewerWindow.qml    # 独立媒体窗口
│   ├── MinimizedWindowDock.qml  # 最小化停靠栏
│   ├── MediaThumbnailStrip.qml  # 缩略图条
│   ├── SessionList.qml     # 会话列表
│   ├── SessionBatchBar.qml # 会话批量操作栏
│   ├── MessageList.qml     # 消息列表
│   ├── MessageItem.qml     # 消息项
│   ├── Toast.qml           # 提示消息
│   ├── Dialog.qml          # 对话框
│   ├── DialogTitleBar.qml  # 对话框标题栏
│   ├── Card.qml            # 卡片组件
│   ├── FullShaderEffect.qml     # 完整 Shader 效果
│   ├── MultiPassShaderEffect.qml # 多通道 Shader 效果
│   ├── SeparableBlurEffect.qml  # 可分离模糊效果
│   ├── OffscreenShaderRenderer.qml # 离屏渲染器
│   ├── ShaderParamsPanel.qml    # Shader 参数面板
│   ├── AIModelPanel.qml    # AI 模型面板
│   ├── AIParamsPanel.qml   # AI 参数面板
│   ├── ThumbnailStatusImage.qml # 缩略图状态图像
│   ├── VideoControlBar.qml # 视频控制栏
│   └── VideoPlaybackController.qml # 视频播放控制器
├── controls/               # 控件（基础交互元素）
│   ├── IconButton.qml      # 图标按钮
│   ├── Button.qml          # 按钮
│   ├── Slider.qml          # 滑块
│   ├── ComboBox.qml        # 下拉框
│   ├── TextField.qml       # 文本框
│   ├── Switch.qml          # 开关
│   ├── Tooltip.qml         # 工具提示
│   ├── RichTooltip.qml     # 富文本工具提示
│   ├── ColoredIcon.qml     # 彩色图标
│   ├── ParamGroup.qml      # 参数组
│   └── ParamSlider.qml     # 参数滑块
├── shaders/                # ShaderEffect 组件
│   ├── BrightnessContrast.qml
│   ├── SaturationHue.qml
│   ├── SharpenBlur.qml
│   ├── Exposure.qml
│   ├── Gamma.qml
│   ├── Temperature.qml
│   ├── Tint.qml
│   ├── Highlights.qml
│   ├── Shadows.qml
│   ├── Hue.qml
│   └── Vignette.qml
├── stores/                 # 状态存储
│   ├── AIModelConfigStore.qml # AI 模型配置存储
│   └── qmldir              # QML 模块定义
├── styles/                 # 样式定义
│   ├── Theme.qml           # 主题单例
│   ├── Colors.qml          # 颜色定义
│   ├── Typography.qml      # 字体定义
│   └── qmldir              # QML 模块定义
└── utils/                  # QML 工具
    ├── Helpers.js          # JS 辅助函数
    ├── NotificationManager.qml # 通知管理器
    ├── ResponsiveUtils.qml # 响应式工具
    ├── ShortcutManager.qml # 快捷键管理器
    └── qmldir              # QML 模块定义
```

## 资源与窗口约定

- 主题 SVG 图标统一通过 `Theme.icon()` 获取，并由 `ColoredIcon` 完成主题着色和兜底
- 位图、品牌 Logo、固定颜色图片继续使用 `Image`，不要混入主题图标链路
- 重启后需要恢复的窗口几何、最大化状态和面板布局由 C++ 的 UI 状态控制器统一管理，QML 只负责消费结果

## 组件分类

### Pages（页面）

| 页面 | 说明 |
|------|------|
| `MainPage.qml` | 主页面：侧边栏、文件列表、预览面板、控制面板 |
| `SettingsPage.qml` | 设置页面：主题、语言、快捷键配置 |

### Components（组件）

| 组件 | 说明 |
|------|------|
| `Sidebar.qml` | 左侧导航栏，会话列表、添加文件按钮 |
| `TitleBar.qml` | 窗口标题栏，窗口控制按钮 |
| `FileList.qml` | 文件列表，支持多选、拖拽排序 |
| `PreviewPane.qml` | 预览面板，显示当前选中文件 |
| `ControlPanel.qml` | 控制面板，Shader 参数、AI 模型选择 |
| `EmbeddedMediaViewer.qml` | 内嵌媒体查看器，支持全屏、拖拽脱离 |
| `MediaViewer.qml` | 媒体查看器主组件 |
| `MediaViewerWindow.qml` | 独立媒体窗口 |
| `FullShaderEffect.qml` | 完整 Shader 效果，14 种参数 |
| `MultiPassShaderEffect.qml` | 多通道 Shader 效果 |
| `OffscreenShaderRenderer.qml` | 离屏渲染器，用于 GPU 导出 |
| `SessionList.qml` | 会话列表，支持固定、重排序 |
| `SessionBatchBar.qml` | 会话批量操作栏 |
| `MessageList.qml` | 消息列表，显示处理状态 |
| `MessageItem.qml` | 消息项，显示单个处理任务 |
| `Dialog.qml` | 对话框组件 |
| `Toast.qml` | 提示消息组件 |

### mediaViewer 子组件

| 组件 | 说明 |
|------|------|
| `MediaViewerCanvas.qml` | 媒体查看器画布，渲染媒体内容 |
| `MediaViewerControls.qml` | 播放控制栏，播放/暂停、进度条 |
| `MediaViewerThumbnailBar.qml` | 缩略图栏，显示视频帧缩略图 |
| `MediaThumbnailAdapter.qml` | 缩略图适配器 |
| `MediaThumbnailDelegate.qml` | 缩略图委托组件 |
| `MediaThumbnailContextMenu.qml` | 缩略图右键菜单 |
| `MediaViewerShell.qml` | 查看器外壳，包装核心功能 |

### Controls（控件）

| 控件 | 说明 |
|------|------|
| `IconButton.qml` | 图标按钮，支持悬停效果 |
| `Button.qml` | 标准按钮 |
| `Slider.qml` | 滑块，支持实时预览 |
| `ComboBox.qml` | 下拉框 |
| `TextField.qml` | 文本输入框 |
| `Switch.qml` | 开关 |
| `Tooltip.qml` | 工具提示 |
| `RichTooltip.qml` | 富文本工具提示 |
| `ColoredIcon.qml` | 彩色图标，支持主题色 |
| `ParamGroup.qml` | 参数组，分组显示相关参数 |
| `ParamSlider.qml` | 参数滑块，带标签和数值显示 |

## 开发指南

### 使用 ShaderEffect

```qml
import QtQuick

ShaderEffect {
    id: effect
    
    property var source: previewImage
    property real brightness: brightnessSlider.value
    property real contrast: contrastSlider.value
    
    vertexShader: "qrc:/shaders/basic.vert.qsb"
    fragmentShader: "qrc:/shaders/colorAdjust.frag.qsb"
}
```

### 使用 C++ 模型

```qml
ListView {
    model: FileController.fileModel
    
    delegate: FileDelegate {
        fileName: model.fileName
        filePath: model.filePath
        status: model.status
        
        onClicked: FileController.selectFile(model.index)
    }
}
```

### 响应主题变化

```qml
import QtQuick
import "../styles"

Rectangle {
    color: Theme.colors.background
    
    Text {
        color: Theme.colors.textPrimary
        text: qsTr("Welcome")
    }
}
```

## 性能优化

### 图像优化

```qml
// 使用 Image Provider 零拷贝
Image {
    source: "image://preview/current"
    cache: false        // 禁用缓存，实时更新
    asynchronous: true  // 异步加载
}

// 指定缩略图大小
Image {
    source: "image://thumbnail/" + path
    sourceSize.width: 160
    sourceSize.height: 120
}
```

### 列表优化

```qml
// 使用 ListView 虚拟化
ListView {
    model: largeModel
    cacheBuffer: 100  // 缓存额外项
}

// 避免使用 Repeater 渲染大量数据
```

### 动画优化

```qml
// 只动画 transform 和 opacity（GPU 加速）
Item {
    transform: Scale { xScale: pressed ? 0.95 : 1.0 }
    opacity: enabled ? 1.0 : 0.5
    
    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }
}
```

## 国际化

### 使用翻译

```qml
Text {
    text: qsTr("Welcome to EnhanceVision")
}

Text {
    text: qsTr("Processing %1 of %2 files").arg(current).arg(total)
}
```

### 更新翻译

```powershell
cmake --build . --target EnhanceVision_lupdate
cmake --build . --target EnhanceVision_lrelease
```

## 代码规范

详见 `.trae/rules/04-qml-standards.md`

### 基本规则

- 文件名使用 PascalCase
- 组件 id 使用 camelCase
- 使用属性绑定而非命令式更新
- 所有用户可见字符串使用 `qsTr()`
- 复杂逻辑放在 C++ 或外部 JS 文件

### 命名约定

| 类型 | 命名风格 | 示例 |
|------|----------|------|
| 文件名 | PascalCase | `FileList.qml` |
| 组件 id | camelCase | `fileList` |
| 属性 | camelCase | `currentIndex` |
| 信号 | camelCase | `onCurrentIndexChanged` |
| 私有属性 | _camelCase | `_internalState` |

## 常见问题

### QML 模块未找到

```
module "QtQuick" is not installed
```

**解决方案**：

```powershell
$env:QML2_IMPORT_PATH = "E:\Qt\6.10.2\msvc2022_64\qml"
```

### 图像不更新

**解决方案**：

```qml
Image {
    source: "image://preview/current?" + Date.now()  // 添加时间戳强制刷新
    cache: false
}
```

### 组件无法访问父组件属性

**解决方案**：

```qml
Item {
    id: root
    property string myProperty: "value"
    
    Item {
        // 可以直接访问 root.myProperty
        Component.onCompleted: console.log(root.myProperty)
    }
}
```
