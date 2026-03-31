# 修复"自动下载"功能不生效问题 - 方案二

## 问题描述

用户在设置中开启"自动保存结果"后，处理好的多媒体文件并没有自动保存到设置的默认导出路径下。

## 问题根因分析

经过代码审查，发现以下问题：

### 1. 设置项已定义但未被使用

- `SettingsController::autoSaveResult()` - 控制是否自动保存结果（已定义）
- `SettingsController::defaultSavePath()` - 默认导出路径（已定义）

### 2. 任务完成时缺少自动保存逻辑

在 `ProcessingController::completeTask` 方法中，任务完成后没有检查 `autoSaveResult` 设置，也没有自动将结果文件复制到 `defaultSavePath`。

## 修复方案：创建独立的 AutoSaveService

### 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    ProcessingController                      │
│                                                              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐ │
│  │ 任务处理  │ → │ 任务完成  │ → │ 发送信号  │ → │ 继续队列  │ │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘ │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ taskCompleted(taskId, resultPath)
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      AutoSaveService                         │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  1. 检查 autoSaveResult 设置                          │  │
│  │  2. 获取 defaultSavePath                              │  │
│  │  3. 异步复制文件到目标目录                             │  │
│  │  4. 处理文件名冲突                                     │  │
│  │  5. 发送保存结果信号                                   │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ fileAutoSaved(path, success)
                            ▼
                   ┌─────────────────┐
                   │  日志 / 通知     │
                   └─────────────────┘
```

### 优点

1. **职责分离**：自动保存逻辑独立于处理逻辑，便于维护
2. **异步处理**：文件复制在后台线程执行，不阻塞主线程
3. **可扩展**：未来可以添加更多功能（如进度通知、批量保存等）
4. **可测试**：独立的服务更容易进行单元测试

## 实施步骤

### 步骤 1：创建 AutoSaveService 头文件

**文件路径**：`include/EnhanceVision/services/AutoSaveService.h`

```cpp
#ifndef ENHANCEVISION_AUTOSAVESERVICE_H
#define ENHANCEVISION_AUTOSAVESERVICE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QMutex>

namespace EnhanceVision {

class AutoSaveService : public QObject
{
    Q_OBJECT

public:
    static AutoSaveService* instance();
    
    void initialize();

    Q_INVOKABLE void autoSaveResult(const QString& taskId, const QString& resultPath);
    
    Q_INVOKABLE bool isAutoSaveEnabled() const;
    Q_INVOKABLE QString getDefaultSavePath() const;

signals:
    void autoSaveCompleted(const QString& taskId, bool success, const QString& savedPath, const QString& error);
    void autoSaveProgress(const QString& taskId, int progress);

private:
    explicit AutoSaveService(QObject* parent = nullptr);
    ~AutoSaveService() override = default;
    
    QString generateUniquePath(const QString& targetDir, const QString& originalPath);
    bool ensureDirectoryExists(const QString& path);

    static AutoSaveService* s_instance;
    QMutex m_mutex;
    QHash<QString, bool> m_pendingSaves;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AUTOSAVESERVICE_H
```

### 步骤 2：创建 AutoSaveService 实现文件

**文件路径**：`src/services/AutoSaveService.cpp`

```cpp
#include "EnhanceVision/services/AutoSaveService.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QtConcurrent>
#include <QDebug>

namespace EnhanceVision {

AutoSaveService* AutoSaveService::s_instance = nullptr;

AutoSaveService::AutoSaveService(QObject* parent)
    : QObject(parent)
{
}

AutoSaveService* AutoSaveService::instance()
{
    if (!s_instance) {
        s_instance = new AutoSaveService();
    }
    return s_instance;
}

void AutoSaveService::initialize()
{
    // 初始化时可以预创建目录等
}

bool AutoSaveService::isAutoSaveEnabled() const
{
    return SettingsController::instance()->autoSaveResult();
}

QString AutoSaveService::getDefaultSavePath() const
{
    return SettingsController::instance()->defaultSavePath();
}

void AutoSaveService::autoSaveResult(const QString& taskId, const QString& resultPath)
{
    if (!isAutoSaveEnabled()) {
        return;
    }
    
    QString savePath = getDefaultSavePath();
    if (savePath.isEmpty() || resultPath.isEmpty()) {
        return;
    }
    
    QFileInfo srcInfo(resultPath);
    if (!srcInfo.exists()) {
        emit autoSaveCompleted(taskId, false, QString(), tr("源文件不存在"));
        return;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingSaves.contains(taskId)) {
            return;
        }
        m_pendingSaves[taskId] = true;
    }
    
    QString destPath = generateUniquePath(savePath, resultPath);
    
    QtConcurrent::run([this, taskId, resultPath, destPath, savePath]() {
        bool success = false;
        QString error;
        
        if (!ensureDirectoryExists(savePath)) {
            error = tr("无法创建目标目录");
        } else {
            if (QFile::copy(resultPath, destPath)) {
                success = true;
                qInfo() << "[AutoSaveService] Auto-saved:" << resultPath << "to:" << destPath;
            } else {
                error = tr("文件复制失败");
                qWarning() << "[AutoSaveService] Failed to copy:" << resultPath << "to:" << destPath;
            }
        }
        
        {
            QMutexLocker locker(&m_mutex);
            m_pendingSaves.remove(taskId);
        }
        
        emit autoSaveCompleted(taskId, success, success ? destPath : QString(), error);
    });
}

QString AutoSaveService::generateUniquePath(const QString& targetDir, const QString& originalPath)
{
    QFileInfo srcInfo(originalPath);
    QString baseName = srcInfo.completeBaseName();
    QString suffix = srcInfo.suffix();
    
    QString destPath = targetDir + "/" + srcInfo.fileName();
    
    int counter = 1;
    while (QFile::exists(destPath)) {
        destPath = QString("%1/%2_%3.%4")
            .arg(targetDir)
            .arg(baseName)
            .arg(counter++)
            .arg(suffix);
    }
    
    return destPath;
}

bool AutoSaveService::ensureDirectoryExists(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

} // namespace EnhanceVision
```

### 步骤 3：在 Application 中初始化 AutoSaveService

**文件路径**：`src/app/Application.cpp`

在 Application 初始化时创建 AutoSaveService 实例，并连接信号。

### 步骤 4：修改 ProcessingController 连接 AutoSaveService

**文件路径**：`src/controllers/ProcessingController.cpp`

在 `completeTask` 方法中调用 AutoSaveService：

```cpp
void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{
    // ... 现有代码 ...
    
    // 任务完成后，触发自动保存
    if (!resultPath.isEmpty()) {
        AutoSaveService::instance()->autoSaveResult(taskId, resultPath);
    }
    
    // ... 继续现有逻辑 ...
}
```

### 步骤 5：更新 CMakeLists.txt

添加新文件到构建系统：

```cmake
set(SOURCES
    # ... 现有源文件 ...
    src/services/AutoSaveService.cpp
)

set(HEADERS
    # ... 现有头文件 ...
    include/EnhanceVision/services/AutoSaveService.h
)
```

### 步骤 6：注册到 QML（可选）

如果需要在 QML 中访问 AutoSaveService 的状态：

```cpp
qmlRegisterSingletonType<EnhanceVision::AutoSaveService>(
    "EnhanceVision.Services", 1, 0, "AutoSaveService",
    [](QQmlEngine*, QJSEngine*) -> QObject* {
        return EnhanceVision::AutoSaveService::instance();
    }
);
```

## 涉及文件

| 文件 | 操作类型 |
|------|----------|
| `include/EnhanceVision/services/AutoSaveService.h` | 新建 |
| `src/services/AutoSaveService.cpp` | 新建 |
| `src/controllers/ProcessingController.cpp` | 修改 |
| `src/app/Application.cpp` | 修改 |
| `CMakeLists.txt` | 修改 |

## 测试验证

1. 开启"自动保存结果"设置
2. 设置一个有效的默认导出路径
3. 处理一个媒体文件
4. 验证结果文件是否自动保存到指定路径
5. 测试文件名冲突处理
6. 测试关闭"自动保存结果"后不会自动保存
7. 测试多个文件同时完成时的并发保存

## 风险评估

- **低风险**：新增独立服务，不影响现有功能
- **异步处理**：文件复制在后台线程，不阻塞主线程
- **可回退**：如果出现问题，可以轻松禁用或移除该服务

## 验收标准

1. ✅ 开启"自动保存结果"后，处理完成的文件自动保存到默认导出路径
2. ✅ 关闭"自动保存结果"后，行为与之前一致（不自动保存）
3. ✅ 文件名冲突时自动添加序号
4. ✅ 目标目录不存在时自动创建
5. ✅ 保存失败时记录日志，不影响正常流程
6. ✅ 异步保存不阻塞主线程
