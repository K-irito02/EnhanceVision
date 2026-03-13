---
alwaysApply: false
globs: ['**/src/**/*.h', '**/src/**/*.hpp', '**/src/**/*.cpp', '**/qml/**/*.qml']
description: '架构设计 - 涉及 QML+C++ 分层架构、数据绑定、信号槽、模型设计时参考'
trigger: glob
---
# 架构设计

## 单进程原生架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    EnhanceVision.exe (单个进程)                  │
│                                                                 │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              Qt Quick (QML) UI 层                          │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │  Pages / Components / Controls / Shaders / Styles   │  │  │
│  │  │  · 声明式 UI 定义                                    │  │  │
│  │  │  · 属性绑定与动画                                    │  │  │
│  │  │  · ShaderEffect 实时滤镜                             │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                           ↕ Q_PROPERTY / 信号槽                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              C++ 业务层                                   │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │  Models (QAbstractListModel)                        │  │  │
│  │  │  · SessionModel · FileModel · ProcessingModel       │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │  Providers (QQuickImageProvider)                    │  │  │
│  │  │  · PreviewProvider · ThumbnailProvider              │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  │  ┌─────────────────────────────────────────────────────┐  │  │
│  │  │  Controllers (QObject + Q_PROPERTY)                 │  │  │
│  │  │  · SessionController · SettingsController           │  │  │
│  │  └─────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                           ↕ 直接调用                             │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │              C++ 核心引擎层                               │  │
│  │  · AIEngine (NCNN + Vulkan)    - AI 推理引擎             │  │
│  │  · ImageProcessor              - 图像处理                 │  │
│  │  · VideoProcessor              - 视频处理                 │  │
│  │  · FrameCache                  - 帧缓存                   │  │
│  │  · ShaderManager               - Shader 管理              │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  所有处理都在本地完成，无需网络连接                              │
│  QML 与 C++ 通过属性绑定和信号槽通信，零序列化开销               │
└─────────────────────────────────────────────────────────────────┘
```

## 分层架构详解

### 第一层：QML UI 层

**职责**：界面渲染、用户交互、动画效果

```
qml/
├── pages/          # 页面（完整的功能视图）
├── components/     # 组件（可复用的 UI 块）
├── controls/       # 控件（基础交互元素）
├── shaders/        # ShaderEffect 组件
└── styles/         # 主题和样式定义
```

**设计原则**：
- QML 只负责 UI 展示和交互，不包含业务逻辑
- 复杂逻辑通过 JavaScript 函数或调用 C++ 方法实现
- 使用属性绑定实现响应式更新

### 第二层：C++ 业务层

**职责**：数据管理、状态同步、业务逻辑

#### Models（数据模型）

```cpp
// 继承 QAbstractListModel，为 QML ListView 提供数据
class FileModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    
public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileNameRole,
        ThumbnailRole,
        StatusRole
    };
    
    Q_INVOKABLE void addFile(const QString& path);
    Q_INVOKABLE void removeFile(int index);
    
signals:
    void countChanged();
};
```

#### Providers（图像提供者）

```cpp
// 为 QML Image 提供图像数据，零拷贝
class PreviewProvider : public QQuickImageProvider {
public:
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        // 直接返回 C++ 层的 QImage，无需序列化
        return m_engine->getCurrentPreview();
    }
};
```

#### Controllers（控制器）

```cpp
// 暴露给 QML 的业务控制器
class SessionController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeSessionId READ activeSessionId NOTIFY activeSessionChanged)
    Q_PROPERTY(QStringList sessions READ sessions NOTIFY sessionsChanged)
    
public:
    Q_INVOKABLE QString createSession(const QString& name = QString());
    Q_INVOKABLE void switchSession(const QString& id);
    Q_INVOKABLE void deleteSession(const QString& id);
    
signals:
    void activeSessionChanged();
    void sessionsChanged();
};
```

### 第三层：C++ 核心引擎层

**职责**：AI 推理、图像处理、视频处理、缓存管理

```cpp
// AI 推理引擎
class AIEngine : public QObject {
    Q_OBJECT
public:
    bool loadModel(const QString& modelPath);
    QImage enhance(const QImage& input, float scale = 2.0f);
    
signals:
    void progressChanged(float progress);
    void finished(const QImage& result);
};

// 图像处理器
class ImageProcessor : public QObject {
    Q_OBJECT
public:
    QImage applyShader(const QImage& input, const ShaderParams& params);
    
private:
    QOpenGLShaderProgram m_shader;
};

// 视频处理器
class VideoProcessor : public QObject {
    Q_OBJECT
public:
    void processVideo(const QString& inputPath, const QString& outputPath,
                      std::function<void(float)> progressCallback);
};
```

## 数据流

### 1. 属性绑定流（自动更新）

```
C++ Q_PROPERTY  ──────→  QML 属性绑定
     ↑                        │
     │                        ↓
     └─────── NOTIFY 信号 ←─── 值变化
```

```qml
// QML 中自动响应 C++ 属性变化
Text {
    text: SessionController.activeSessionId  // 自动绑定
    color: Theme.colors.textPrimary
}
```

### 2. 图像数据流（零拷贝）

```
C++ QImage  ──→  QQuickImageProvider  ──→  QML Image
   (内存)           (直接引用)            (GPU 纹理)
```

```qml
// QML 中直接访问 C++ 图像
Image {
    source: "image://preview/current"  // 通过 Provider 获取
}
```

### 3. 信号槽流（事件通知）

```
C++ 信号  ──────→  QML Connections.onTriggered
   │
   └──→ 处理完成、进度更新、错误通知等
```

```qml
Connections {
    target: ProcessingController
    function onProgressChanged(progress) {
        progressBar.value = progress
    }
    function onFinished(result) {
        previewImage.source = "image://preview/result"
    }
}
```

### 4. 方法调用流（命令执行）

```
QML 事件  ──────→  C++ Q_INVOKABLE 方法
   │
   └──→ 按钮点击、用户操作等
```

```qml
Button {
    text: qsTr("开始处理")
    onClicked: ProcessingController.startProcessing()
}
```

## C++ 与 QML 集成模式

### 模式 1：单例控制器

适用于全局状态管理：

```cpp
// C++
class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    
public:
    static SettingsController* instance();
};

// 注册为 QML 单例
qmlRegisterSingletonType<SettingsController>("EnhanceVision", 1, 0, "SettingsController",
    [](QQmlEngine*, QJSEngine*) -> QObject* {
        return SettingsController::instance();
    });
```

```qml
// QML 使用
import EnhanceVision 1.0

Item {
    property string currentTheme: SettingsController.theme
}
```

### 模式 2：ListModel 数据绑定

适用于列表数据：

```cpp
// C++
class FileModel : public QAbstractListModel {
    // ... 实现 rowCount, data, roleNames
};

// 注册
qmlRegisterType<FileModel>("EnhanceVision", 1, 0, "FileModel");
```

```qml
// QML 使用
ListView {
    model: FileController.fileModel  // C++ 模型实例
    delegate: FileDelegate {
        fileName: model.fileName
        thumbnail: model.thumbnail
    }
}
```

### 模式 3：Image Provider

适用于图像数据：

```cpp
// C++ 注册
engine->addImageProvider("preview", new PreviewProvider());
engine->addImageProvider("thumbnail", new ThumbnailProvider());
```

```qml
// QML 使用
Image {
    source: "image://preview/" + imageId
    cache: false  // 禁用缓存，实时更新
}
```

### 模式 4：信号槽连接

适用于异步通知：

```cpp
// C++
signals:
    void processingStarted();
    void processingProgress(float progress);
    void processingFinished(const QString& resultPath);
    void processingError(const QString& error);
```

```qml
// QML
Connections {
    target: ProcessingController
    function onProcessingProgress(progress) {
        progressBar.value = progress * 100
    }
    function onProcessingFinished(resultPath) {
        resultImage.source = "file:///" + resultPath
    }
}
```

## 线程模型

```
┌─────────────────────────────────────────────────────────────┐
│                      主线程 (GUI)                            │
│  · Qt 事件循环                                               │
│  · QML 场景图渲染                                            │
│  · 用户交互响应                                              │
│  · C++ 控制器方法调用                                        │
└─────────────────────────────────────────────────────────────┘
                           ↕ 信号槽（Qt::QueuedConnection）
┌─────────────────────────────────────────────────────────────┐
│                    工作线程 (QThreadPool)                    │
│  · AI 推理 (NCNN)                                           │
│  · 图像处理                                                  │
│  · 视频帧处理                                                │
│  · 文件 IO                                                   │
└─────────────────────────────────────────────────────────────┘
```

**规则**：
- 主线程只做轻量操作（属性更新、UI 更新）
- 耗时操作必须在工作线程执行
- 线程间通信使用信号槽（自动队列化）
- 禁止在工作线程直接操作 QML 对象

## 关键设计决策

### 为什么选择 QML 而非 WebEngine？

| 对比项 | QML | WebEngine + React |
|--------|-----|-------------------|
| 内存占用 | ~100MB | ~500MB |
| 启动速度 | <1s | 2-5s |
| 图像传输 | 零拷贝 | 需要序列化 |
| GPU 访问 | 直接访问 | 间接访问 |
| Shader 集成 | 原生支持 | 复杂 |
| 开发效率 | 高（声明式） | 高（React 生态） |
| 调试难度 | 中等 | 高（多层抽象） |

### 为什么使用 QQuickImageProvider？

- **零拷贝**：QML Image 直接引用 C++ QImage 内存
- **实时更新**：通过更改 source URL 触发重新加载
- **格式无关**：支持任意图像格式，QML 不需要了解细节
- **线程安全**：Provider 在独立线程执行，不阻塞 UI

### 为什么使用 QAbstractListModel？

- **高效更新**：只更新变化的项，不是整个列表
- **虚拟化支持**：ListView 自动实现虚拟滚动
- **角色绑定**：QML 可以直接绑定 model.roleName
- **排序过滤**：支持 QSortFilterProxyModel
