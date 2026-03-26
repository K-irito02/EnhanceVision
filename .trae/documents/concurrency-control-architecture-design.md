# 会话级与消息级最大并发任务技术分析与架构设计

## 一、现有架构分析

### 1.1 核心组件现状

| 组件 | 职责 | 现有并发能力 |
|------|------|-------------|
| `ProcessingController` | 任务队列管理、调度执行 | 全局并发限制、AI单任务限制 |
| `TaskCoordinator` | 任务与会话/消息关联、取消协调 | 孤儿任务检测、取消令牌 |
| `ResourceManager` | 资源监控、动态调整 | 内存/GPU监控、压力级别 |
| `SessionController` | 会话生命周期管理 | 会话切换、消息索引 |
| `AIEngine` | NCNN推理引擎 | 单任务串行推理、GPU加速 |

### 1.2 现有数据结构

```cpp
// 任务状态枚举（已定义）
enum class TaskState {
    Pending, Preparing, Running, Pausing, Paused,
    Cancelling, Cancelled, Completed, Failed, Orphaned
};

// 任务优先级枚举（已定义）
enum class TaskPriority {
    UserInteractive = 0,  // 用户当前关注的会话
    UserInitiated = 1,    // 用户主动发起
    Utility = 2,          // 后台任务
    Background = 3        // 低优先级
};

// 资源配额结构（已定义）
struct ResourceQuota {
    int maxConcurrentTasks = 4;
    qint64 maxMemoryMB = 4096;
    qint64 maxGpuMemoryMB = 2048;
    int maxTasksPerSession = 100;
    int taskTimeoutMs = 300000;
    int cancelTimeoutMs = 5000;
};
```

### 1.3 现有并发控制机制

```
┌─────────────────────────────────────────────────────────────────┐
│                    ProcessingController                          │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ processNextTask() 调度逻辑                                   ││
│  │  ├─ 全局并发限制: m_maxConcurrentTasks                       ││
│  │  ├─ AI推理限制: m_activeAiTaskId (单任务串行)               ││
│  │  ├─ 会话级限制: m_activeTasksPerSession                     ││
│  │  └─ 消息级限制: m_activeTasksPerMessage                     ││
│  └─────────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ 特殊模式                                                    ││
│  │  ├─ InteractionPriorityMode: 降低并发为用户交互留资源       ││
│  │  └─ ImportBurstMode: 批量导入时降低并发                     ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### 1.4 现有问题分析

| 问题 | 描述 | 影响 |
|------|------|------|
| 会话级并发粒度不足 | 仅计数，无优先级队列 | 多会话时无法保证当前会话优先 |
| 消息级并发无队列 | 直接跳过超限消息 | 无法保证公平调度 |
| 无任务抢占机制 | 高优先级任务无法打断低优先级 | 用户交互响应慢 |
| 缺少死锁检测 | 无循环等待检测 | 极端情况可能卡死 |
| 资源预估不准 | 仅基于文件大小估算 | 实际使用可能超限 |

---

## 二、架构设计方案

### 2.1 双层并发控制架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          ConcurrencyManager (新增)                       │
│                                                                          │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │ SessionScheduler │  │ MessageScheduler │  │  TaskScheduler   │      │
│  │   (会话级调度)    │  │   (消息级调度)    │  │   (任务级调度)   │      │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘      │
│           │                     │                     │                 │
│           └─────────────────────┼─────────────────────┘                 │
│                                 ▼                                        │
│                    ┌────────────────────────┐                           │
│                    │   PriorityQueue<T>     │                           │
│                    │  (优先级任务队列)       │                           │
│                    └────────────────────────┘                           │
│                                 │                                        │
│                                 ▼                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                      ResourceGovernor                             │  │
│  │  ├─ MemoryTracker (内存追踪)                                      │  │
│  │  ├─ GpuMemoryTracker (GPU显存追踪)                                │  │
│  │  ├─ CpuLoadMonitor (CPU负载监控)                                  │  │
│  │  └─ DeadlockDetector (死锁检测)                                   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 核心类设计

#### 2.2.1 ConcurrencyManager - 并发管理器

```cpp
class ConcurrencyManager : public QObject {
    Q_OBJECT

public:
    static ConcurrencyManager* instance();
    
    // 配置接口
    void setSessionConcurrencyLimit(int maxConcurrentSessions);
    void setMessageConcurrencyLimit(int maxFilesPerMessage);
    void setGlobalConcurrencyLimit(int maxTasks);
    
    // 任务提交
    QString submitTask(const TaskContext& context, 
                       std::function<void()> executor);
    
    // 任务控制
    void pauseTask(const QString& taskId);
    void resumeTask(const QString& taskId);
    void cancelTask(const QString& taskId);
    void reprioritizeTask(const QString& taskId, TaskPriority newPriority);
    
    // 会话级控制
    void pauseSession(const QString& sessionId);
    void resumeSession(const QString& sessionId);
    void setSessionPriority(const QString& sessionId, TaskPriority priority);
    
    // 消息级控制
    void pauseMessage(const QString& messageId);
    void resumeMessage(const QString& messageId);
    
    // 状态查询
    TaskState getTaskState(const QString& taskId) const;
    SessionConcurrencyInfo getSessionInfo(const QString& sessionId) const;
    MessageConcurrencyInfo getMessageInfo(const QString& messageId) const;
    
    // 资源查询
    ConcurrencyStatistics getStatistics() const;

signals:
    void taskStateChanged(const QString& taskId, TaskState newState);
    void sessionConcurrencyChanged(const QString& sessionId);
    void messageConcurrencyChanged(const QString& messageId);
    void resourcePressureChanged(int pressureLevel);
    void deadlockDetected(const QStringList& involvedTaskIds);
    void resourceExhausted(const QString& resourceType);

private:
    // 会话级调度器
    struct SessionSlot {
        QString sessionId;
        TaskPriority priority;
        int activeTaskCount;
        int maxConcurrentTasks;
        QQueue<QString> pendingTasks;
        QSet<QString> activeTasks;
        bool isPaused;
    };
    
    // 消息级调度器
    struct MessageSlot {
        QString messageId;
        QString sessionId;
        int activeTaskCount;
        int maxConcurrentTasks;
        QQueue<QString> pendingTasks;
        QSet<QString> activeTasks;
        bool isPaused;
    };
    
    QHash<QString, SessionSlot> m_sessions;
    QHash<QString, MessageSlot> m_messages;
    QHash<QString, TaskContext> m_taskContexts;
    
    int m_maxConcurrentSessions;
    int m_maxFilesPerMessage;
    int m_globalMaxTasks;
    
    std::unique_ptr<ResourceGovernor> m_resourceGovernor;
    std::unique_ptr<DeadlockDetector> m_deadlockDetector;
};
```

#### 2.2.2 PriorityQueue - 优先级队列

```cpp
template<typename T>
class PriorityQueue {
public:
    void enqueue(const T& item, TaskPriority priority);
    T dequeue();
    T peek() const;
    
    void updatePriority(const QString& itemId, TaskPriority newPriority);
    void remove(const QString& itemId);
    
    bool isEmpty() const;
    int size() const;
    int sizeByPriority(TaskPriority priority) const;
    
    QList<T> itemsByPriority(TaskPriority priority) const;
    void clear();

private:
    // 使用多级队列实现 O(1) 优先级访问
    QHash<TaskPriority, QQueue<T>> m_queues;
    QHash<QString, TaskPriority> m_itemPriorities;
    int m_totalSize = 0;
};
```

#### 2.2.3 ResourceGovernor - 资源治理器

```cpp
class ResourceGovernor : public QObject {
    Q_OBJECT

public:
    explicit ResourceGovernor(QObject* parent = nullptr);
    
    // 资源申请
    bool tryAcquire(const QString& taskId, 
                    qint64 memoryMB, 
                    qint64 gpuMemoryMB);
    void release(const QString& taskId);
    
    // 资源查询
    qint64 usedMemoryMB() const;
    qint64 usedGpuMemoryMB() const;
    qint64 availableMemoryMB() const;
    qint64 availableGpuMemoryMB() const;
    
    // 压力级别
    ResourcePressure pressureLevel() const;
    qreal memoryUsagePercent() const;
    qreal gpuMemoryUsagePercent() const;
    
    // 预警机制
    void setWarningThreshold(qreal percent);  // 默认 75%
    void setCriticalThreshold(qreal percent); // 默认 90%
    
    // 动态调整
    int recommendedConcurrency() const;
    bool shouldThrottle() const;
    bool shouldReject() const;

signals:
    void warningThresholdReached();
    void criticalThresholdReached();
    void resourceAvailable();

private:
    struct ResourceAllocation {
        QString taskId;
        qint64 memoryMB;
        qint64 gpuMemoryMB;
        QDateTime allocatedAt;
    };
    
    QHash<QString, ResourceAllocation> m_allocations;
    std::atomic<qint64> m_totalMemoryMB{0};
    std::atomic<qint64> m_totalGpuMemoryMB{0};
    std::atomic<int> m_pressureLevel{0};
    
    qreal m_warningThreshold = 0.75;
    qreal m_criticalThreshold = 0.90;
    
    void updatePressureLevel();
    qint64 querySystemMemory() const;
    qint64 queryGpuMemory() const;
};
```

#### 2.2.4 DeadlockDetector - 死锁检测器

```cpp
class DeadlockDetector : public QObject {
    Q_OBJECT

public:
    explicit DeadlockDetector(QObject* parent = nullptr);
    
    // 资源依赖注册
    void registerDependency(const QString& taskId, 
                           const QString& resourceId);
    void unregisterDependency(const QString& taskId);
    
    // 死锁检测
    bool detectDeadlock() const;
    QStringList findDeadlockedTasks() const;
    
    // 超时检测
    void setTaskTimeout(qint64 timeoutMs);
    QStringList findTimedOutTasks() const;
    
    // 自动恢复
    void enableAutoRecovery(bool enabled);
    void setRecoveryStrategy(RecoveryStrategy strategy);

signals:
    void deadlockDetected(const QStringList& involvedTaskIds);
    void taskTimeout(const QString& taskId);
    void recoveryAttempted(const QString& taskId, bool success);

public slots:
    void performDetection();
    void attemptRecovery(const QStringList& deadlockedTaskIds);

private:
    struct DependencyNode {
        QString taskId;
        QSet<QString> heldResources;
        QSet<QString> waitingResources;
        QDateTime startTime;
    };
    
    QHash<QString, DependencyNode> m_nodes;
    QHash<QString, QSet<QString>> m_resourceHolders;
    qint64 m_taskTimeoutMs = 300000; // 5分钟
    bool m_autoRecoveryEnabled = true;
    RecoveryStrategy m_recoveryStrategy = RecoveryStrategy::CancelYoungest;
    
    bool hasCycle(const QString& startTaskId, 
                  QSet<QString>& visited,
                  QSet<QString>& recursionStack) const;
};
```

### 2.3 任务生命周期管理

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         Task Lifecycle State Machine                     │
└─────────────────────────────────────────────────────────────────────────┘

                    ┌───────────┐
                    │  Created  │
                    └─────┬─────┘
                          │ submit()
                          ▼
                    ┌───────────┐
          ┌────────│  Pending  │◄────────┐
          │        └─────┬─────┘         │
          │              │ start()       │
          │              ▼               │
          │        ┌───────────┐         │
          │        │ Preparing │         │
          │        └─────┬─────┘         │
          │              │ ready()       │
          │              ▼               │
          │        ┌───────────┐         │
          │   ┌───►│  Running  │◄──────┐ │
          │   │    └─────┬─────┘       │ │
          │   │          │             │ │
          │   │    ┌─────┴─────┐       │ │
          │   │    │           │       │ │
          │   │    ▼           ▼       │ │
          │   │ ┌────────┐ ┌────────┐  │ │
          │   │ │Pausing │ │Cancelling│ │
          │   │ └───┬────┘ └────┬───┘  │ │
          │   │     │           │      │ │
          │   │     ▼           ▼      │ │
          │   │ ┌────────┐ ┌────────┐  │ │
          │   └─│ Paused │ │Cancelled│  │ │
          │     └────────┘ └────────┘  │ │
          │                             │ │
          │     resume()   cancel()     │ │
          └─────────────────────────────┘ │
                                          │
                    ┌─────────────────────┘
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
    ┌───────────┐       ┌───────────┐
    │ Completed │       │   Failed  │
    └───────────┘       └───────────┘
```

### 2.4 并发控制流程

#### 2.4.1 任务提交流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Task Submission Flow                            │
└─────────────────────────────────────────────────────────────────────────┘

用户提交任务
      │
      ▼
┌─────────────────┐
│ 创建 TaskContext │
│ - 估算资源需求   │
│ - 设置优先级     │
│ - 生成取消令牌   │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│ ConcurrencyManager::submitTask()                             │
│                                                              │
│  1. 检查全局并发限制                                         │
│     └─ if (activeTaskCount >= globalMax) → 加入全局等待队列 │
│                                                              │
│  2. 检查会话级并发限制                                       │
│     └─ if (sessionActiveCount >= sessionMax) {              │
│            if (sessionPriority < current) → 抢占考虑        │
│            else → 加入会话等待队列                           │
│        }                                                     │
│                                                              │
│  3. 检查消息级并发限制                                       │
│     └─ if (messageActiveCount >= messageMax) {              │
│            → 加入消息等待队列                                │
│        }                                                     │
│                                                              │
│  4. 资源申请                                                 │
│     └─ ResourceGovernor::tryAcquire()                       │
│         ├─ 成功 → 进入 Preparing 状态                       │
│         └─ 失败 → 加入资源等待队列                          │
│                                                              │
│  5. 注册依赖关系                                             │
│     └─ DeadlockDetector::registerDependency()               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────┐
│  任务开始执行    │
│  State: Running │
└─────────────────┘
```

#### 2.4.2 Shader模式并发流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Shader Mode Concurrency Flow                          │
└─────────────────────────────────────────────────────────────────────────┘

特点：
- CPU密集型，GPU可选
- 可并行处理多个文件
- 资源需求相对固定

并发策略：
┌──────────────────────────────────────────────────────────────────────┐
│ Session A (当前活动)                                                   │
│  └─ Message 1: [File1(Running), File2(Running), File3(Pending)...]   │
│  └─ Message 2: [File4(Pending)...]                                   │
│                                                                       │
│ Session B (后台)                                                      │
│  └─ Message 3: [File5(Running), File6(Pending)...]                   │
│                                                                       │
│ 配置示例：                                                            │
│  maxConcurrentSessions = 2                                           │
│  maxFilesPerMessage = 4                                              │
│  globalMaxTasks = 8                                                  │
└──────────────────────────────────────────────────────────────────────┘

调度优先级：
1. 当前活动会话的消息优先
2. 同一消息内按文件顺序
3. 后台会话按提交顺序
```

#### 2.4.3 AI推理模式并发流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    AI Inference Mode Concurrency Flow                   │
└─────────────────────────────────────────────────────────────────────────┘

特点：
- GPU密集型
- 单任务独占GPU资源
- 内存需求大

并发策略：
┌──────────────────────────────────────────────────────────────────────┐
│ AI任务队列 (串行执行)                                                 │
│                                                                       │
│  [Task1(Running-GPU)] → [Task2(Waiting)] → [Task3(Waiting)]         │
│                                                                       │
│ 同时可并行：                                                          │
│  - Shader任务 (使用CPU)                                              │
│  - 缩略图生成 (低优先级)                                             │
│  - 文件IO操作                                                        │
└──────────────────────────────────────────────────────────────────────┘

GPU资源管理：
┌──────────────────────────────────────────────────────────────────────┐
│ GpuResourceManager                                                    │
│                                                                       │
│  - 单一GPU锁: m_gpuInferenceMutex                                    │
│  - 显存追踪: m_usedGpuMemoryMB                                       │
│  - 自动降级: GPU OOM → CPU回退                                       │
│  - 分块处理: 大图自动分块避免OOM                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.5 资源管理保障

#### 2.5.1 线程安全机制

```cpp
// 1. 读写锁保护共享数据
class ThreadSafeTaskRegistry {
    mutable QReadWriteLock m_lock;
    QHash<QString, TaskContext> m_tasks;
    
public:
    TaskContext getTask(const QString& id) const {
        QReadLocker locker(&m_lock);
        return m_tasks.value(id);
    }
    
    void updateTask(const QString& id, const TaskContext& ctx) {
        QWriteLocker locker(&m_lock);
        m_tasks[id] = ctx;
    }
};

// 2. 原子操作保护计数器
class AtomicCounter {
    std::atomic<int> m_count{0};
public:
    void increment() { m_count.fetch_add(1, std::memory_order_relaxed); }
    void decrement() { m_count.fetch_sub(1, std::memory_order_relaxed); }
    int get() const { return m_count.load(std::memory_order_relaxed); }
};

// 3. 取消令牌
class CancellationToken {
    std::atomic<bool> m_cancelled{false};
public:
    bool isCancelled() const { return m_cancelled.load(); }
    void cancel() { m_cancelled.store(true); }
    void reset() { m_cancelled.store(false); }
};
```

#### 2.5.2 资源耗尽防护

```cpp
class ResourceExhaustionHandler {
public:
    enum class Action {
        Reject,           // 拒绝新任务
        Throttle,         // 降低处理速度
        CancelOldest,     // 取消最老任务
        CancelLowPriority // 取消低优先级任务
    };
    
    Action determineAction(const ResourcePressure& pressure) {
        if (pressure.memoryPercent > 0.95) {
            return Action::CancelLowPriority;
        } else if (pressure.memoryPercent > 0.90) {
            return Action::Reject;
        } else if (pressure.memoryPercent > 0.80) {
            return Action::Throttle;
        }
        return Action::Reject;
    }
    
    void executeAction(Action action) {
        switch (action) {
            case Action::Throttle:
                m_concurrencyManager->reduceConcurrency(0.5);
                break;
            case Action::CancelLowPriority:
                m_concurrencyManager->cancelByPriority(TaskPriority::Background);
                break;
            // ...
        }
    }
};
```

#### 2.5.3 死锁检测与恢复

```cpp
class DeadlockRecovery {
public:
    enum class Strategy {
        CancelYoungest,    // 取消最新任务
        CancelOldest,      // 取消最老任务
        CancelLowPriority, // 取消低优先级
        RandomVictim       // 随机牺牲
    };
    
    void recoverFromDeadlock(const QStringList& deadlockedTasks) {
        qWarning() << "Deadlock detected, tasks:" << deadlockedTasks;
        
        QString victim = selectVictim(deadlockedTasks, m_strategy);
        
        // 尝试优雅取消
        if (m_concurrencyManager->gracefulCancel(victim, 5000)) {
            qInfo() << "Deadlock resolved by cancelling:" << victim;
            return;
        }
        
        // 强制终止
        m_concurrencyManager->forceCancel(victim);
        qWarning() << "Deadlock resolved by force cancel:" << victim;
    }
    
private:
    QString selectVictim(const QStringList& tasks, Strategy strategy) {
        // 根据策略选择牺牲任务
        switch (strategy) {
            case Strategy::CancelYoungest:
                return findYoungestTask(tasks);
            case Strategy::CancelLowPriority:
                return findLowestPriorityTask(tasks);
            // ...
        }
    }
};
```

---

## 三、性能优化设计

### 3.1 负载均衡策略

```cpp
class LoadBalancer {
public:
    struct NodeLoad {
        QString nodeId;
        int activeTasks;
        qint64 usedMemoryMB;
        qreal cpuUsage;
        qreal gpuUsage;
    };
    
    QString selectOptimalNode(const TaskContext& task) {
        // 1. 收集各节点负载
        QList<NodeLoad> loads = collectNodeLoads();
        
        // 2. 过滤不可用节点
        loads = filterAvailableNodes(loads, task);
        
        // 3. 计算综合得分
        for (auto& load : loads) {
            load.score = calculateScore(load, task);
        }
        
        // 4. 选择最优节点
        std::sort(loads.begin(), loads.end(), 
                  [](const auto& a, const auto& b) { return a.score > b.score; });
        
        return loads.first().nodeId;
    }
    
private:
    qreal calculateScore(const NodeLoad& load, const TaskContext& task) {
        // 综合考虑CPU、内存、GPU、任务数
        qreal cpuScore = 1.0 - load.cpuUsage;
        qreal memScore = 1.0 - (load.usedMemoryMB / m_maxMemoryMB);
        qreal taskScore = 1.0 - (load.activeTasks / m_maxTasks);
        
        // GPU任务额外考虑GPU负载
        qreal gpuScore = 1.0;
        if (task.requiresGpu) {
            gpuScore = 1.0 - load.gpuUsage;
        }
        
        return cpuScore * 0.3 + memScore * 0.3 + taskScore * 0.2 + gpuScore * 0.2;
    }
};
```

### 3.2 任务优先级调整

```cpp
class PriorityAdjuster {
public:
    void adjustPriorities() {
        // 1. 用户交互会话提升优先级
        promoteActiveSession();
        
        // 2. 长时间等待任务提升优先级（防止饥饿）
        preventStarvation();
        
        // 3. 资源紧张时降低后台任务优先级
        throttleBackgroundTasks();
    }
    
private:
    void preventStarvation() {
        constexpr qint64 kStarvationThresholdMs = 30000; // 30秒
        
        for (auto& task : m_pendingTasks) {
            qint64 waitTime = task.queuedAt.msecsTo(QDateTime::currentDateTime());
            
            if (waitTime > kStarvationThresholdMs) {
                // 逐级提升优先级
                if (task.priority == TaskPriority::Background) {
                    task.priority = TaskPriority::Utility;
                } else if (task.priority == TaskPriority::Utility) {
                    task.priority = TaskPriority::UserInitiated;
                }
            }
        }
    }
    
    void promoteActiveSession() {
        QString activeSessionId = m_sessionController->activeSessionId();
        
        for (auto& task : m_pendingTasks) {
            if (task.sessionId == activeSessionId && 
                task.priority > TaskPriority::UserInteractive) {
                task.priority = TaskPriority::UserInteractive;
            }
        }
    }
};
```

### 3.3 响应速度优化

```cpp
class ResponsivenessOptimizer {
public:
    // 预加载机制
    void preloadResources(const QString& sessionId) {
        // 预加载模型
        for (const auto& message : getPendingMessages(sessionId)) {
            if (message.mode == ProcessingMode::AIInference) {
                m_aiEngine->preloadModel(message.aiParams.modelId);
            }
        }
        
        // 预生成缩略图
        for (const auto& file : getPendingFiles(sessionId)) {
            m_thumbnailProvider->preloadThumbnail(file.filePath);
        }
    }
    
    // 快速响应模式
    void enableQuickResponse(bool enabled) {
        if (enabled) {
            // 降低并发，为用户交互预留资源
            m_concurrencyManager->setConcurrencyLimit(
                m_baseConcurrencyLimit - 1);
            
            // 提升UI线程优先级
            QThread::currentThread()->setPriority(QThread::HighPriority);
        } else {
            m_concurrencyManager->setConcurrencyLimit(m_baseConcurrencyLimit);
            QThread::currentThread()->setPriority(QThread::NormalPriority);
        }
    }
};
```

---

## 四、质量保障设计

### 4.1 单元测试设计

```cpp
class ConcurrencyManagerTest : public QObject {
    Q_OBJECT

private slots:
    // 基础功能测试
    void testTaskSubmission();
    void testTaskCancellation();
    void testTaskPrioritization();
    
    // 会话级并发测试
    void testSessionConcurrencyLimit();
    void testSessionPriorityOverride();
    void testSessionPauseResume();
    
    // 消息级并发测试
    void testMessageConcurrencyLimit();
    void testMessageFileOrdering();
    
    // 资源管理测试
    void testResourceAcquisition();
    void testResourceRelease();
    void testResourceExhaustion();
    
    // 死锁测试
    void testDeadlockDetection();
    void testDeadlockRecovery();
    
    // 边界条件测试
    void testEmptyQueue();
    void testFullQueue();
    void testConcurrentSubmission();
};

// 测试用例示例
void ConcurrencyManagerTest::testSessionConcurrencyLimit() {
    ConcurrencyManager mgr;
    mgr.setSessionConcurrencyLimit(2);
    mgr.setMessageConcurrencyLimit(4);
    
    // 创建会话
    QString session1 = mgr.createSession();
    QString session2 = mgr.createSession();
    QString session3 = mgr.createSession();
    
    // 提交任务
    int runningCount = 0;
    for (int i = 0; i < 10; ++i) {
        TaskContext ctx;
        ctx.sessionId = (i < 4) ? session1 : ((i < 8) ? session2 : session3);
        mgr.submitTask(ctx, [&runningCount]() { 
            runningCount++; 
            QThread::msleep(100); 
            runningCount--;
        });
    }
    
    // 验证：最多2个会话并发
    QTRY_COMPARE(mgr.activeSessionCount(), 2);
    
    // 等待完成
    QTRY_COMPARE(mgr.totalCompletedTasks(), 10);
}
```

### 4.2 集成测试设计

```cpp
class ConcurrencyIntegrationTest : public QObject {
    Q_OBJECT

private slots:
    void testShaderModeEndToEnd();
    void testAIInferenceModeEndToEnd();
    void testMixedModeProcessing();
    void testMultiSessionScenario();
    void testResourcePressureScenario();
    void testCrashRecovery();
    
private:
    void setupTestEnvironment();
    void cleanupTestEnvironment();
};

void ConcurrencyIntegrationTest::testMultiSessionScenario() {
    // 模拟多会话场景
    QVector<QString> sessions;
    for (int i = 0; i < 5; ++i) {
        sessions.append(m_sessionController->createSession());
    }
    
    // 每个会话提交多个消息
    for (const auto& sessionId : sessions) {
        m_sessionController->switchSession(sessionId);
        
        for (int j = 0; j < 3; ++j) {
            QList<MediaFile> files = createTestFiles(5);
            m_fileController->addFiles(files);
            m_processingController->sendToProcessing(
                static_cast<int>(ProcessingMode::Shader), 
                defaultShaderParams());
        }
    }
    
    // 验证并发限制
    QVERIFY(m_concurrencyManager->activeSessionCount() <= 
            m_settings->maxConcurrentSessions());
    
    // 验证所有任务完成
    QTRY_COMPARE_WITH_TIMEOUT(
        m_processingController->queueSize(), 0, 60000);
}
```

### 4.3 压力测试设计

```cpp
class ConcurrencyStressTest : public QObject {
    Q_OBJECT

private slots:
    void testHighConcurrencyStress();
    void testMemoryPressureStress();
    void testLongRunningStress();
    void testRapidSubmissionStress();
    
private:
    QElapsedTimer m_timer;
    QHash<QString, qint64> m_metrics;
};

void ConcurrencyStressTest::testHighConcurrencyStress() {
    const int kTaskCount = 1000;
    const int kSessionCount = 20;
    
    m_timer.start();
    
    // 快速提交大量任务
    QVector<QString> sessions;
    for (int i = 0; i < kSessionCount; ++i) {
        sessions.append(m_sessionController->createSession());
    }
    
    std::atomic<int> completedCount{0};
    for (int i = 0; i < kTaskCount; ++i) {
        TaskContext ctx;
        ctx.sessionId = sessions[i % kSessionCount];
        
        m_concurrencyManager->submitTask(ctx, [&completedCount]() {
            QThread::msleep(10); // 模拟工作
            completedCount++;
        });
    }
    
    // 等待完成
    QTRY_COMPARE_WITH_TIMEOUT(
        completedCount.load(), kTaskCount, 120000);
    
    qint64 elapsed = m_timer.elapsed();
    qInfo() << "Stress test completed:" << kTaskCount 
            << "tasks in" << elapsed << "ms"
            << "throughput:" << (kTaskCount * 1000.0 / elapsed) << "tasks/sec";
    
    // 验证无内存泄漏
    QVERIFY(m_resourceManager->usedMemoryMB() < 100);
}
```

### 4.4 性能基准指标

```yaml
performance_benchmarks:
  task_throughput:
    shader_mode:
      single_session:
        target: 30 tasks/sec
        threshold: 20 tasks/sec
      multi_session:
        target: 25 tasks/sec
        threshold: 15 tasks/sec
    ai_inference_mode:
      single_session:
        target: 2 tasks/sec  # GPU限制
        threshold: 1 task/sec
      multi_session:
        target: 1.5 tasks/sec
        threshold: 0.8 tasks/sec
  
  response_time:
    task_submission:
      p50: 5ms
      p95: 20ms
      p99: 50ms
    task_cancellation:
      p50: 10ms
      p95: 50ms
      p99: 100ms
    session_switch:
      p50: 50ms
      p95: 100ms
      p99: 200ms
  
  resource_efficiency:
    memory_overhead_per_task: 5MB
    gpu_memory_overhead_per_task: 50MB
    cpu_utilization_target: 80%
    gpu_utilization_target: 90%
  
  stability:
    max_concurrent_tasks: 100
    max_sessions: 50
    max_tasks_per_session: 500
    max_queue_wait_time: 5min
    max_task_duration: 30min
```

### 4.5 监控体系设计

```cpp
class ConcurrencyMonitor : public QObject {
    Q_OBJECT

public:
    struct Metrics {
        // 任务指标
        int totalTasks;
        int pendingTasks;
        int runningTasks;
        int completedTasks;
        int failedTasks;
        
        // 会话指标
        int activeSessions;
        int totalSessions;
        
        // 资源指标
        qint64 usedMemoryMB;
        qint64 usedGpuMemoryMB;
        qreal cpuUsage;
        qreal gpuUsage;
        
        // 性能指标
        qreal avgTaskDurationMs;
        qreal avgQueueWaitMs;
        qreal throughput;
    };
    
    void startMonitoring(int intervalMs = 1000);
    void stopMonitoring();
    
    Metrics currentMetrics() const;
    Metrics aggregatedMetrics(qint64 periodMs) const;
    
    void exportMetrics(const QString& format);
    
signals:
    void metricsUpdated(const Metrics& metrics);
    void alertTriggered(const QString& alertType, const QString& message);

private:
    QTimer* m_monitorTimer;
    Metrics m_currentMetrics;
    QList<Metrics> m_history;
    
    void collectMetrics();
    void checkAlerts();
    void updateHistory();
};
```

---

## 五、实现计划

### 5.1 阶段划分

```
Phase 1: 基础架构 (预计 3 天)
├── ConcurrencyManager 核心类实现
├── PriorityQueue 优先级队列实现
├── 基础单元测试
└── 集成到现有 ProcessingController

Phase 2: 资源管理 (预计 2 天)
├── ResourceGovernor 增强
├── 死锁检测器实现
├── 资源耗尽处理机制
└── 相关测试用例

Phase 3: 高级特性 (预计 2 天)
├── 任务抢占机制
├── 负载均衡策略
├── 优先级动态调整
└── 性能优化

Phase 4: 质量保障 (预计 2 天)
├── 完整单元测试套件
├── 集成测试套件
├── 压力测试套件
└── 性能基准验证

Phase 5: 监控与文档 (预计 1 天)
├── 监控体系实现
├── 性能指标采集
├── 技术文档编写
└── 用户指南更新
```

### 5.2 文件结构

```
src/core/
├── ConcurrencyManager.cpp      # 并发管理器实现
├── ResourceGovernor.cpp        # 资源治理器增强
├── DeadlockDetector.cpp        # 死锁检测器
├── LoadBalancer.cpp            # 负载均衡器
├── PriorityAdjuster.cpp        # 优先级调整器
└── ConcurrencyMonitor.cpp      # 监控器

include/EnhanceVision/core/
├── ConcurrencyManager.h
├── ResourceGovernor.h
├── DeadlockDetector.h
├── LoadBalancer.h
├── PriorityAdjuster.h
├── ConcurrencyMonitor.h
└── ConcurrencyTypes.h          # 并发相关类型定义

tests/
├── test_concurrency_manager.cpp
├── test_resource_governor.cpp
├── test_deadlock_detector.cpp
├── test_concurrency_integration.cpp
└── test_concurrency_stress.cpp
```

### 5.3 配置项设计

```cpp
// SettingsController 新增配置项
Q_PROPERTY(int maxConcurrentSessions READ maxConcurrentSessions 
           WRITE setMaxConcurrentSessions NOTIFY maxConcurrentSessionsChanged)
Q_PROPERTY(int maxConcurrentFilesPerMessage READ maxConcurrentFilesPerMessage 
           WRITE setMaxConcurrentFilesPerMessage NOTIFY maxConcurrentFilesPerMessageChanged)
Q_PROPERTY(int taskTimeoutMs READ taskTimeoutMs 
           WRITE setTaskTimeoutMs NOTIFY taskTimeoutMsChanged)
Q_PROPERTY(bool enableDeadlockDetection READ enableDeadlockDetection 
           WRITE setEnableDeadlockDetection NOTIFY enableDeadlockDetectionChanged)
Q_PROPERTY(bool enableAutoRecovery READ enableAutoRecovery 
           WRITE setEnableAutoRecovery NOTIFY enableAutoRecoveryChanged)
Q_PROPERTY(qreal memoryWarningThreshold READ memoryWarningThreshold 
           WRITE setMemoryWarningThreshold NOTIFY memoryWarningThresholdChanged)
Q_PROPERTY(qreal memoryCriticalThreshold READ memoryCriticalThreshold 
           WRITE setMemoryCriticalThreshold NOTIFY memoryCriticalThresholdChanged)
```

---

## 六、风险与缓解措施

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| GPU资源竞争导致性能下降 | 高 | 实现GPU资源锁、自动降级机制 |
| 死锁导致系统卡死 | 高 | 死锁检测、自动恢复、超时机制 |
| 内存泄漏导致崩溃 | 高 | 资源追踪、自动清理、压力测试 |
| 高并发下UI卡顿 | 中 | 信号节流、异步更新、快速响应模式 |
| 任务饥饿导致响应慢 | 中 | 优先级老化、公平调度、监控告警 |
| 跨平台兼容性问题 | 中 | 平台适配层、条件编译、测试覆盖 |

---

## 七、总结

本设计方案针对 Shader 模式和 AI 推理模式的并发处理需求，设计了完整的双层并发控制架构：

1. **并发控制机制**：会话级和消息级双层限制，优先级队列调度，资源动态调整
2. **资源管理保障**：线程安全访问、资源耗尽防护、死锁检测恢复
3. **任务管理架构**：完整生命周期管理、状态实时追踪、错误处理重试
4. **性能优化**：负载均衡、优先级调整、响应速度优化
5. **质量保障**：单元测试、集成测试、压力测试、性能基准、监控体系

该方案将确保系统在高并发场景下的稳定性、响应性和资源利用效率。
