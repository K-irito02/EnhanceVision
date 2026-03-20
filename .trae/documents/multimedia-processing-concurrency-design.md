# 多媒体文件处理系统并发与资源管理设计方案

## 一、问题分析

### 1.1 当前系统存在的问题

#### 问题一：并发处理能力不足
- **ImageProcessor/VideoProcessor 单任务限制**：`m_isProcessing` 标志导致一次只能处理一个文件
- **资源无限制**：没有内存、GPU显存、CPU使用率的上限控制
- **任务队列无优先级**：所有任务 FIFO 处理，无法响应用户当前关注的会话

#### 问题二：取消机制不完善
- **取消只是设置标志**：`cancel()` 设置 `m_cancelled = true`，但处理线程可能继续执行
- **资源未释放**：取消后临时文件、GPU资源可能未清理
- **状态不同步**：取消后消息状态可能未正确更新

#### 问题三：会话/消息删除时的资源泄漏
- **MessageModel::removeMessage**：直接删除消息，未通知 ProcessingController 取消相关任务
- **SessionController::deleteSession**：删除会话时未取消该会话下所有处理任务
- **正在处理的任务**：删除后任务可能继续运行，但结果无处存放

### 1.2 设计目标

| 目标 | 说明 |
|------|------|
| **流畅性** | UI 不阻塞，用户操作即时响应 |
| **稳定性** | 系统不崩溃，资源不泄漏 |
| **健壮性** | 异常情况正确处理，状态一致 |
| **可扩展性** | 支持未来 AI 推理等更重型的处理 |

---

## 二、系统架构设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              QML UI 层                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐   │
│  │ MainPage    │  │ MessageList │  │ ControlPanel│  │ Sidebar     │   │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘   │
└─────────┼────────────────┼────────────────┼────────────────┼───────────┘
          │                │                │                │
          ▼                ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          C++ 控制器层                                   │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │SessionController │  │ProcessingController│  │ FileController  │      │
│  │ - 会话生命周期   │  │ - 任务队列管理    │  │ - 文件上传管理  │      │
│  │ - 消息同步       │  │ - 并发控制        │  │ - 文件预览      │      │
│  └────────┬─────────┘  └────────┬─────────┘  └─────────────────┘      │
│           │                     │                                      │
│           │    ┌────────────────┼────────────────┐                     │
│           │    │                │                │                     │
│           ▼    ▼                ▼                ▼                     │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    TaskCoordinator (新增)                        │   │
│  │  - 任务生命周期管理                                              │   │
│  │  - 会话-任务关联追踪                                             │   │
│  │  - 取消/清理协调                                                 │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          处理引擎层                                     │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │ ImageProcessor   │  │ VideoProcessor   │  │ AIProcessor      │      │
│  │ - 支持可中断     │  │ - 支持可中断     │  │ (未来)           │      │
│  │ - 资源监控       │  │ - 帧级取消       │  │                  │      │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘      │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    ResourceManager (新增)                        │   │
│  │  - 内存使用监控                                                   │   │
│  │  - GPU 显存监控                                                   │   │
│  │  - 并发任务数限制                                                 │   │
│  │  - 资源压力降级                                                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 核心数据结构

```cpp
// 任务状态（扩展）
enum class TaskState {
    Pending,        // 队列等待
    Preparing,      // 准备资源
    Running,        // 处理中
    Pausing,        // 暂停中
    Paused,         // 已暂停
    Cancelling,     // 取消中
    Cancelled,      // 已取消
    Completed,      // 已完成
    Failed,         // 失败
    Orphaned        // 孤儿任务（关联的消息/会话已删除）
};

// 任务关联信息
struct TaskContext {
    QString taskId;
    QString messageId;
    QString sessionId;
    QString fileId;
    
    // 资源追踪
    qint64 estimatedMemoryMB;
    qint64 estimatedGpuMemoryMB;
    
    // 状态
    TaskState state;
    bool isCancellable;
    bool isPausable;
    
    // 取消令牌
    std::shared_ptr<std::atomic<bool>> cancelToken;
    std::shared_ptr<std::atomic<bool>> pauseToken;
};

// 资源配额
struct ResourceQuota {
    int maxConcurrentTasks = 4;
    qint64 maxMemoryMB = 4096;        // 4GB
    qint64 maxGpuMemoryMB = 2048;     // 2GB
    int maxTasksPerSession = 100;
};
```

---

## 三、核心组件设计

### 3.1 TaskCoordinator - 任务协调器（新增）

**职责**：
- 维护 sessionId/messageId/taskId 的关联关系
- 协调任务取消和资源清理
- 处理孤儿任务

```cpp
class TaskCoordinator : public QObject {
    Q_OBJECT
    
public:
    static TaskCoordinator* instance();
    
    // 注册任务关联
    void registerTask(const TaskContext& context);
    
    // 取消消息的所有任务
    void cancelMessageTasks(const QString& messageId);
    
    // 取消会话的所有任务
    void cancelSessionTasks(const QString& sessionId);
    
    // 检查任务是否孤儿
    bool isOrphaned(const QString& taskId) const;
    
    // 获取任务上下文
    TaskContext getTaskContext(const QString& taskId) const;
    
signals:
    void taskOrphaned(const QString& taskId);
    void taskCancelled(const QString& taskId, const QString& reason);
    
private:
    QHash<QString, TaskContext> m_taskContexts;
    QHash<QString, QSet<QString>> m_sessionToTasks;    // sessionId -> taskIds
    QHash<QString, QSet<QString>> m_messageToTasks;    // messageId -> taskIds
    QMutex m_mutex;
};
```

### 3.2 ResourceManager - 资源管理器（新增）

**职责**：
- 监控系统资源使用
- 动态调整并发限制
- 资源压力时降级处理

```cpp
class ResourceManager : public QObject {
    Q_OBJECT
    
public:
    static ResourceManager* instance();
    
    // 资源申请（返回是否成功）
    bool tryAcquire(const QString& taskId, qint64 memoryMB, qint64 gpuMemoryMB);
    
    // 资源释放
    void release(const QString& taskId);
    
    // 获取当前资源使用情况
    qint64 usedMemoryMB() const;
    qint64 usedGpuMemoryMB() const;
    int activeTaskCount() const;
    
    // 检查是否可以启动新任务
    bool canStartNewTask(qint64 estimatedMemoryMB, qint64 estimatedGpuMemoryMB) const;
    
    // 获取推荐的最大并发数
    int recommendedConcurrency() const;
    
signals:
    void resourcePressureChanged(int pressureLevel);  // 0=低, 1=中, 2=高
    void resourceExhausted();
    
private:
    ResourceQuota m_quota;
    QHash<QString, QPair<qint64, qint64>> m_taskResources;  // taskId -> (memory, gpuMemory)
    std::atomic<int> m_pressureLevel{0};
    QTimer* m_monitorTimer;
    
    void monitorResources();
};
```

### 3.3 ProcessingController 改进

```cpp
class ProcessingController : public QObject {
    // ... 现有成员 ...
    
    // 新增成员
    TaskCoordinator* m_taskCoordinator;
    ResourceManager* m_resourceManager;
    
    // 新增方法
    
    // 带资源检查的任务启动
    bool tryStartTask(QueueTask& task);
    
    // 处理孤儿任务
    void handleOrphanedTask(const QString& taskId);
    
    // 优雅取消
    void gracefulCancel(const QString& taskId, int timeoutMs = 5000);
    
    // 强制取消（最后手段）
    void forceCancel(const QString& taskId);
    
    // 会话切换时的处理
    void onSessionChanging(const QString& oldSessionId, const QString& newSessionId);
    
    // 暂停/恢复特定会话的任务
    void pauseSessionTasks(const QString& sessionId);
    void resumeSessionTasks(const QString& sessionId);
};
```

### 3.4 ImageProcessor 改进

```cpp
class ImageProcessor : public QObject {
    // ... 现有成员 ...
    
    // 新增：支持取消令牌
    std::shared_ptr<std::atomic<bool>> m_cancelToken;
    std::shared_ptr<std::atomic<bool>> m_pauseToken;
    
public:
    // 改进的处理方法
    void processImageAsync(
        const QString& inputPath,
        const QString& outputPath,
        const ShaderParams& params,
        std::shared_ptr<std::atomic<bool>> cancelToken,
        std::shared_ptr<std::atomic<bool>> pauseToken,
        ProgressCallback progressCallback,
        FinishCallback finishCallback
    );
    
    // 立即中断（用于强制取消）
    void interrupt();
    
private:
    // 在关键点检查取消/暂停
    bool shouldContinue() const;
    void waitIfPaused();
};
```

---

## 四、关键流程设计

### 4.1 并发处理流程

```
用户上传文件并发送处理
         │
         ▼
┌─────────────────────────────────────┐
│ ProcessingController::sendToProcessing │
│ 1. 创建 Message                      │
│ 2. 估算资源需求                      │
│ 3. 创建 TaskContext                 │
│ 4. 注册到 TaskCoordinator           │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│ ProcessingController::processNextTask │
│                                      │
│ while (有待处理任务 && 未达并发上限) { │
│   task = 获取下一个待处理任务        │
│   if (!ResourceManager::canStartNewTask) │
│     break;  // 资源不足，等待        │
│   if (TaskCoordinator::isOrphaned(task)) │
│     continue;  // 跳过孤儿任务       │
│   startTask(task);                   │
│ }                                    │
└─────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│ ProcessingController::startTask      │
│ 1. ResourceManager::tryAcquire      │
│ 2. 创建 cancelToken/pauseToken      │
│ 3. 更新任务状态为 Running           │
│ 4. 提交到线程池执行                 │
└─────────────────────────────────────┘
```

### 4.2 取消任务流程

```
用户删除消息/会话 或 手动取消任务
         │
         ▼
┌─────────────────────────────────────┐
│ TaskCoordinator::cancelMessageTasks  │
│ 或 cancelSessionTasks               │
│                                      │
│ for each taskId in 关联任务:        │
│   task = getTask(taskId)            │
│   switch (task.state) {             │
│     case Pending:                   │
│       直接移除，更新状态             │
│     case Running:                   │
│       gracefulCancel(taskId)        │
│     case Paused:                    │
│       resume后取消                   │
│   }                                 │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│ ProcessingController::gracefulCancel │
│                                      │
│ 1. 设置 cancelToken = true          │
│ 2. 设置状态为 Cancelling            │
│ 3. 等待最多 timeoutMs               │
│ 4. if (任务仍在运行) {              │
│      forceCancel(taskId)            │
│    }                                │
│ 5. ResourceManager::release         │
│ 6. 清理临时文件                      │
│ 7. 更新状态为 Cancelled             │
└─────────────────────────────────────┘
```

### 4.3 会话切换流程

```
用户切换会话
         │
         ▼
┌─────────────────────────────────────┐
│ SessionController::switchSession     │
│                                      │
│ 1. 保存当前会话消息                  │
│ 2. ProcessingController::onSessionChanging │
│    - 降低旧会话任务优先级（可选）    │
│    - 或保持原样继续处理              │
│ 3. 切换 SessionModel                │
│ 4. 加载新会话消息                    │
└─────────────────────────────────────┘
```

### 4.4 删除消息流程

```
用户删除消息
         │
         ▼
┌─────────────────────────────────────┐
│ MessageModel::removeMessage (改进)   │
│                                      │
│ 1. TaskCoordinator::cancelMessageTasks │
│    (先取消任务，再删除消息)          │
│ 2. 等待取消完成（或超时）            │
│ 3. 从列表中移除消息                  │
│ 4. 发出 messageRemoved 信号          │
└─────────────────────────────────────┘
```

### 4.5 删除/清空会话流程

```
用户删除会话
         │
         ▼
┌─────────────────────────────────────┐
│ SessionController::deleteSession     │
│                                      │
│ 1. TaskCoordinator::cancelSessionTasks │
│    (取消该会话下所有任务)            │
│ 2. 等待所有取消完成（或超时）        │
│ 3. SessionModel::deleteSession      │
│ 4. 如果是当前会话，切换到其他会话    │
│ 5. 发出 sessionDeleted 信号          │
└─────────────────────────────────────┘
```

---

## 五、实现步骤

### 第一阶段：基础设施（优先级：高）

#### 步骤 1.1：创建 TaskCoordinator
- 文件：`src/core/TaskCoordinator.cpp` / `include/EnhanceVision/core/TaskCoordinator.h`
- 功能：任务关联追踪、孤儿任务检测

#### 步骤 1.2：创建 ResourceManager
- 文件：`src/core/ResourceManager.cpp` / `include/EnhanceVision/core/ResourceManager.h`
- 功能：资源监控、配额管理

#### 步骤 1.3：扩展 DataTypes
- 添加 `TaskState` 枚举
- 添加 `TaskContext` 结构
- 添加 `ResourceQuota` 结构

### 第二阶段：核心改进（优先级：高）

#### 步骤 2.1：改进 ProcessingController
- 集成 TaskCoordinator 和 ResourceManager
- 改进 `processNextTask` 方法
- 添加 `gracefulCancel` 和 `forceCancel` 方法
- 添加会话切换处理

#### 步骤 2.2：改进 ImageProcessor
- 支持取消令牌
- 在处理循环中检查取消/暂停
- 添加 `interrupt` 方法

#### 步骤 2.3：改进 VideoProcessor
- 帧级取消支持
- FFmpeg 资源正确释放
- 支持暂停/恢复

### 第三阶段：集成改进（优先级：中）

#### 步骤 3.1：改进 MessageModel
- `removeMessage` 先取消任务
- `clear` 取消所有相关任务

#### 步骤 3.2：改进 SessionController
- `deleteSession` 先取消会话任务
- `clearSession` 先取消会话任务
- `deleteSelectedSessions` 批量取消

#### 步骤 3.3：改进 QML 层
- 删除确认对话框（处理中的任务）
- 任务状态显示优化
- 资源压力提示

### 第四阶段：测试与优化（优先级：中）

#### 步骤 4.1：单元测试
- TaskCoordinator 测试
- ResourceManager 测试
- 取消流程测试

#### 步骤 4.2：集成测试
- 并发处理测试
- 会话切换测试
- 删除操作测试

#### 步骤 4.3：压力测试
- 大量文件处理
- 资源耗尽场景
- 长时间运行

---

## 六、关键代码示例

### 6.1 TaskCoordinator 实现

```cpp
void TaskCoordinator::cancelMessageTasks(const QString& messageId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_messageToTasks.contains(messageId)) {
        return;
    }
    
    QSet<QString> taskIds = m_messageToTasks[messageId];
    
    for (const QString& taskId : taskIds) {
        if (m_taskContexts.contains(taskId)) {
            TaskContext& ctx = m_taskContexts[taskId];
            
            if (ctx.cancelToken) {
                ctx.cancelToken->store(true);
            }
            
            ctx.state = TaskState::Cancelling;
            emit taskCancelled(taskId, "message_deleted");
        }
    }
}

bool TaskCoordinator::isOrphaned(const QString& taskId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_taskContexts.contains(taskId)) {
        return true;
    }
    
    const TaskContext& ctx = m_taskContexts[taskId];
    
    // 检查消息是否还存在
    // 这需要与 MessageModel 协作
    return ctx.state == TaskState::Orphaned;
}
```

### 6.2 ResourceManager 实现

```cpp
bool ResourceManager::tryAcquire(const QString& taskId, qint64 memoryMB, qint64 gpuMemoryMB) {
    if (!canStartNewTask(memoryMB, gpuMemoryMB)) {
        return false;
    }
    
    m_taskResources[taskId] = qMakePair(memoryMB, gpuMemoryMB);
    return true;
}

bool ResourceManager::canStartNewTask(qint64 estimatedMemoryMB, qint64 estimatedGpuMemoryMB) const {
    // 检查并发数
    if (activeTaskCount() >= m_quota.maxConcurrentTasks) {
        return false;
    }
    
    // 检查内存
    if (usedMemoryMB() + estimatedMemoryMB > m_quota.maxMemoryMB) {
        return false;
    }
    
    // 检查 GPU 显存
    if (usedGpuMemoryMB() + estimatedGpuMemoryMB > m_quota.maxGpuMemoryMB) {
        return false;
    }
    
    return true;
}

void ResourceManager::monitorResources() {
    qint64 totalMemory = getSystemMemoryMB();
    qint64 availableMemory = getAvailableMemoryMB();
    qint64 usedPercent = (totalMemory - availableMemory) * 100 / totalMemory;
    
    int newPressureLevel = 0;
    if (usedPercent > 90) {
        newPressureLevel = 2;  // 高压力
    } else if (usedPercent > 75) {
        newPressureLevel = 1;  // 中压力
    }
    
    if (newPressureLevel != m_pressureLevel.load()) {
        m_pressureLevel.store(newPressureLevel);
        emit resourcePressureChanged(newPressureLevel);
    }
}
```

### 6.3 改进的 ImageProcessor

```cpp
void ImageProcessor::processImageAsync(
    const QString& inputPath,
    const QString& outputPath,
    const ShaderParams& params,
    std::shared_ptr<std::atomic<bool>> cancelToken,
    std::shared_ptr<std::atomic<bool>> pauseToken,
    ProgressCallback progressCallback,
    FinishCallback finishCallback
) {
    if (m_isProcessing) {
        if (finishCallback) {
            finishCallback(false, QString(), tr("已有处理任务正在进行"));
        }
        return;
    }

    m_isProcessing = true;
    m_cancelToken = cancelToken;
    m_pauseToken = pauseToken;

    QtConcurrent::run([this, inputPath, outputPath, params, progressCallback, finishCallback]() {
        bool success = false;
        QString error;
        QString resultPath;

        try {
            // 读取图像
            if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
            
            QImage inputImage(inputPath);
            if (inputImage.isNull()) {
                throw std::runtime_error(tr("无法读取图像文件").toStdString());
            }

            // 应用滤镜
            if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
            
            QImage resultImage = applyShader(inputImage, params);

            // 保存结果
            if (!shouldContinue()) throw std::runtime_error(tr("处理已取消").toStdString());
            
            if (!resultImage.save(outputPath)) {
                throw std::runtime_error(tr("无法保存图像文件").toStdString());
            }

            resultPath = outputPath;
            success = true;

        } catch (const std::exception& e) {
            error = QString::fromStdString(e.what());
            success = false;
        }

        m_isProcessing = false;
        m_cancelToken.reset();
        m_pauseToken.reset();

        if (finishCallback) {
            finishCallback(success, resultPath, error);
        }
        emit finished(success, resultPath, error);
    });
}

bool ImageProcessor::shouldContinue() const {
    if (m_cancelToken && m_cancelToken->load()) {
        return false;
    }
    
    // 检查暂停
    if (m_pauseToken && m_pauseToken->load()) {
        // 等待暂停解除
        while (m_pauseToken->load() && !(m_cancelToken && m_cancelToken->load())) {
            QThread::msleep(100);
        }
    }
    
    return !(m_cancelToken && m_cancelToken->load());
}
```

### 6.4 改进的 MessageModel::removeMessage

```cpp
bool MessageModel::removeMessage(const QString& messageId) {
    int index = findMessageIndex(messageId);
    if (index < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return false;
    }

    // 先取消该消息关联的所有任务
    TaskCoordinator::instance()->cancelMessageTasks(messageId);

    // 等待取消完成（最多 2 秒）
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000) {
        bool allCancelled = true;
        // 检查所有关联任务是否已取消
        // ...
        if (allCancelled) break;
        QCoreApplication::processEvents();
    }

    // 从列表中移除
    beginRemoveRows(QModelIndex(), index, index);
    m_messages.removeAt(index);
    endRemoveRows();

    emit countChanged();
    emit messageRemoved(messageId);
    return true;
}
```

---

## 七、异常场景处理

### 7.1 资源耗尽

```
资源耗尽检测
     │
     ▼
ResourceManager::resourcePressureChanged(2)
     │
     ▼
ProcessingController 处理:
  1. 暂停接受新任务
  2. 降低处理优先级
  3. 显示用户提示
  4. 等待资源释放
```

### 7.2 任务超时

```
任务执行超时
     │
     ▼
ProcessingController::onTaskTimeout
     │
     ▼
  1. 尝试优雅取消
  2. 等待 5 秒
  3. 强制中断
  4. 标记为 Failed
  5. 释放资源
```

### 7.3 孤儿任务

```
任务完成但消息已删除
     │
     ▼
ProcessingController::completeTask
     │
     ├─ 检查 TaskCoordinator::isOrphaned
     │
     ├─ 如果是孤儿:
     │    1. 删除临时文件
     │    2. 释放资源
     │    3. 不更新消息状态
     │
     └─ 如果不是孤儿:
          正常完成流程
```

---

## 八、性能优化建议

### 8.1 任务优先级

```cpp
enum class TaskPriority {
    UserInteractive = 0,  // 用户当前关注的会话
    UserInitiated = 1,    // 用户主动发起
    Utility = 2,          // 后台任务
    Background = 3        // 低优先级
};

// 根据会话是否活动调整优先级
void ProcessingController::adjustPriorityForActiveSession(const QString& sessionId) {
    for (auto& task : m_tasks) {
        if (getTaskContext(task.taskId).sessionId == sessionId) {
            task.priority = TaskPriority::UserInteractive;
        } else {
            task.priority = TaskPriority::UserInitiated;
        }
    }
}
```

### 8.2 资源预估

```cpp
qint64 estimateMemoryUsage(const QString& filePath, MediaType type) {
    QFileInfo info(filePath);
    qint64 fileSize = info.size();
    
    if (type == MediaType::Image) {
        // 图像处理大约需要 3-5 倍文件大小的内存
        return fileSize * 4 / (1024 * 1024);
    } else if (type == MediaType::Video) {
        // 视频处理需要帧缓存
        return qMax(512LL, fileSize / (1024 * 1024) / 10);
    }
    
    return 256;  // 默认 256MB
}
```

### 8.3 批量操作优化

```cpp
void SessionController::deleteSelectedSessions() {
    // 先收集所有要取消的任务
    QSet<QString> allTaskIds;
    for (const QString& sessionId : selectedSessionIds) {
        allTaskIds.unite(TaskCoordinator::instance()->sessionTaskIds(sessionId));
    }
    
    // 批量取消
    TaskCoordinator::instance()->cancelTasks(allTaskIds);
    
    // 等待取消完成
    waitForCancellation(allTaskIds, 5000);
    
    // 批量删除会话
    m_sessionModel->deleteSelectedSessions();
}
```

---

## 九、测试用例

### 9.1 并发处理测试

| 测试场景 | 预期结果 |
|----------|----------|
| 同时上传 10 个文件 | 按并发限制排队处理 |
| 处理中上传新文件 | 新文件加入队列 |
| 切换会话后上传 | 新会话独立处理 |
| 资源不足时上传 | 等待资源释放 |

### 9.2 取消测试

| 测试场景 | 预期结果 |
|----------|----------|
| 取消待处理任务 | 立即从队列移除 |
| 取消处理中任务 | 优雅停止，清理资源 |
| 删除处理中的消息 | 任务取消，消息删除 |
| 清空处理中的会话 | 所有任务取消，会话清空 |

### 9.3 异常测试

| 测试场景 | 预期结果 |
|----------|----------|
| 任务执行超时 | 强制中断，标记失败 |
| 内存不足 | 暂停新任务，提示用户 |
| GPU 不可用 | 降级到 CPU 处理 |
| 文件损坏 | 任务失败，错误提示 |

---

## 十、总结

本设计方案通过引入 **TaskCoordinator** 和 **ResourceManager** 两个核心组件，解决了当前系统在并发处理和资源管理方面的不足：

1. **并发处理**：通过资源配额和并发限制，确保系统在高负载下仍能流畅运行
2. **取消机制**：通过取消令牌和优雅取消流程，确保任务可以被正确中断
3. **资源管理**：通过资源监控和压力检测，防止系统资源耗尽
4. **会话隔离**：通过任务关联追踪，确保会话/消息删除时正确清理相关任务

实施建议：
- 优先实现 TaskCoordinator 和 ResourceManager
- 然后改进 ProcessingController 和 ImageProcessor
- 最后集成到 MessageModel 和 SessionController
- 完善测试覆盖
