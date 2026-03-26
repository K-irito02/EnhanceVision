# EnhanceVision 并发架构设计文档

## 概述

本文档描述 EnhanceVision 项目的并发处理架构，包括任务调度、优先级管理、超时检测、重试策略、死锁检测和 AI 推理并发等核心组件。

## 架构组件

### 1. 核心组件

#### 1.1 ConcurrencyManager
统一门面，整合所有并发子组件：
- `PriorityTaskQueue` - 优先级任务队列
- `TaskTimeoutWatchdog` - 超时检测
- `TaskRetryPolicy` - 重试策略
- `PriorityAdjuster` - 优先级调整器
- `DeadlockDetector` - 死锁检测器

**职责**：
- 管理会话/消息级并发槽位
- 提供统一的任务调度接口
- 转发子组件信号

#### 1.2 PriorityTaskQueue
多级优先级队列，支持 O(1) 入队/出队：
- `UserInteractive` - 用户交互级（最高）
- `UserInitiated` - 用户发起级
- `Utility` - 工具级
- `Background` - 后台级（最低）

**特性**：
- 同优先级内 FIFO 顺序
- 支持动态优先级更新
- 会话切换时批量提升/降级

#### 1.3 TaskTimeoutWatchdog
任务超时和停滞检测：
- 默认超时：300 秒
- 停滞警告阈值：30 秒
- 检查间隔：5 秒

**信号**：
- `taskTimedOut(taskId)` - 任务超时
- `taskStalled(taskId, stallMs)` - 任务停滞

#### 1.4 TaskRetryPolicy
指数退避重试策略：
- 默认最大重试次数：3
- 基础延迟：1000ms
- 退避因子：2.0

**信号**：
- `retryScheduled(taskId, delayMs, attempt)` - 重试调度
- `retriesExhausted(taskId, totalAttempts, lastError)` - 重试耗尽

#### 1.5 PriorityAdjuster
饥饿防护和会话优先级调整：
- 饥饿阈值：30 秒
- 检查间隔：10 秒

**功能**：
- 自动提升长时间等待的任务优先级
- 会话切换时调整任务优先级

#### 1.6 DeadlockDetector
基于等待图的死锁检测：
- 检查间隔：10 秒
- 使用 DFS 检测循环依赖

**恢复策略**：
- `CancelYoungest` - 取消最新任务（默认）
- `CancelLowestPriority` - 取消最低优先级任务
- `CancelAll` - 取消所有参与者

### 2. AI 推理并发

#### 2.1 AIEnginePool
AI 推理引擎池，支持多任务并行推理：
- 默认池大小：2（可配置）
- 动态信号连接/断开

**接口**：
- `acquire(taskId)` - 获取引擎
- `release(taskId)` - 释放引擎
- `availableCount()` - 可用引擎数

### 3. 监控组件

#### 3.1 ConcurrencyMonitor
实时统计和异常检测：
- 快照间隔：5 秒
- 保留最近 120 个快照

**指标**：
- 待处理/活跃任务数
- 活跃会话数
- 内存/GPU 使用情况
- 吞吐量统计

**异常检测**：
- 资源压力过高
- 死锁等待边存在
- 队列积压
- 调度停滞

## 数据流

```
用户请求
    ↓
ProcessingController.addTask()
    ↓
ConcurrencyManager.enqueueTask()
    ↓
PriorityTaskQueue (按优先级排序)
    ↓
processNextTask() 调度循环
    ↓
ConcurrencyManager.scheduleNext()
    ↓
├─ Shader 任务 → ThreadPool
└─ AI 任务 → AIEnginePool.acquire() → AIEngine.processAsync()
    ↓
任务完成/失败
    ↓
ConcurrencyManager.releaseSlot()
    ↓
processNextTask() (继续调度)
```

## 并发限制

| 维度 | 默认值 | 配置项 |
|------|--------|--------|
| 最大并发任务 | 4 | `maxConcurrentTasks` |
| 最大并发会话 | 2 | `maxConcurrentSessions` |
| 每消息最大并发 | 2 | `maxConcurrentFilesPerMessage` |
| AI 引擎池大小 | 2 | 代码配置 |

## 文件清单

### 头文件 (`include/EnhanceVision/core/`)
- `PriorityTaskQueue.h`
- `PriorityAdjuster.h`
- `DeadlockDetector.h`
- `TaskTimeoutWatchdog.h`
- `TaskRetryPolicy.h`
- `ConcurrencyManager.h`
- `ConcurrencyMonitor.h`
- `AIEnginePool.h`

### 源文件 (`src/core/`)
- `PriorityTaskQueue.cpp`
- `PriorityAdjuster.cpp`
- `DeadlockDetector.cpp`
- `TaskTimeoutWatchdog.cpp`
- `TaskRetryPolicy.cpp`
- `ConcurrencyManager.cpp`
- `ConcurrencyMonitor.cpp`
- `AIEnginePool.cpp`

### 测试文件 (`tests/cpp/`)
- `test_concurrency.cpp`

## 变更历史

| 日期 | 版本 | 变更内容 |
|------|------|----------|
| 2026-03-27 | 1.0 | 初始并发架构实现 |
