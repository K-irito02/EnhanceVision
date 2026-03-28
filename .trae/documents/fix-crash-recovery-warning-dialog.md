# 修复"自动重新处理"机制中警告弹窗提示功能异常问题

## 问题分析

### 当前实现流程

1. **应用启动时**：`SettingsController::markAppRunning()` 将 `lastExitClean` 设为 `false`
2. **应用正常退出时**：`Application::~Application()` 调用 `markAppExiting()` 将 `lastExitClean` 设为 `true`
3. **检测异常关闭**：`checkAndHandleCrashRecovery()` 检查 `lastExitClean` 是否为 `false`

### 问题根源

**时序问题**：`markAppExiting()` 在 `Application::~Application()` 中调用，但存在以下问题：

1. **强制终止计时器**：`LifecycleSupervisor::executeShutdownPipeline()` 设置 500ms 后强制终止进程
2. **析构函数可能未执行**：如果事件循环卡住或超时，`forceTerminate()` 直接调用 `TerminateProcess()`，跳过所有析构函数
3. **结果**：正常关闭时 `markAppExiting()` 可能未被调用，导致下次启动误判为异常关闭

### 异常关闭定义

根据需求，以下情况应被识别为异常关闭：
- 应用程序崩溃（Crash）
- 应用程序闪退
- 进程仍在运行但窗口句柄为0
- 其他未正常退出的异常场景

## 解决方案

### 核心思路

**将 `markAppExiting()` 调用提前到关闭流程开始时**，而不是依赖析构函数。

### 设计原则

1. **尽早标记**：在确认是正常关闭意图时立即标记
2. **区分原因**：根据 `ExitReason` 区分正常关闭和异常关闭
3. **健壮性保证**：考虑各种边界情况

### 实现步骤

#### 步骤1：修改 `SettingsController` - 增加退出原因记录

**文件**：`include/EnhanceVision/controllers/SettingsController.h` 和 `src/controllers/SettingsController.cpp`

**修改内容**：
1. 添加 `markNormalExit()` 方法：立即标记正常退出
2. 添加 `m_exitReason` 成员变量记录退出原因
3. 修改 `checkAndHandleCrashRecovery()` 逻辑，增加对退出原因的判断

```cpp
// SettingsController.h 新增
Q_PROPERTY(QString lastExitReason READ lastExitReason NOTIFY lastExitReasonChanged)

void markNormalExit(const QString& reason = QStringLiteral("normal"));
QString lastExitReason() const;

signals:
    void lastExitReasonChanged();

private:
    QString m_lastExitReason;
```

```cpp
// SettingsController.cpp 新增
void SettingsController::markNormalExit(const QString& reason)
{
    m_lastExitClean = true;
    m_lastExitReason = reason;
    m_settings->setValue("system/lastExitClean", true);
    m_settings->setValue("system/lastExitReason", reason);
    m_settings->sync();
}
```

#### 步骤2：修改 `LifecycleSupervisor` - 在关闭流程开始时标记正常退出

**文件**：`src/core/LifecycleSupervisor.cpp`

**修改内容**：
在 `requestShutdown()` 中，根据 `ExitReason` 判断是否为正常关闭，如果是则立即调用 `markNormalExit()`

```cpp
void LifecycleSupervisor::requestShutdown(ExitReason reason, const QString& detail)
{
    // ... 现有代码 ...
    
    // 判断是否为正常关闭
    bool isNormalExit = (reason == ExitReason::Normal ||
                         reason == ExitReason::MainWindowClosed ||
                         reason == ExitReason::UserRequest);
    
    if (isNormalExit) {
        // 立即标记正常退出，不依赖析构函数
        SettingsController::instance()->markNormalExit(exitReasonToString(reason));
    }
    
    // ... 后续关闭流程 ...
}
```

#### 步骤3：修改 `Application` - 移除析构函数中的标记

**文件**：`src/app/Application.cpp`

**修改内容**：
移除 `Application::~Application()` 中的 `markAppExiting()` 调用，因为已经在 `LifecycleSupervisor::requestShutdown()` 中处理

```cpp
Application::~Application()
{
    qInfo() << "[Application] Destructor called";
    
    // 移除：SettingsController::instance()->markAppExiting();
    // 因为已经在 LifecycleSupervisor::requestShutdown() 中处理
    
    m_sessionController->saveSessions();
    delete m_mainWidget;
    SettingsController::destroyInstance();
    
    qInfo() << "[Application] Destructor completed";
}
```

#### 步骤4：增强异常关闭检测 - 添加更多检测维度

**文件**：`src/controllers/SettingsController.cpp`

**修改内容**：
增强 `checkAndHandleCrashRecovery()` 的检测逻辑

```cpp
bool SettingsController::checkAndHandleCrashRecovery()
{
    bool wasClean = m_settings->value("system/lastExitClean", true).toBool();
    QString lastReason = m_settings->value("system/lastExitReason", "").toString();
    
    // 如果标记为正常退出，则不显示警告
    if (wasClean) {
        return false;
    }
    
    // 如果有退出原因记录，说明是正常关闭但标记未更新（边界情况）
    // 这种情况下也不显示警告
    if (!lastReason.isEmpty() && 
        (lastReason == "normal" || 
         lastReason == "main_window_closed" ||
         lastReason == "user_request")) {
        qInfo() << "[SettingsController] Exit reason recorded but lastExitClean not set, treating as normal exit";
        return false;
    }
    
    // 确认为异常关闭
    m_crashDetectedOnStartup = true;
    m_autoReprocessShaderEnabled = false;
    m_autoReprocessAIEnabled = false;
    
    m_settings->setValue("reprocess/shaderEnabled", false);
    m_settings->setValue("reprocess/aiEnabled", false);
    m_settings->sync();
    
    emit autoReprocessShaderEnabledChanged();
    emit autoReprocessAIEnabledChanged();
    emit autoReprocessAllEnabledChanged();
    emit crashDetected();
    emit crashDetectedOnStartupChanged();
    
    return true;
}
```

#### 步骤5：处理 Shader/AI 处理中的关闭场景

**文件**：`src/controllers/ProcessingController.cpp` 和相关文件

**修改内容**：
确保在处理任务时关闭应用，能够正确标记为正常退出

1. 在 `LifecycleSupervisor::executeShutdownPipeline()` 的 `CancellingTasks` 阶段，确保任务取消完成后不会影响退出标记
2. 添加处理中任务的取消确认机制

#### 步骤6：添加启动时重置标记的逻辑

**文件**：`src/controllers/SettingsController.cpp`

**修改内容**：
在 `markAppRunning()` 中，确保正确设置初始状态

```cpp
void SettingsController::markAppRunning()
{
    // 清除上次退出原因，标记为运行中
    m_lastExitClean = false;
    m_lastExitReason.clear();
    m_settings->setValue("system/lastExitClean", false);
    m_settings->remove("system/lastExitReason");  // 清除退出原因
    m_settings->sync();
}
```

#### 步骤7：处理 SEH 异常处理器中的崩溃

**文件**：`src/main.cpp`

**修改内容**：
确保 SEH 异常处理器不会影响崩溃检测机制（当前实现直接 `TerminateProcess`，不会修改配置文件，这是正确的）

### 边界情况处理

1. **强制终止计时器触发**：如果在 500ms 内事件循环未退出，`forceTerminate()` 会终止进程，但由于已经在 `requestShutdown()` 中标记了正常退出，所以不会误判

2. **处理任务时关闭**：无论是否有任务处理中，只要是通过正常 UI 关闭，都应该标记为正常退出

3. **多实例运行**：每个实例有独立的配置文件，不会相互影响

4. **配置文件损坏**：默认值为 `true`，意味着如果配置文件损坏，不会显示警告

### 测试场景

1. **正常关闭（无任务）**：点击关闭按钮 → 不显示警告
2. **正常关闭（有任务处理中）**：处理中点击关闭 → 不显示警告
3. **崩溃关闭**：模拟崩溃 → 显示警告
4. **任务管理器强制结束**：显示警告
5. **断电/系统崩溃**：显示警告
6. **窗口句柄丢失**：当前实现会触发 `requestShutdown(ExitReason::MainWindowHandleLost)`，应显示警告

## 文件修改清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `include/EnhanceVision/controllers/SettingsController.h` | 修改 | 添加 `markNormalExit()`、`lastExitReason()` 等 |
| `src/controllers/SettingsController.cpp` | 修改 | 实现新增方法，增强检测逻辑 |
| `src/core/LifecycleSupervisor.cpp` | 修改 | 在 `requestShutdown()` 中调用 `markNormalExit()` |
| `src/app/Application.cpp` | 修改 | 移除析构函数中的 `markAppExiting()` 调用 |

## 风险评估

1. **低风险**：修改逻辑清晰，不影响现有功能
2. **向后兼容**：配置文件格式兼容
3. **回滚方案**：如有问题可快速回滚

## 实现优先级

1. **高优先级**：步骤1-4（核心修复）
2. **中优先级**：步骤5-6（增强健壮性）
3. **低优先级**：步骤7（验证现有实现）
