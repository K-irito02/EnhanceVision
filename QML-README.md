# EnhanceVision QML 前端

这是 EnhanceVision 图像视频画质增强工具的 QML 前端部分，基于 Qt Quick 实现。

## 架构说明

```
┌─────────────────────────────────────────────────────────────┐
│                    EnhanceVision.exe (单个进程)              │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              Qt Quick (QML) UI 层                      │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │  Pages / Components / Controls / Shaders        │  │  │
│  │  │  · 声明式 UI 定义                                │  │  │
│  │  │  · 属性绑定与动画                                │  │  │
│  │  │  · ShaderEffect 实时滤镜                         │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕ Q_PROPERTY / 信号槽              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              C++ 业务层                               │  │
│  │  · Models (QAbstractListModel)                       │  │
│  │  · Providers (QQuickImageProvider)                   │  │
│  │  · Controllers (QObject + Q_PROPERTY)                │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↕ 直接调用                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              C++ 核心引擎层                           │  │
│  │  · AIEngine (NCNN + Vulkan)                          │  │
│  │  · ImageProcessor / VideoProcessor                   │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

**关键优势**：
- **零拷贝图像传输**：通过 QQuickImageProvider 直接访问 C++ 图像数据
- **GPU 加速渲染**：Qt Scene Graph 自动使用 GPU 渲染
- **实时 Shader 滤镜**：ShaderEffect 直接在 GPU 上处理图像
- **声明式 UI**：QML 类似 React 的声明式语法，开发效率高

## 核心功能

### 图像处理
- **Shader 模式**：实时亮度、对比度、饱和度、色相、模糊、锐化调整
- **AI 推理模式**：Real-ESRGAN 超分辨率增强
- **实时预览**：参数调整即时生效

### 文件管理
- **多媒体文件支持**：图片（JPG/PNG/BMP/WebP）、视频（MP4/MKV/AVI）
- **批量处理**：一次添加多个文件
- **会话管理**：保存处理历史

### 媒体查看器
- **全屏模式**：沉浸式查看
- **原图对比**：一键切换原图/处理效果
- **视频控制**：播放/暂停、进度、音量、倍速

### 用户界面
- **深色/浅色主题**：一键切换
- **响应式布局**：适配不同屏幕
- **键盘快捷键**：提高操作效率
- **国际化**：中文/英文切换

## 技术栈

| 类别 | 技术 |
|------|------|
| **UI 框架** | Qt Quick (QML) |
| **图形渲染** | Qt Scene Graph + RHI |
| **控件库** | Qt Quick Controls 2 |
| **动画** | QML Animations |
| **Shader** | GLSL (qsb 编译) |
| **国际化** | Qt Linguist |

## 目录结构

```
qml/
├── main.qml                # QML 入口
├── App.qml                 # 应用根组件
├── pages/                  # 页面
│   ├── MainPage.qml        # 主页面
│   └── SettingsPage.qml    # 设置页面
├── components/             # 可复用组件
│   ├── Sidebar.qml         # 侧边栏
│   ├── TitleBar.qml        # 标题栏
│   ├── FileList.qml        # 文件列表
│   ├── PreviewPane.qml     # 预览面板
│   ├── ControlPanel.qml    # 控制面板
│   └── MediaViewer.qml     # 媒体查看器
├── controls/               # 自定义控件
│   ├── IconButton.qml      # 图标按钮
│   ├── Slider.qml          # 滑块
│   └── ComboBox.qml        # 下拉框
├── shaders/                # ShaderEffect 组件
│   ├── BrightnessContrast.qml
│   ├── SaturationHue.qml
│   └── SharpenBlur.qml
├── styles/                 # 样式定义
│   ├── Theme.qml           # 主题单例
│   ├── Colors.qml          # 颜色定义
│   └── Typography.qml      # 字体定义
└── utils/                  # QML 工具
    └── Helpers.js          # JS 辅助函数
```

## 开发指南

### 运行应用

```powershell
# Debug 构建
cmake --preset windows-msvc-2022-debug
cmake --build --preset windows-msvc-2022-debug
.\build\msvc2022\Debug\EnhanceVision.exe

# Release 构建
cmake --preset windows-msvc-2022-release
cmake --build --preset windows-msvc-2022-release
.\build\msvc2022\Release\EnhanceVision.exe
```

### QML 调试

```powershell
# 启用 QML 调试
set QML_IMPORT_TRACE=1
set QT_DEBUG_PLUGINS=1
.\build\msvc2022\Debug\EnhanceVision.exe
```

### 热重载（开发模式）

在 `main.cpp` 中启用 QML 热重载：

```cpp
engine->setBaseUrl(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/qml/"));
```

## 组件示例

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
    cache: false
    asynchronous: true
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
    cacheBuffer: 100
    // delegate 会被复用
}

// 避免使用 Repeater 渲染大量数据
```

### 动画优化

```qml
// 只动画 transform 和 opacity
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

- 文件名使用 PascalCase
- 组件 id 使用 camelCase
- 使用属性绑定而非命令式更新
- 所有用户可见字符串使用 qsTr()
- 复杂逻辑放在 C++ 或外部 JS 文件
