# 进度条功能整合与重构计划

## 一、现状分析

### 1.1 当前架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           当前进度条实现架构                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────────────┐     ┌──────────────────┐     ┌────────────────┐  │
│  │   AI 模式        │     │   Shader 模式    │     │   UI 层        │  │
│  ├──────────────────┤     ├──────────────────┤     ├────────────────┤  │
│  │ AIEngine         │     │ ImageProcessor   │     │ MessageItem    │  │
│  │ ├─ProgressReporter│     │ ├─progressChanged│     │ ├─ProgressBar  │  │
│  │ │ ├─progress(0-1) │     │ │ ├─progress(0-100)│    │ │ ├─动画       │  │
│  │ │ ├─stage        │     │ │ └─status string │     │ │ └─预估时间   │  │
│  │ │ └─estimatedSec │     │                  │     │                │  │
│  │ └─processAsync   │     │ VideoProcessor   │     │ ProcessingModel│  │
│  │                  │     │ ├─progressChanged│     │ └─任务列表     │  │
│  │ AIVideoProcessor │     │ │ └─progress(0-100)│    │                │  │
│  │ └─progressChanged│     │ └─processAsync   │     └────────────────┘  │
│  └──────────────────┘     └──────────────────┘                         │
│           │                        │                                    │
│           └──────────┬─────────────┘                                    │
│                      ▼                                                  │
│           ┌──────────────────────┐                                      │
│           │ ProcessingController │                                      │
│           │ ├─updateTaskProgress │                                      │
│           │ ├─syncMessageProgress│                                      │
│           │ └─防抖/节流逻辑      │                                      │
│           └──────────────────────┘                                      │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 1.2 现有组件分析

| 组件 | 文件位置 | 进度范围 | 更新机制 | 问题 |
|------|----------|----------|----------|------|
| `ProgressReporter` | `core/ProgressReporter.h/cpp` | 0.0-1.0 | 信号+节流 | 仅 AI 模式使用 |
| `AIEngine` | `core/AIEngine.h/cpp` | 0.0-1.0 | ProgressReporter | 进度点较少 |
| `ImageProcessor` | `core/ImageProcessor.h/cpp` | 0-100 | 信号直接发射 | 仅 4 个固定进度点 |
| `VideoProcessor` | `core/VideoProcessor.h/cpp` | 0-100 | 回调+信号 | 进度映射粗糙 |
| `ProcessingController` | `controllers/ProcessingController.h/cpp` | 0-100 | 聚合+防抖 | 逻辑复杂分散 |
| `MessageItem` | `qml/components/MessageItem.qml` | 0-100 | 绑定+动画 | 重复计算预估时间 |

### 1.3 核心问题识别

1. **接口不统一**
   - AI 模式使用 `double` (0.0-1.0)
   - Shader 模式使用 `int` (0-100)
   - 数据类型转换频繁，易出错

2. **进度精度不足**
   - Shader 图像处理仅 4 个进度点（10%, 30%, 80%, 100%）
   - 缺少细粒度进度反馈

3. **代码重复**
   - 预估时间计算在 `ProgressReporter` 和 `MessageItem` 中各有一套
   - 节流逻辑在多处重复实现

4. **扩展性差**
   - 添加新的处理模式需要修改多处代码
   - 缺少抽象的进度提供者接口

5. **线程安全风险**
   - 部分进度更新未正确处理跨线程问题
   - 信号发射方式不一致

---

## 二、设计方案

### 2.1 统一进度管理接口

```cpp
// 新增接口：IProgressProvider
class IProgressProvider {
public:
    virtual ~IProgressProvider() = default;
    
    // 进度值 (0.0-1.0)
    virtual double progress() const = 0;
    
    // 当前阶段描述
    virtual QString stage() const = 0;
    
    // 预估剩余时间（秒），-1 表示未知
    virtual int estimatedRemainingSec() const = 0;
    
    // 进度是否有效
    virtual bool isValid() const = 0;
    
signals:
    virtual void progressChanged(double progress) = 0;
    virtual void stageChanged(const QString& stage) = 0;
    virtual void estimatedRemainingSecChanged(int seconds) = 0;
};
```

### 2.2 增强 ProgressReporter

扩展现有 `ProgressReporter` 实现 `IProgressProvider`：

```cpp
class ProgressReporter : public QObject, public IProgressProvider {
    Q_OBJECT
    Q_INTERFACES(EnhanceVision::IProgressProvider)
    
    // 新增属性
    Q_PROPERTY(int totalSteps READ totalSteps WRITE setTotalSteps)
    Q_PROPERTY(int currentStep READ currentStep WRITE setCurrentStep)
    Q_PROPERTY(QString subStage READ subStage NOTIFY subStageChanged)
    
public:
    // 分级进度支持
    void setProgress(double overallProgress, int stepIndex, int totalSteps);
    void setSubProgress(double subProgress);  // 当前步骤内的子进度
    
    // 进度预测增强
    void setProgressSpeedModel(double itemsPerSecond);
    
    // 批量任务支持
    void beginBatch(int totalItems);
    void updateBatchProgress(int completedItems, int totalItems);
    void endBatch();
};
```

### 2.3 Shader 模式进度增强

#### ImageProcessor 改进

```cpp
class ImageProcessor : public QObject {
    // 新增：细粒度进度报告
    void processImageAsync(const QString& inputPath,
                          const QString& outputPath,
                          const ShaderParams& params,
                          ProgressReporter* reporter = nullptr);  // 使用统一接口
    
private:
    // 处理阶段枚举
    enum class ProcessingStage {
        Loading = 0,      // 0-10%
        Preprocessing,    // 10-20%
        ColorAdjust,      // 20-40%
        Effects,          // 40-70%
        Postprocessing,   // 70-90%
        Saving            // 90-100%
    };
    
    void reportStageProgress(ProcessingStage stage, double stageProgress);
};
```

#### VideoProcessor 改进

```cpp
class VideoProcessor : public QObject {
    // 新增：帧级精确进度
    void setFrameProgressCallback(std::function<void(int64_t, int64_t, double)> callback);
    
    // 进度映射改进
    // 阶段划分：
    // - 初始化：0-5%
    // - 解码准备：5-10%
    // - 帧处理：10-90%（按帧数精确映射）
    // - 编码收尾：90-95%
    // - 写入文件：95-100%
};
```

### 2.4 ProcessingController 重构

```cpp
class ProcessingController : public QObject {
    // 统一的进度管理器
    std::unique_ptr<ProgressManager> m_progressManager;
    
    // 简化的进度更新接口
    void registerProgressProvider(const QString& taskId, IProgressProvider* provider);
    void unregisterProgressProvider(const QString& taskId);
    
    // 聚合进度计算（自动处理）
    struct AggregatedProgress {
        double overallProgress;
        QString currentStage;
        int estimatedRemainingSec;
        int completedItems;
        int totalItems;
    };
    
    AggregatedProgress getAggregatedProgress(const QString& messageId);
};
```

### 2.5 新增 ProgressManager 组件

```cpp
// 新增核心组件：统一进度管理器
class ProgressManager : public QObject {
    Q_OBJECT
    
public:
    explicit ProgressManager(QObject* parent = nullptr);
    
    // 注册/注销进度提供者
    void registerProvider(const QString& id, IProgressProvider* provider);
    void unregisterProvider(const QString& id);
    
    // 聚合查询
    struct ProgressInfo {
        double progress;
        QString stage;
        int estimatedRemainingSec;
        bool isValid;
    };
    
    ProgressInfo getProgress(const QString& id) const;
    ProgressInfo getAggregatedProgress(const QStringList& ids) const;
    
    // 进度预测
    void updateSpeedModel(const QString& id, double speed);
    int predictRemainingTime(const QString& id) const;
    
signals:
    void progressChanged(const QString& id, double progress);
    void stageChanged(const QString& id, const QString& stage);
    void aggregatedProgressChanged(const QString& groupId, double progress);
    
private:
    QHash<QString, QPointer<IProgressProvider>> m_providers;
    QHash<QString, ProgressHistory> m_history;  // 用于预测
    QTimer* m_updateTimer;  // 统一的更新节流
};
```

### 2.6 QML 层简化

```qml
// MessageItem.qml 改进
MessageItem {
    id: root
    
    // 直接绑定到 ProcessingController 提供的聚合进度
    property var progressInfo: processingController ? 
        processingController.getAggregatedProgress(taskId) : null
    
    // 进度条直接使用绑定值
    progress: progressInfo ? progressInfo.progress * 100 : 0
    estimatedRemainingSec: progressInfo ? progressInfo.estimatedRemainingSec : 0
    currentStage: progressInfo ? progressInfo.stage : ""
    
    // 移除本地的预估时间计算逻辑
}
```

---

## 三、实施步骤

### 阶段一：基础架构重构（预计 2-3 天）

#### 任务 1.1：定义统一接口

**文件**：`include/EnhanceVision/core/IProgressProvider.h`

```cpp
#ifndef ENHANCEVISION_IPROGRESSPROVIDER_H
#define ENHANCEVISION_IPROGRESSPROVIDER_H

#include <QObject>
#include <QString>

namespace EnhanceVision {

class IProgressProvider
{
public:
    virtual ~IProgressProvider() = default;
    
    virtual double progress() const = 0;
    virtual QString stage() const = 0;
    virtual int estimatedRemainingSec() const = 0;
    virtual bool isValid() const = 0;
    
    virtual QObject* asQObject() = 0;
};

} // namespace EnhanceVision

Q_DECLARE_INTERFACE(EnhanceVision::IProgressProvider, 
                    "com.enhancevision.IProgressProvider/1.0")

#endif // ENHANCEVISION_IPROGRESSPROVIDER_H
```

#### 任务 1.2：增强 ProgressReporter

**修改文件**：
- `include/EnhanceVision/core/ProgressReporter.h`
- `src/core/ProgressReporter.cpp`

**改动点**：
1. 实现 `IProgressProvider` 接口
2. 添加分级进度支持
3. 增强预估时间算法
4. 添加批量任务支持

#### 任务 1.3：创建 ProgressManager

**新增文件**：
- `include/EnhanceVision/core/ProgressManager.h`
- `src/core/ProgressManager.cpp`

**功能**：
1. 统一管理所有进度提供者
2. 提供聚合进度计算
3. 统一节流和防抖
4. 进度预测模型

### 阶段二：Shader 模式进度增强（预计 1-2 天）

#### 任务 2.1：ImageProcessor 进度细化

**修改文件**：
- `include/EnhanceVision/core/ImageProcessor.h`
- `src/core/ImageProcessor.cpp`

**改动点**：
1. 添加 `ProgressReporter*` 参数
2. 细化处理阶段进度报告
3. 实现基于像素处理量的进度估算

#### 任务 2.2：VideoProcessor 进度优化

**修改文件**：
- `include/EnhanceVision/core/VideoProcessor.h`
- `src/core/VideoProcessor.cpp`

**改动点**：
1. 精确帧级进度映射
2. 阶段划分更清晰
3. 添加处理速度估算

### 阶段三：ProcessingController 重构（预计 2 天）

#### 任务 3.1：集成 ProgressManager

**修改文件**：
- `include/EnhanceVision/controllers/ProcessingController.h`
- `src/controllers/ProcessingController.cpp`

**改动点**：
1. 添加 `ProgressManager` 成员
2. 简化进度同步逻辑
3. 统一进度更新接口

#### 任务 3.2：AI 模式进度集成

**修改文件**：
- `src/controllers/ProcessingController.cpp`

**改动点**：
1. 注册 AIEngine 的 ProgressReporter
2. 统一进度聚合计算

### 阶段四：QML 层优化（预计 1 天）

#### 任务 4.1：MessageItem 简化

**修改文件**：
- `qml/components/MessageItem.qml`

**改动点**：
1. 移除本地预估时间计算
2. 直接绑定 ProcessingController 提供的进度信息
3. 优化进度条动画

#### 任务 4.2：进度条组件抽取

**新增文件**：
- `qml/controls/UnifiedProgressBar.qml`

**功能**：
1. 统一的进度条外观
2. 平滑动画
3. 预估时间显示
4. 阶段描述显示

### 阶段五：测试与验证（预计 1-2 天）

#### 任务 5.1：单元测试

**新增文件**：
- `tests/ProgressReporterTest.cpp`
- `tests/ProgressManagerTest.cpp`

**测试用例**：
1. 进度值边界测试
2. 跨线程信号测试
3. 聚合进度计算测试
4. 预估时间准确性测试

#### 任务 5.2：集成测试

**测试场景**：
1. Shader 图像处理进度验证
2. Shader 视频处理进度验证
3. AI 图像推理进度验证
4. AI 视频推理进度验证
5. 多任务并行进度验证
6. 取消/暂停进度状态验证

#### 任务 5.3：性能测试

**测试项目**：
1. 进度更新频率对 UI 性能的影响
2. 内存泄漏检测
3. 线程安全压力测试

---

## 四、详细设计

### 4.1 ProgressReporter 增强实现

```cpp
// ProgressReporter.h 新增成员
class ProgressReporter : public QObject, public IProgressProvider {
    // ... 现有成员 ...
    
    // === 新增成员 ===
    
    // 分级进度支持
    std::atomic<int> m_totalSteps{1};
    std::atomic<int> m_currentStep{0};
    std::atomic<double> m_subProgress{0.0};
    QString m_subStage;
    
    // 批量任务支持
    std::atomic<int> m_batchTotal{0};
    std::atomic<int> m_batchCompleted{0};
    std::atomic<bool> m_batchMode{false};
    
    // 进度预测模型
    struct SpeedSample {
        qint64 timestampMs;
        double progress;
    };
    std::deque<SpeedSample> m_speedSamples;
    std::atomic<double> m_estimatedSpeed{0.0};  // 进度/秒
    
    // 改进的平滑算法
    double smoothProgressV2(double rawValue);
    
    // 改进的预估时间算法
    void updateEstimatedTimeV2();
    
public:
    // === IProgressProvider 接口实现 ===
    double progress() const override { return m_progress.load(); }
    QString stage() const override;
    int estimatedRemainingSec() const override { return m_estimatedRemainingSec.load(); }
    bool isValid() const override { return m_progress.load() >= 0; }
    QObject* asQObject() override { return this; }
    
    // === 新增公共接口 ===
    
    // 分级进度
    void setProgress(double value, int step, int totalSteps, const QString& stage = QString());
    void setSubProgress(double subProgress, const QString& subStage = QString());
    
    // 批量任务
    void beginBatch(int totalItems, const QString& stage = QString());
    void updateBatch(int completedItems, const QString& subStage = QString());
    void endBatch();
    
    // 进度预测
    void setEstimatedSpeed(double progressPerSecond);
    void recordSpeedSample();
};
```

### 4.2 ProgressManager 实现

```cpp
// ProgressManager.h
#ifndef ENHANCEVISION_PROGRESSMANAGER_H
#define ENHANCEVISION_PROGRESSMANAGER_H

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QMutex>
#include "EnhanceVision/core/IProgressProvider.h"

namespace EnhanceVision {

class ProgressManager : public QObject {
    Q_OBJECT
    
public:
    struct ProgressInfo {
        double progress = 0.0;
        QString stage;
        int estimatedRemainingSec = -1;
        bool isValid = false;
        qint64 lastUpdateMs = 0;
    };
    
    explicit ProgressManager(QObject* parent = nullptr);
    ~ProgressManager() override;
    
    // 单任务管理
    void registerProvider(const QString& id, IProgressProvider* provider);
    void unregisterProvider(const QString& id);
    bool hasProvider(const QString& id) const;
    
    // 进度查询
    ProgressInfo getProgress(const QString& id) const;
    
    // 聚合进度（用于多文件任务）
    ProgressInfo getAggregatedProgress(const QStringList& ids) const;
    
    // 分组管理
    void addToGroup(const QString& groupId, const QString& providerId);
    void removeFromGroup(const QString& groupId, const QString& providerId);
    ProgressInfo getGroupProgress(const QString& groupId) const;
    
    // 全局配置
    void setUpdateInterval(int ms);
    int updateInterval() const;
    
signals:
    void progressUpdated(const QString& id, const ProgressInfo& info);
    void groupProgressUpdated(const QString& groupId, const ProgressInfo& info);
    
private slots:
    void onProviderProgressChanged(double progress);
    void onProviderStageChanged(const QString& stage);
    void onProviderEstimatedTimeChanged(int seconds);
    void onProviderDestroyed(QObject* obj);
    void onThrottleTimer();
    
private:
    void updateProgressInfo(const QString& id);
    void emitThrottled(const QString& id, const ProgressInfo& info);
    
    mutable QMutex m_mutex;
    QHash<QString, QPointer<IProgressProvider>> m_providers;
    QHash<QString, ProgressInfo> m_progressCache;
    QHash<QString, QStringList> m_groups;
    
    QTimer* m_throttleTimer;
    QSet<QString> m_pendingUpdates;
    int m_updateIntervalMs = 100;
    
    QHash<QString, qint64> m_lastEmitTime;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROGRESSMANAGER_H
```

### 4.3 ImageProcessor 进度细化

```cpp
// ImageProcessor.cpp 改进示例

void ImageProcessor::processImageAsync(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
                                       ProgressReporter* reporter)
{
    if (m_isProcessing) {
        emit finished(false, QString(), tr("已有处理任务正在进行"));
        return;
    }
    
    m_isProcessing = true;
    m_cancelled = false;
    m_reporter = reporter;
    
    if (m_reporter) {
        m_reporter->reset();
        m_reporter->beginBatch(5, tr("处理图像"));  // 5 个阶段
    }
    
    QtConcurrent::run([this, inputPath, outputPath, params]() {
        try {
            // 阶段 1：加载图像 (0-20%)
            if (m_reporter) m_reporter->updateBatch(0, tr("加载图像"));
            emit progressChanged(5, tr("正在读取图像..."));
            
            QImage inputImage(inputPath);
            if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
            if (inputImage.isNull()) throw std::runtime_error(tr("无法读取图像文件").toStdString());
            
            if (m_reporter) m_reporter->updateBatch(1, tr("预处理"));
            emit progressChanged(20, tr("图像加载完成"));
            
            // 阶段 2：预处理 (20-30%)
            QImage result = inputImage.convertToFormat(QImage::Format_RGB32);
            int width = result.width();
            int height = result.height();
            int totalPixels = width * height;
            
            if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
            if (m_reporter) m_reporter->updateBatch(2, tr("应用滤镜"));
            
            // 阶段 3：颜色调整 (30-60%) - 细粒度进度
            int pixelStep = totalPixels / 30;  // 每 ~3% 报告一次
            int nextReportPixel = pixelStep;
            int pixelsProcessed = 0;
            
            for (int y = 0; y < height; ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
                
                for (int x = 0; x < width; ++x) {
                    // ... 处理像素 ...
                    
                    pixelsProcessed++;
                    if (pixelsProcessed >= nextReportPixel) {
                        double pixelProgress = 30.0 + 30.0 * pixelsProcessed / totalPixels;
                        emit progressChanged(static_cast<int>(pixelProgress), tr("应用颜色调整..."));
                        if (m_reporter) {
                            m_reporter->setSubProgress(pixelProgress / 100.0);
                        }
                        nextReportPixel += pixelStep;
                        
                        if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
                    }
                }
            }
            
            // 阶段 4：特效处理 (60-85%)
            if (m_reporter) m_reporter->updateBatch(3, tr("应用特效"));
            if (params.blur > 0.001f) {
                emit progressChanged(65, tr("应用模糊效果..."));
                applyBlur(result, params.blur);
            }
            if (params.denoise > 0.001f) {
                emit progressChanged(75, tr("应用降噪效果..."));
                // ... 降噪处理 ...
            }
            if (params.sharpness > 0.001f) {
                emit progressChanged(80, tr("应用锐化效果..."));
                // ... 锐化处理 ...
            }
            
            if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
            
            // 阶段 5：保存 (85-100%)
            if (m_reporter) m_reporter->updateBatch(4, tr("保存结果"));
            emit progressChanged(90, tr("正在保存结果..."));
            
            if (!result.save(outputPath)) {
                throw std::runtime_error(tr("无法保存图像文件").toStdString());
            }
            
            if (m_reporter) m_reporter->endBatch();
            emit progressChanged(100, tr("处理完成"));
            
            m_isProcessing = false;
            emit finished(true, outputPath, QString());
            
        } catch (const std::exception& e) {
            m_isProcessing = false;
            if (m_reporter) m_reporter->reset();
            emit finished(false, QString(), QString::fromStdString(e.what()));
        }
    });
}
```

### 4.4 VideoProcessor 进度优化

```cpp
// VideoProcessor.cpp 进度映射改进

void VideoProcessor::processVideoInternal(...) {
    // 进度映射表
    // 0-5%:   打开输入文件
    // 5-10%:  初始化解码器
    // 10-15%: 初始化编码器
    // 15-20%: 初始化转换上下文
    // 20-90%: 帧处理（精确映射）
    // 90-95%: 刷新编码器
    // 95-100%: 写入文件尾
    
    auto reportProgress = [&](int stageStart, int stageEnd, double stageProgress, const QString& msg) {
        int progress = static_cast<int>(stageStart + (stageEnd - stageStart) * stageProgress);
        if (progressCallback) progressCallback(progress, msg);
        emit progressChanged(progress, msg);
    };
    
    // 阶段 1：打开文件 (0-5%)
    reportProgress(0, 5, 0.0, tr("正在打开文件..."));
    if (avformat_open_input(...) != 0) {
        throw std::runtime_error(tr("无法打开输入视频文件").toStdString());
    }
    reportProgress(0, 5, 1.0, tr("文件打开完成"));
    
    // ... 其他阶段 ...
    
    // 帧处理阶段 (20-90%)
    const int frameProgressStart = 20;
    const int frameProgressRange = 70;
    
    while (av_read_frame(...) >= 0) {
        // ... 处理帧 ...
        
        frameCount++;
        
        // 精确进度映射
        double frameProgress = static_cast<double>(frameCount) / totalFrames;
        int overallProgress = static_cast<int>(frameProgressStart + frameProgressRange * frameProgress);
        
        // 每 10 帧或关键帧报告一次
        if (frameCount % 10 == 0 || frameCount == 1 || frameCount == totalFrames) {
            QString msg = tr("正在处理第 %1/%2 帧...").arg(frameCount).arg(totalFrames);
            if (progressCallback) progressCallback(overallProgress, msg);
            emit progressChanged(overallProgress, msg);
        }
        
        if (m_cancelled) {
            throw std::runtime_error(tr("处理已取消").toStdString());
        }
    }
    
    // ... 后续阶段 ...
}
```

### 4.5 ProcessingController 集成

```cpp
// ProcessingController.h 新增成员
class ProcessingController : public QObject {
    // ... 现有成员 ...
    
    // 新增：统一进度管理器
    std::unique_ptr<ProgressManager> m_progressManager;
    
    // 任务到进度提供者的映射
    QHash<QString, QString> m_taskToProgressId;
    
public:
    // 新增 Q_PROPERTY 供 QML 访问
    Q_INVOKABLE QVariantMap getTaskProgress(const QString& taskId) const;
    Q_INVOKABLE QVariantMap getMessageProgress(const QString& messageId) const;
    
signals:
    // 新增：统一的进度更新信号
    void taskProgressInfoChanged(const QString& taskId, const QVariantMap& info);
    void messageProgressInfoChanged(const QString& messageId, const QVariantMap& info);
};

// ProcessingController.cpp 实现
void ProcessingController::startTask(QueueTask& task) {
    // ... 现有逻辑 ...
    
    // 注册进度提供者
    if (message.mode == ProcessingMode::AIInference) {
        AIEngine* engine = m_aiEnginePool->acquire(taskId);
        if (engine && engine->progressReporter()) {
            m_progressManager->registerProvider(taskId, engine->progressReporter());
            m_taskToProgressId[taskId] = taskId;
        }
    } else if (message.mode == ProcessingMode::Shader) {
        auto processor = QSharedPointer<ImageProcessor>::create();
        // 创建专用的 ProgressReporter
        auto reporter = std::make_unique<ProgressReporter>(this);
        m_progressManager->registerProvider(taskId, reporter.get());
        processor->processImageAsync(inputPath, outputPath, params, reporter.get());
        m_taskToProgressId[taskId] = taskId;
    }
    
    // 连接进度管理器信号
    connect(m_progressManager.get(), &ProgressManager::progressUpdated,
            this, [this](const QString& id, const ProgressManager::ProgressInfo& info) {
        emit taskProgressInfoChanged(id, QVariantMap{
            {"progress", info.progress * 100},
            {"stage", info.stage},
            {"estimatedRemainingSec", info.estimatedRemainingSec},
            {"isValid", info.isValid}
        });
    });
}

QVariantMap ProcessingController::getTaskProgress(const QString& taskId) const {
    auto info = m_progressManager->getProgress(taskId);
    return QVariantMap{
        {"progress", info.progress * 100},
        {"stage", info.stage},
        {"estimatedRemainingSec", info.estimatedRemainingSec},
        {"isValid", info.isValid}
    };
}

QVariantMap ProcessingController::getMessageProgress(const QString& messageId) const {
    // 收集该消息下所有任务的进度 ID
    QStringList taskIds;
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && m_taskToProgressId.contains(task.taskId)) {
            taskIds.append(m_taskToProgressId[task.taskId]);
        }
    }
    
    auto info = m_progressManager->getAggregatedProgress(taskIds);
    return QVariantMap{
        {"progress", info.progress * 100},
        {"stage", info.stage},
        {"estimatedRemainingSec", info.estimatedRemainingSec},
        {"isValid", info.isValid}
    };
}
```

### 4.6 QML 层简化

```qml
// MessageItem.qml 改进
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EnhanceVision.Controllers
import "../styles"
import "../controls"

Rectangle {
    id: root
    
    property string taskId: ""
    property string messageId: ""
    
    // 直接从 ProcessingController 获取进度信息
    property var progressInfo: {
        if (!processingController || !messageId) return null
        return processingController.getMessageProgress(messageId)
    }
    
    // 绑定进度值
    property real progress: progressInfo ? progressInfo.progress : 0
    property string currentStage: progressInfo ? progressInfo.stage : ""
    property int estimatedRemainingSec: progressInfo ? progressInfo.estimatedRemainingSec : -1
    
    // 移除本地的 _calculateRemainingTime() 函数
    // 移除 _timeSamples、_lastProgress 等本地状态
    
    // 进度条显示
    ColumnLayout {
        visible: status === 0 || status === 1
        
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: status === 0 ? qsTr("等待处理...") : currentStage || qsTr("处理中...")
                color: Theme.colors.foreground
                font.pixelSize: 12
            }
            
            Item { Layout.fillWidth: true }
            
            Text {
                visible: status === 1
                text: Math.round(progress) + "%"
                color: Theme.colors.primary
                font.pixelSize: 12
                font.weight: Font.Bold
            }
            
            Text {
                visible: status === 1 && estimatedRemainingSec > 0
                text: formatRemainingTime(estimatedRemainingSec)
                color: Theme.colors.mutedForeground
                font.pixelSize: 11
            }
        }
        
        // 进度条
        Rectangle {
            Layout.fillWidth: true
            height: 4
            radius: 2
            color: Theme.colors.muted
            
            Rectangle {
                width: status === 0 ? 0 : parent.width * Math.min(progress, 100) / 100
                height: parent.height
                radius: parent.radius
                color: Theme.colors.primary
                
                Behavior on width {
                    NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                }
            }
        }
    }
    
    function formatRemainingTime(seconds) {
        if (seconds <= 0) return ""
        if (seconds >= 3600) return qsTr("预计 %1h%2m").arg(Math.floor(seconds/3600)).arg(Math.floor((seconds%3600)/60))
        if (seconds >= 60) return qsTr("预计 %1m%2s").arg(Math.floor(seconds/60)).arg(seconds%60)
        return qsTr("预计 %1s").arg(seconds)
    }
}
```

---

## 五、质量保证

### 5.1 线程安全检查清单

- [ ] 所有 `std::atomic` 变量正确使用内存序
- [ ] 信号发射使用 `Qt::QueuedConnection` 跨线程
- [ ] 互斥锁保护非原子状态
- [ ] 避免死锁（锁顺序一致）
- [ ] QPointer 检查对象生命周期

### 5.2 内存管理检查清单

- [ ] 所有 `new` 对象有对应的 `delete` 或使用智能指针
- [ ] 信号连接在对象销毁前断开
- [ ] Qt 对象父子关系正确设置
- [ ] 无循环引用

### 5.3 性能优化检查清单

- [ ] 进度更新频率控制在 10-15 FPS
- [ ] 避免频繁的字符串操作
- [ ] QML 绑定最小化
- [ ] 使用 `Q_PROPERTY` 而非 `Q_INVOKABLE` 获取频繁更新的值

### 5.4 测试用例清单

| 测试类别 | 测试用例 | 预期结果 |
|----------|----------|----------|
| 单元测试 | ProgressReporter 边界值 | 0.0 和 1.0 正确处理 |
| 单元测试 | ProgressReporter 异常跳跃 | 异常值被过滤 |
| 单元测试 | ProgressManager 聚合计算 | 多任务进度正确聚合 |
| 单元测试 | 预估时间计算 | 误差在合理范围内 |
| 集成测试 | Shader 图像处理进度 | 进度平滑无跳跃 |
| 集成测试 | Shader 视频处理进度 | 帧级进度精确 |
| 集成测试 | AI 图像推理进度 | 分块进度正确报告 |
| 集成测试 | AI 视频推理进度 | 帧级进度精确 |
| 集成测试 | 取消任务进度 | 进度正确重置 |
| 集成测试 | 多任务并行 | 各任务进度独立 |
| 性能测试 | 高频进度更新 | UI 不卡顿 |
| 性能测试 | 长时间运行 | 无内存泄漏 |

---

## 六、风险与缓解措施

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 重构影响现有功能 | 高 | 分阶段实施，每阶段独立测试 |
| 线程安全问题 | 高 | 使用静态分析工具，增加线程安全测试 |
| 性能回退 | 中 | 性能基准测试，对比重构前后 |
| QML 绑定失效 | 中 | 保持 Q_PROPERTY 接口兼容 |
| 预估时间不准确 | 低 | 使用滑动窗口平均，增加样本数 |

---

## 七、验收标准

### 功能验收

1. **进度准确性**
   - 进度值与实际处理进度偏差 < 5%
   - 无进度回退现象
   - 进度跳跃幅度 < 10%

2. **显示流畅性**
   - 进度条更新频率稳定在 10-15 FPS
   - UI 操作响应时间 < 100ms
   - 无明显卡顿或闪烁

3. **预估时间准确性**
   - 预估时间误差 < 30%（处理进度 > 20% 后）
   - 预估时间随进度推进逐渐收敛

4. **线程安全**
   - 无数据竞争
   - 无死锁
   - 无崩溃

### 代码质量验收

1. **代码覆盖率**
   - 新增代码单元测试覆盖率 > 80%

2. **静态分析**
   - 无 Clang-Tidy 警告
   - 无 Clazy 警告

3. **内存检测**
   - Valgrind/ASan 无泄漏报告

---

## 八、时间估算

| 阶段 | 任务 | 预计时间 |
|------|------|----------|
| 阶段一 | 基础架构重构 | 2-3 天 |
| 阶段二 | Shader 模式进度增强 | 1-2 天 |
| 阶段三 | ProcessingController 重构 | 2 天 |
| 阶段四 | QML 层优化 | 1 天 |
| 阶段五 | 测试与验证 | 1-2 天 |
| **总计** | | **7-10 天** |

---

## 九、后续优化方向

1. **进度预测算法优化**
   - 引入机器学习模型预测处理时间
   - 基于历史数据优化预估

2. **可视化增强**
   - 添加处理速度曲线图
   - 显示各阶段耗时占比

3. **用户自定义**
   - 允许用户设置进度更新频率
   - 支持进度通知阈值

4. **断点续传支持**
   - 进度状态持久化
   - 中断后恢复处理
