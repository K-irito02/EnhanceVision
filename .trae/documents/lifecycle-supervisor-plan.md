# 生命周期监管与进程退出问题修复计划

## 问题诊断

### 1. 核心问题：窗口闪退但后台进程仍在运行

**根本原因分析：**

1. **QtConcurrent 任务不受 QApplication 生命周期控制**
   - `AIEngine::processVideoInternal` 使用 `QtConcurrent::run` 在全局线程池中运行
   - 当主窗口崩溃（ntdll.dll 异常 0xc0000005/0xc0000374）时，后台任务继续执行
   - `QApplication::quit()` 只停止事件循环，不会自动终止 `QtConcurrent` 任务

2. **生命周期守护机制存在缺陷**
   - `lifecycleWatchdog` 检查间隔 1200ms 太长
   - 当窗口崩溃时，`scheduleHardExit` 可能根本没机会执行
   - `QApplication::lastWindowClosed` 信号在崩溃场景下不会触发

3. **视频处理崩溃点**
   - 日志显示崩溃发生在 `sws_scale` 或 `processFrame` 调用时
   - 异常码 `0xc0000005`（访问违例）和 `0xc0000374`（堆损坏）
   - 可能是 QImage 内存访问问题或 NCNN Vulkan 推理时的 GPU 内存问题

### 2. 当前架构问题

- 生命周期逻辑分散在 `Application` 类中
- 缺乏统一的关闭流程和状态机
- 后台任务取消机制不够健壮
- 缺少崩溃场景的兜底处理

---

## 解决方案设计

### 架构设计：LifecycleSupervisor（生命周期监管器）

```
┌─────────────────────────────────────────────────────────────────┐
│                    LifecycleSupervisor                           │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    State Machine                          │   │
│  │  Running → ShuttingDown → CancellingTasks → ForceExit    │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                  Shutdown Pipeline                        │   │
│  │  Phase 1: CancelTasks    (timeout: 2000ms)               │   │
│  │  Phase 2: ReleaseResources (timeout: 1000ms)             │   │
│  │  Phase 3: ForceTerminate  (timeout: 500ms)               │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   Watchdog System                         │   │
│  │  - WindowHandleWatchdog (interval: 300ms)                │   │
│  │  - ProcessHealthWatchdog (interval: 1000ms)              │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 实施步骤

### 阶段一：创建 LifecycleSupervisor 核心框架

#### 1.1 创建 LifecycleSupervisor 类

**文件：** `include/EnhanceVision/core/LifecycleSupervisor.h`

```cpp
// 状态枚举
enum class LifecycleState {
    Running,           // 正常运行
    ShutdownRequested, // 关闭请求
    CancellingTasks,   // 取消任务中
    ReleasingResources,// 释放资源中
    ForceExit,         // 强制退出
    Terminated         // 已终止
};

// 退出原因枚举
enum class ExitReason {
    Normal,
    MainWindowClosed,
    MainWindowDestroyed,
    MainWindowHandleLost,
    MainWindowHidden,
    CrashDetected,
    WatchdogTimeout,
    UserRequest
};

// 关闭阶段
struct ShutdownPhase {
    QString name;
    int timeoutMs;
    std::function<bool()> execute;
    bool completed = false;
};
```

#### 1.2 实现状态机和关闭管道

**文件：** `src/core/LifecycleSupervisor.cpp`

核心功能：
- `requestShutdown(ExitReason reason)` - 请求关闭
- `executeShutdownPipeline()` - 执行关闭管道
- `transitionTo(LifecycleState newState)` - 状态转换
- `forceTerminate()` - 强制终止进程

### 阶段二：增强后台任务取消机制

#### 2.1 创建 CancellableTaskToken

**文件：** `include/EnhanceVision/core/CancellableTaskToken.h`

```cpp
class CancellableTaskToken : public QObject {
    Q_OBJECT
public:
    bool isCancelled() const;
    void cancel();
    void checkAndThrow(); // 如果已取消，抛出异常
    
signals:
    void cancelled();
};
```

#### 2.2 修改 AIEngine 支持可取消任务

- 在 `processVideoInternal` 中定期检查取消状态
- 使用 `QThreadPool::clear()` 清空待执行任务
- 添加任务中断点

### 阶段三：改进 Watchdog 系统

#### 3.1 缩短检查间隔

- 窗口句柄检查：300ms
- 进程健康检查：1000ms

#### 3.2 添加 Windows 异常处理器

```cpp
// 在 main.cpp 中添加 SEH 异常处理器
LONG WINAPI SehExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);
```

### 阶段四：重构 Application 类

#### 4.1 集成 LifecycleSupervisor

- 将生命周期逻辑委托给 `LifecycleSupervisor`
- 简化 `Application` 类职责

#### 4.2 改进关闭流程

```cpp
void Application::setupLifecycleGuard() {
    // 使用 LifecycleSupervisor 替代原有逻辑
    m_lifecycleSupervisor = new LifecycleSupervisor(this);
    m_lifecycleSupervisor->setupWindowWatchdog(m_mainWidget);
    m_lifecycleSupervisor->setupProcessWatchdog();
}
```

### 阶段五：添加调试和诊断功能

#### 5.1 增强日志输出

- 每个关闭阶段开始/结束日志
- 状态转换日志
- 任务取消详情日志

#### 5.2 添加崩溃诊断

- 记录崩溃时的调用栈
- 保存崩溃现场信息

---

## 详细实现计划

### Step 1: 创建 LifecycleSupervisor 头文件

**路径：** `include/EnhanceVision/core/LifecycleSupervisor.h`

包含：
- 状态枚举和退出原因枚举
- ShutdownPhase 结构体
- LifecycleSupervisor 类声明

### Step 2: 实现 LifecycleSupervisor

**路径：** `src/core/LifecycleSupervisor.cpp`

实现：
- 构造函数/析构函数
- 状态机逻辑
- 关闭管道执行
- Watchdog 设置
- 强制终止逻辑

### Step 3: 创建 CancellableTaskToken

**路径：** `include/EnhanceVision/core/CancellableTaskToken.h` 和 `src/core/CancellableTaskToken.cpp`

### Step 4: 修改 AIEngine

**文件：** `src/core/AIEngine.cpp`

修改：
- `processVideoInternal` - 添加取消检查点
- `processFrame` - 添加取消检查
- 添加 `forceCancel()` 方法

### Step 5: 修改 ProcessingController

**文件：** `src/controllers/ProcessingController.cpp`

修改：
- `cancelAllTasks` - 使用强制取消
- 添加 `forceCancelAllTasks()` 方法

### Step 6: 重构 Application

**文件：** `src/app/Application.cpp` 和 `include/EnhanceVision/app/Application.h`

修改：
- 集成 LifecycleSupervisor
- 简化 setupLifecycleGuard
- 移除重复的生命周期逻辑

### Step 7: 修改 main.cpp

**文件：** `src/main.cpp`

添加：
- Windows SEH 异常处理器
- 进程强制终止兜底

### Step 8: 更新 CMakeLists.txt

添加新文件到构建系统

---

## 关键代码变更点

### 1. LifecycleSupervisor 核心逻辑

```cpp
void LifecycleSupervisor::executeShutdownPipeline() {
    qInfo() << "[Lifecycle] Starting shutdown pipeline, reason:" << exitReasonToString(m_exitReason);
    
    // Phase 1: 取消所有任务
    if (!executePhase("CancelTasks", 2000, [this]() {
        emit cancelAllTasksRequested();
        if (m_processingController) {
            m_processingController->forceCancelAllTasks();
        }
        return true;
    })) {
        qWarning() << "[Lifecycle] CancelTasks phase timeout";
    }
    
    // Phase 2: 释放资源
    if (!executePhase("ReleaseResources", 1000, [this]() {
        emit releaseResourcesRequested();
        return true;
    })) {
        qWarning() << "[Lifecycle] ReleaseResources phase timeout";
    }
    
    // Phase 3: 强制退出
    forceTerminate();
}

void LifecycleSupervisor::forceTerminate() {
    qCritical() << "[Lifecycle] Force terminating process";
    
    // 记录最终状态
    qInfo() << "[Lifecycle] Final state:" << stateToString(m_state);
    qInfo() << "[Lifecycle] Exit reason:" << exitReasonToString(m_exitReason);
    
    // 强制终止
#ifdef Q_OS_WIN
    TerminateProcess(GetCurrentProcess(), 1);
#else
    std::_Exit(1);
#endif
}
```

### 2. AIEngine 取消检查点

```cpp
// 在 processVideoInternal 的关键循环中添加
while (av_read_frame(inFmtCtx, pkt) >= 0) {
    // 检查取消状态
    if (m_cancelRequested || m_forceCancelRequested) {
        av_packet_unref(pkt);
        qInfo() << "[AIEngine][Video] Cancelled at frame" << frameCount;
        fail(tr("视频处理已取消"));
        return;
    }
    
    // ... 原有逻辑
}
```

### 3. Windows 异常处理器

```cpp
// main.cpp
LONG WINAPI SehExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    qCritical() << "[Crash] SEH Exception detected:"
                << "code:" << QString("0x%1").arg(pExceptionInfo->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'));
    
    // 强制终止进程
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[]) {
    // 设置 SEH 异常处理器
    SetUnhandledExceptionFilter(SehExceptionHandler);
    
    // ... 原有代码
}
```

---

## 测试验证

### 测试场景

1. **正常关闭测试**
   - 点击关闭按钮
   - 验证进程完全退出

2. **视频处理中关闭测试**
   - 开始视频处理
   - 在处理过程中关闭窗口
   - 验证后台任务被取消，进程退出

3. **崩溃模拟测试**
   - 模拟窗口崩溃
   - 验证进程被强制终止

4. **多进程残留测试**
   - 验证没有残留的 EnhanceVision.exe 进程

---

## 风险评估

### 低风险
- LifecycleSupervisor 是新增模块，不影响现有功能
- 状态机和关闭管道是独立逻辑

### 中等风险
- AIEngine 取消逻辑修改需要仔细测试
- Windows 异常处理器可能影响调试

### 缓解措施
- 分阶段实施，每阶段独立测试
- 保留原有日志便于问题定位
- 添加详细的状态日志

---

## 文件清单

### 新增文件
1. `include/EnhanceVision/core/LifecycleSupervisor.h`
2. `src/core/LifecycleSupervisor.cpp`
3. `include/EnhanceVision/core/CancellableTaskToken.h`
4. `src/core/CancellableTaskToken.cpp`

### 修改文件
1. `src/app/Application.cpp`
2. `include/EnhanceVision/app/Application.h`
3. `src/core/AIEngine.cpp`
4. `include/EnhanceVision/core/AIEngine.h`
5. `src/controllers/ProcessingController.cpp`
6. `include/EnhanceVision/controllers/ProcessingController.h`
7. `src/main.cpp`
8. `CMakeLists.txt`
