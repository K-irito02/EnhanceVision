---
name: "cpp-standards"
alwaysApply: false
globs: ['**/*.cpp', '**/*.h', '**/*.hpp']
description: 'C++ coding standards - Reference when writing or reviewing C++ code'
trigger: glob
---
# C++ 代码规范

## 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 类名 | PascalCase | `ImageProcessor`, `SessionController` |
| 函数名 | camelCase | `processFrame()`, `loadModel()` |
| 变量名 | camelCase | `frameCount`, `currentPath` |
| 成员变量 | m_ 前缀 | `m_processor`, `m_isRunning` |
| 常量 | k 前缀 + PascalCase | `kMaxFrameCount` |
| 枚举 | PascalCase | `enum class ProcessingMode { HighQuality, Fast }` |
| 命名空间 | 小写 | `EnhanceVision::core`, `EnhanceVision::models` |
| Qt 信号 | camelCase + Changed | `progressChanged()`, `sessionCreated()` |
| Qt 槽函数 | on + PascalCase | `onProcessingFinished()` |

## 文件头

```cpp
/**
 * @file FileName.h
 * @brief 简要描述
 *
 * @author EnhanceVision Team
 * @date 2024-01-01
 */
```

## 注释规范

```cpp
/**
 * @brief 处理单帧图像
 * @param frame 输入图像 RGB888
 * @param mode 处理模式
 * @return 处理后图像，失败返回空 QImage
 * @note 阻塞调用，建议在工作线程执行
 */
QImage processFrame(const QImage& frame, ProcessingMode mode);
```

## QML 集成规范

### Q_PROPERTY 规范

```cpp
class SessionController : public QObject {
    Q_OBJECT
    
    // ✅ 正确：只读属性 + NOTIFY 信号
    Q_PROPERTY(QString activeSessionId READ activeSessionId NOTIFY activeSessionChanged)
    
    // ✅ 正确：可写属性 + NOTIFY 信号
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    
    // ✅ 正确：列表属性使用 QVariantList 或 QStringList
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    
    // ❌ 错误：缺少 NOTIFY 信号（QML 无法响应变化）
    Q_PROPERTY(int count READ count)
    
public:
    QString activeSessionId() const;
    QString theme() const;
    void setTheme(const QString& theme);
    QStringList recentFiles() const;
    
signals:
    void activeSessionChanged();
    void themeChanged();
    void recentFilesChanged();
};
```

### Q_INVOKABLE 规范

```cpp
class FileController : public QObject {
    Q_OBJECT
    
public:
    // ✅ 正确：异步操作通过信号返回结果
    Q_INVOKABLE void loadFiles(const QStringList& paths);
    
    // ✅ 正确：同步操作返回简单类型
    Q_INVOKABLE bool hasFiles() const;
    
    // ✅ 正确：返回 QVariant 支持任意类型
    Q_INVOKABLE QVariant getFileInfo(int index);
    
    // ❌ 错误：阻塞操作（应在工作线程执行）
    Q_INVOKABLE QImage processImage(const QImage& input);
    
signals:
    void filesLoaded(int count);
    void loadError(const QString& error);
};
```

### QAbstractListModel 规范

```cpp
class FileModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    
public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,  // 从 UserRole + 1 开始
        FileNameRole,
        FileSizeRole,
        ThumbnailRole,
        StatusRole
    };
    
    explicit FileModel(QObject* parent = nullptr);
    
    // 必须实现
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // 可选实现（支持动态修改）
    Q_INVOKABLE void addFile(const QString& path);
    Q_INVOKABLE void removeFile(int index);
    Q_INVOKABLE void clear();
    
signals:
    void countChanged();
    
private:
    struct FileInfo {
        QString path;
        QString name;
        qint64 size;
        QString thumbnail;
        int status;
    };
    QList<FileInfo> m_files;
};

// roleNames 实现
QHash<int, QByteArray> FileModel::roleNames() const {
    return {
        {FilePathRole, "filePath"},
        {FileNameRole, "fileName"},
        {FileSizeRole, "fileSize"},
        {ThumbnailRole, "thumbnail"},
        {StatusRole, "status"}
    };
}
```

### QQuickImageProvider 规范

```cpp
class PreviewProvider : public QQuickImageProvider {
public:
    PreviewProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)  // 返回 QImage
    {}
    
    // ✅ 正确：返回 QImage，零拷贝
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        Q_UNUSED(size)
        Q_UNUSED(requestedSize)
        
        // 根据 id 获取对应图像
        if (id == "current") {
            return m_currentImage;
        } else if (id.startsWith("result_")) {
            QString resultId = id.mid(7);
            return m_resultImages.value(resultId);
        }
        return QImage();
    }
    
    // ❌ 错误：返回 QPixmap（需要转换，性能差）
    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;
    
    void setCurrentImage(const QImage& image) {
        m_currentImage = image;
    }
    
private:
    QImage m_currentImage;
    QMap<QString, QImage> m_resultImages;
};
```

## 线程安全规范

### 主线程规则

```cpp
// ✅ 正确：轻量操作在主线程
void SessionController::createSession(const QString& name) {
    QString id = generateId();
    m_sessions.append({id, name});
    emit sessionCreated(id);
}

// ❌ 错误：耗时操作在主线程
void ImageController::processImage(const QString& path) {
    QImage image(path);  // 可能阻塞
    QImage result = m_processor->process(image);  // 耗时操作
    emit finished(result);
}
```

### 工作线程规则

```cpp
// ✅ 正确：使用 QtConcurrent 或 QThreadPool
void ImageController::processImage(const QString& path) {
    QtConcurrent::run([this, path]() {
        QImage image(path);
        QImage result = m_processor->process(image);
        
        // 通过信号返回主线程
        emit processingFinished(result);
    });
}

// ✅ 正确：使用信号槽跨线程通信
class AIWorker : public QObject {
    Q_OBJECT
public slots:
    void process(const QImage& input) {
        QImage result = m_engine->enhance(input);
        emit finished(result);
    }
signals:
    void finished(const QImage& result);
};
```

### 信号槽跨线程

```cpp
// ✅ 正确：自动队列化（默认行为）
connect(worker, &AIWorker::finished, 
        controller, &ImageController::onProcessingFinished);

// ✅ 正确：显式指定队列连接
connect(worker, &AIWorker::finished, 
        controller, &ImageController::onProcessingFinished,
        Qt::QueuedConnection);

// ❌ 错误：直接连接（跨线程不安全）
connect(worker, &AIWorker::finished, 
        controller, &ImageController::onProcessingFinished,
        Qt::DirectConnection);
```

## 内存管理规范

### 智能指针

```cpp
// ✅ 正确：使用 std::unique_ptr
std::unique_ptr<AIEngine> m_engine = std::make_unique<AIEngine>();

// ✅ 正确：使用 QScopedPointer
QScopedPointer<QFile> m_file(new QFile(path));

// ✅ 正确：使用 QSharedPointer 共享所有权
QSharedPointer<ImageData> m_cachedImage;

// ❌ 错误：裸指针 + 手动 delete
AIEngine* m_engine = new AIEngine();
// ... 容易忘记 delete
```

### QObject 父子关系

```cpp
// ✅ 正确：设置父对象，自动管理生命周期
auto* model = new FileModel(this);  // this 作为父对象

// ✅ 正确：QML 创建的对象由 QML 引擎管理
// 无需手动管理

// ❌ 错误：无父对象的 QObject 可能泄漏
auto* worker = new AIWorker();  // 没有父对象
```

## 现代 C++ 特性

### 结构化绑定

```cpp
// ✅ 正确
auto [width, height] = getImageSize();
for (const auto& [id, session] : m_sessions) {
    processSession(id, session);
}
```

### 范围 for

```cpp
// ✅ 正确
for (const auto& file : m_files) {
    processFile(file);
}

// ❌ 避免：索引遍历（除非需要索引）
for (int i = 0; i < m_files.size(); ++i) {
    processFile(m_files[i]);
}
```

### auto 关键字

```cpp
// ✅ 正确：类型明显时使用 auto
auto result = m_engine->enhance(image);
auto it = m_sessions.find(id);

// ✅ 正确：复杂类型使用 auto
auto callback = [this](const QImage& img) { emit finished(img); };

// ❌ 避免：基本类型使用 auto（降低可读性）
auto count = m_files.size();  // 用 int count 或 qsizetype count
```

## 错误处理规范

### 返回值

```cpp
// ✅ 正确：使用 std::optional 表示可能失败
std::optional<QImage> loadImage(const QString& path) {
    QImage image(path);
    if (image.isNull()) {
        return std::nullopt;
    }
    return image;
}

// ✅ 正确：使用 QResult（Qt 6.10+）
QResult<QImage> loadImage(const QString& path);
```

### 异常

```cpp
// ✅ 正确：构造函数抛出异常
class AIEngine {
public:
    AIEngine(const QString& modelPath) {
        if (!loadModel(modelPath)) {
            throw std::runtime_error("Failed to load model");
        }
    }
};

// ❌ 避免：Qt 信号槽中抛出异常
signals:
    void errorOccurred(const QString& error);  // 使用信号代替
```

## 关键原则

- **现代 C++20**：使用智能指针、RAII、结构化绑定、concepts
- **线程安全**：UI 线程不做耗时操作，AI/媒体处理在工作线程
- **Qt 集成**：正确使用 Q_PROPERTY、Q_INVOKABLE、信号槽
- **内存安全**：优先使用智能指针，避免裸指针
- **错误处理**：使用 std::optional 或信号通知错误
