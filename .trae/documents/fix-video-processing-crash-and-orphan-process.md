# 修复视频处理崩溃和孤儿进程问题

## 问题分析

### 现象
1. 使用 AI 推理模式处理视频时，应用程序窗口闪退（句柄变为 0）
2. 后台进程仍在继续运行，形成"孤儿进程"
3. Windows 事件日志显示崩溃异常码：`0xc0000005` (ACCESS_VIOLATION) 和 `0xc0000374` (堆损坏)
4. 崩溃发生在视频首帧处理路径（sws_scale / processFrame）

### 根本原因

#### 1. 窗口关闭时后台任务未终止
- `AIEngine::processAsync` 使用 `QtConcurrent::run` 启动后台线程处理视频
- 主窗口关闭/崩溃时，这些后台线程没有被正确终止
- 后台线程继续访问可能已释放的资源（GPU 设备、模型、内存）

#### 2. 缺少应用程序退出时的清理机制
- `Application` 类析构函数没有等待或取消正在进行的任务
- 没有连接 `QCoreApplication::aboutToQuit` 信号来执行清理

#### 3. 视频处理循环缺少应用程序状态检查
- `processVideoInternal` 只检查 `m_cancelRequested`
- 没有检查应用程序是否正在退出

#### 4. 崩溃导致资源状态不一致
- 当崩溃发生时，`m_inferenceMutex` 可能仍被持有
- 后台线程不知道主线程已崩溃，继续执行

## 解决方案

### 方案一：添加应用程序退出时的清理机制（推荐）

#### 1.1 在 Application 类中添加退出清理
```cpp
// Application.h
private slots:
    void onAboutToQuit();

// Application.cpp
void Application::initialize()
{
    // ... 现有代码 ...
    
    // 连接应用程序退出信号
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &Application::onAboutToQuit);
}

void Application::onAboutToQuit()
{
    qInfo() << "[Application] aboutToQuit signal received, cleaning up...";
    
    // 取消所有正在进行的处理任务
    if (m_processingController) {
        m_processingController->cancelAllTasks();
        m_processingController->waitForCompletion(5000); // 等待最多5秒
    }
    
    // 取消 AI 引擎的推理
    if (m_aiEngine) {
        m_aiEngine->cancelProcess();
        m_aiEngine->waitForCompletion(5000);
    }
}
```

#### 1.2 在 ProcessingController 中添加等待完成方法
```cpp
// ProcessingController.h
void waitForCompletion(int timeoutMs = -1);

// ProcessingController.cpp
void ProcessingController::waitForCompletion(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    while (m_currentProcessingCount > 0) {
        if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
            qWarning() << "[ProcessingController] waitForCompletion timeout, forcing cancel";
            cancelAllTasks();
            break;
        }
        QThread::msleep(50);
        QCoreApplication::processEvents();
    }
    
    m_threadPool->waitForDone(timeoutMs > 0 ? timeoutMs - timer.elapsed() : -1);
}
```

#### 1.3 在 AIEngine 中添加等待完成方法
```cpp
// AIEngine.h
void waitForCompletion(int timeoutMs = -1);

// AIEngine.cpp
void AIEngine::waitForCompletion(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    
    while (m_isProcessing.load()) {
        if (timeoutMs > 0 && timer.elapsed() >= timeoutMs) {
            qWarning() << "[AIEngine] waitForCompletion timeout, forcing cancel";
            cancelProcess();
            break;
        }
        QThread::msleep(50);
    }
}
```

### 方案二：在视频处理循环中添加应用程序状态检查

#### 2.1 添加全局退出标志
```cpp
// AIEngine.h
private:
    static std::atomic<bool> s_appExiting;
public:
    static void setAppExiting(bool exiting) { s_appExiting.store(exiting); }
    static bool isAppExiting() { return s_appExiting.load(); }
```

#### 2.2 在视频处理循环中检查
```cpp
// AIEngine.cpp - processVideoInternal
while (av_read_frame(inFmtCtx, pkt) >= 0) {
    // 添加应用程序退出检查
    if (m_cancelRequested || s_appExiting.load()) {
        av_packet_unref(pkt);
        fail(tr("视频处理已取消或应用程序正在退出"));
        return;
    }
    // ... 现有代码 ...
}
```

#### 2.3 在 main.cpp 中设置退出标志
```cpp
// main.cpp
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // ... 现有代码 ...
}

int main(int argc, char *argv[])
{
    // ... 现有代码 ...
    
    // 在退出前设置标志
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        AIEngine::setAppExiting(true);
    });
    
    int result = app.exec();
    // ... 现有代码 ...
}
```

### 方案三：添加 Windows 异常处理和进程清理

#### 3.1 添加 SEH 异常处理
```cpp
// main.cpp
#include <windows.h>
#include <eh.h>

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    qCritical() << "[Crash] Unhandled exception:" 
                << QString("0x%1").arg(exceptionInfo->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'));
    
    // 强制终止所有线程和进程
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char *argv[])
{
    // 设置异常处理器
    SetUnhandledExceptionFilter(ExceptionHandler);
    
    // ... 现有代码 ...
}
```

#### 3.2 使用 QSharedMemory 确保单实例并监控进程状态
```cpp
// Application.cpp
void Application::initialize()
{
    // ... 现有代码 ...
    
    // 创建进程监控
    m_processMonitor = new ProcessMonitor(this);
    m_processMonitor->start();
}
```

### 方案四：改进视频处理崩溃点

#### 4.1 添加 sws_scale 错误处理
```cpp
// AIEngine.cpp - processVideoInternal
// 在 sws_scale 调用前后添加保护
__try {
    int swsRet = sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
                           0, decFrame->height, dst, dstStride);
    if (swsRet < 0) {
        qWarning() << "[AIEngine][Video] frame" << frameCount 
                   << "sws_scale failed with ret:" << swsRet;
        failedFrames++;
        continue;
    }
} __except(EXCEPTION_EXECUTE_HANDLER) {
    qCritical() << "[AIEngine][Video] frame" << frameCount 
                << "sws_scale caused exception, skipping";
    failedFrames++;
    continue;
}
```

## 实施计划

### 第一阶段：核心修复（必须实施）
1. **在 Application 类中添加 `aboutToQuit` 信号处理**
   - 文件：`src/app/Application.h`, `src/app/Application.cpp`
   - 内容：添加退出清理逻辑

2. **在 ProcessingController 中添加 `waitForCompletion` 方法**
   - 文件：`src/controllers/ProcessingController.h`, `src/controllers/ProcessingController.cpp`
   - 内容：添加等待任务完成的方法

3. **在 AIEngine 中添加 `waitForCompletion` 方法**
   - 文件：`include/EnhanceVision/core/AIEngine.h`, `src/core/AIEngine.cpp`
   - 内容：添加等待推理完成的方法

### 第二阶段：增强保护（推荐实施）
4. **添加全局退出标志**
   - 文件：`include/EnhanceVision/core/AIEngine.h`, `src/core/AIEngine.cpp`
   - 内容：添加静态退出标志，在视频处理循环中检查

5. **在 main.cpp 中设置退出标志**
   - 文件：`src/main.cpp`
   - 内容：连接 aboutToQuit 信号设置退出标志

### 第三阶段：异常处理（可选实施）
6. **添加 Windows SEH 异常处理**
   - 文件：`src/main.cpp`
   - 内容：添加异常处理器，崩溃时强制终止进程

## 测试验证

### 测试场景
1. 正常处理视频后关闭窗口 → 应正常退出
2. 处理视频过程中关闭窗口 → 应取消任务并退出
3. 处理视频过程中发生崩溃 → 应终止进程，不留孤儿进程
4. 多个视频任务同时进行时关闭窗口 → 应全部取消并退出

### 验证方法
1. 使用任务管理器检查进程是否正确终止
2. 检查日志文件确认清理流程执行
3. 检查 Windows 事件查看器确认无新的崩溃记录

## 风险评估

### 低风险
- 添加 `aboutToQuit` 信号处理：标准 Qt 模式，影响范围可控
- 添加等待完成方法：纯新增功能，不影响现有逻辑

### 中等风险
- 添加全局退出标志：需要确保线程安全，使用 `std::atomic`
- 修改视频处理循环：需要仔细测试，避免引入新的竞态条件

### 需要注意
- SEH 异常处理可能与某些调试器冲突
- 等待超时设置需要根据实际任务特性调整

## 文件修改清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `src/app/Application.h` | 修改 | 添加 `onAboutToQuit` 槽函数声明 |
| `src/app/Application.cpp` | 修改 | 实现退出清理逻辑 |
| `include/EnhanceVision/core/AIEngine.h` | 修改 | 添加 `waitForCompletion` 和静态退出标志 |
| `src/core/AIEngine.cpp` | 修改 | 实现等待完成方法，视频循环检查退出标志 |
| `src/controllers/ProcessingController.h` | 修改 | 添加 `waitForCompletion` 方法声明 |
| `src/controllers/ProcessingController.cpp` | 修改 | 实现等待完成方法 |
| `src/main.cpp` | 修改 | 添加退出标志设置和可选的异常处理 |
