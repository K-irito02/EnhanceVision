---
alwaysApply: false
globs: ['**/*.qml', '**/*.cpp', '**/*.h']
description: '性能优化规则 - 涉及 QML 渲染、内存管理、线程优化时参考'
trigger: glob
---
# 性能优化规则

## QML 渲染优化

### 属性绑定优化

```qml
// ✅ 正确：简单绑定
Text {
    text: model.fileName
    visible: model.status === "ready"
}

// ❌ 避免：复杂绑定表达式
Text {
    text: {
        if (model.status === "ready") {
            return model.fileName + " - " + model.fileSize
        } else if (model.status === "processing") {
            return qsTr("Processing...")
        } else {
            return qsTr("Unknown")
        }
    }
}

// ✅ 正确：使用函数或组件分离
Text {
    text: root._getDisplayText()
}

function _getDisplayText() {
    // 复杂逻辑
}
```

### 列表虚拟化

```qml
// ✅ 正确：ListView 自动虚拟化
ListView {
    model: largeModel
    delegate: ItemDelegate {
        // 只渲染可见项
    }
    cacheBuffer: 100  // 预缓存像素
}

// ❌ 避免：Repeater 渲染所有项
Repeater {
    model: largeModel  // 会创建所有 delegate
    delegate: ItemDelegate { }
}
```

### Loader 延迟加载

```qml
// ✅ 正确：按需加载重型组件
Loader {
    id: mediaViewerLoader
    active: false  // 默认不加载
    
    sourceComponent: MediaViewer {
        // 重型组件
    }
}

// 需要时激活
Button {
    onClicked: mediaViewerLoader.active = true
}
```

### 图像优化

```qml
// ✅ 正确：使用 Image Provider
Image {
    source: "image://preview/current"
    cache: false  // 需要实时更新时禁用缓存
    asynchronous: true  // 异步加载
    fillMode: Image.PreserveAspectFit
}

// ✅ 正确：指定 sourceSize 避免加载大图
Image {
    source: "image://thumbnail/" + model.filePath
    sourceSize.width: 160
    sourceSize.height: 120
}

// ❌ 避免：加载原始大图
Image {
    source: "file:///" + model.filePath  // 可能是 4K/8K 图像
}
```

## 动画优化

### GPU 加速属性

```qml
// ✅ 正确：只动画 transform 和 opacity
Item {
    transform: Scale { xScale: pressed ? 0.95 : 1.0 }
    opacity: enabled ? 1.0 : 0.5
    
    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }
}

// ❌ 避免：动画 width/height/x/y（触发 layout）
Rectangle {
    width: expanded ? 300 : 60
    
    Behavior on width {
        NumberAnimation { duration: 300 }  // 触发重新布局
    }
}
```

### 动画性能

```qml
// ✅ 正确：使用 SmoothedAnimation
Behavior on x {
    SmoothedAnimation { velocity: 200 }
}

// ✅ 正确：减少同时运行的动画数量
// 避免同屏超过 5 个持续运行的动画

// ❌ 避免：高频率动画
SequentialAnimation {
    running: true
    loops: Animation.Infinite
    NumberAnimation { duration: 50 }  // 太快
}
```

## 内存优化

### 对象生命周期

```qml
// ✅ 正确：使用 Loader 控制生命周期
Loader {
    id: dialogLoader
    active: false
    sourceComponent: Dialog { }
}

function showDialog() {
    dialogLoader.active = true
}

function hideDialog() {
    dialogLoader.active = false  // 释放内存
}

// ❌ 避免：一直存在的不可见对象
Dialog {
    visible: false  // 对象仍在内存中
}
```

### 组件复用

```qml
// ✅ 正确：使用 ListView delegate 复用
ListView {
    model: fileModel
    delegate: FileDelegate {
        // delegate 会被复用
    }
}

// ❌ 避免：动态创建不销毁
Component.onCompleted: {
    for (var i = 0; i < 100; i++) {
        component.createObject(parent)  // 没有销毁
    }
}
```

## 线程优化

### 主线程规则

```cpp
// ✅ 正确：轻量操作在主线程
void SessionController::createSession(const QString& name) {
    QString id = generateId();
    m_sessions.append({id, name});
    emit sessionCreated(id);
}

// ❌ 避免：耗时操作在主线程
void ImageController::processImage(const QString& path) {
    QImage image(path);  // 可能阻塞
    QImage result = m_processor->process(image);
    emit finished(result);
}
```

### 工作线程

```cpp
// ✅ 正确：使用 QtConcurrent
void ImageController::processImage(const QString& path) {
    QtConcurrent::run([this, path]() {
        QImage image(path);
        QImage result = m_processor->process(image);
        emit processingFinished(result);
    });
}

// ✅ 正确：使用 QThreadPool
class AIWorker : public QObject, public QRunnable {
    void run() override {
        // 耗时处理
        emit finished(result);
    }
};

QThreadPool::globalInstance()->start(worker);
```

### 信号槽跨线程

```cpp
// ✅ 正确：自动队列化
connect(worker, &AIWorker::finished, 
        controller, &ImageController::onFinished);
// 跨线程时自动使用 Qt::QueuedConnection

// ❌ 避免：直接连接跨线程
connect(worker, &AIWorker::finished, 
        controller, &ImageController::onFinished,
        Qt::DirectConnection);  // 不安全
```

## 图像处理优化

### 零拷贝传输

```cpp
// ✅ 正确：Image Provider 零拷贝
class PreviewProvider : public QQuickImageProvider {
public:
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        return m_currentImage;  // 直接返回，无需序列化
    }
};
```

### Shader 实时处理

```qml
// ✅ 正确：使用 ShaderEffect GPU 处理
ShaderEffect {
    property var source: previewImage
    property real brightness: brightnessSlider.value
    
    fragmentShader: "qrc:/shaders/brightness.frag.qsb"
}

// ❌ 避免：CPU 逐像素处理
function adjustBrightness(image, value) {
    // CPU 处理慢
}
```

### 缩略图缓存

```cpp
// ✅ 正确：预生成缩略图
class ThumbnailCache {
public:
    QImage getThumbnail(const QString& path, const QSize& size) {
        QString key = QString("%1_%2x%3").arg(path).arg(size.width()).arg(size.height());
        if (m_cache.contains(key)) {
            return m_cache[key];
        }
        
        QImage thumbnail = generateThumbnail(path, size);
        m_cache.insert(key, thumbnail);
        return thumbnail;
    }
    
private:
    QCache<QString, QImage> m_cache;
};
```

## 性能检查清单

### QML 检查

- [ ] 是否使用了复杂绑定表达式？
- [ ] 列表是否使用 ListView 而非 Repeater？
- [ ] 重型组件是否使用 Loader 延迟加载？
- [ ] 图像是否指定了 sourceSize？
- [ ] 动画是否只使用 transform/opacity？
- [ ] 是否有超过 5 个同时运行的动画？

### C++ 检查

- [ ] 耗时操作是否在工作线程执行？
- [ ] 图像传输是否使用 Image Provider？
- [ ] 是否正确使用信号槽跨线程通信？
- [ ] 是否有内存泄漏（检查智能指针）？
- [ ] 大对象是否使用缓存？

### 构建检查

- [ ] 性能测试是否使用 Release 构建？
- [ ] Debug 构建性能比 Release 慢 2-10x，仅用于调试

## 性能基准

| 操作 | 目标性能 |
|------|---------|
| 应用启动 | < 1 秒 |
| 图像加载（4K） | < 100ms |
| Shader 滤镜预览 | 60 FPS |
| AI 推理（1080p x2） | < 2 秒 |
| 列表滚动 | 60 FPS |
| 内存占用（空闲） | < 150MB |
