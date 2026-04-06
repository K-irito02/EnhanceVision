# 重构计划：将剩余时间倒计时从 QML 移至 C++ 后端

## 问题描述

当用户从其他会话标签切换回来时，消息卡片上的"剩余时间"会从0开始变化到最终剩余时间。这是因为 QML 中的 Timer 和动画逻辑在 delegate 被销毁后停止运行。

## 解决方案

将倒计时逻辑完全移至 C++ 后端，使用 `ProcessingTimeManager` 类在后台管理所有处理中任务的倒计时。QML 只负责显示，不再有任何计时器或动画逻辑。

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    C++ Backend                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              ProcessingTimeManager                   │   │
│  │  - 管理所有处理中任务的倒计时                          │   │
│  │  - QTimer 每秒更新一次                                │   │
│  │  - 后台持续运行，不受 QML delegate 生命周期影响        │   │
│  └─────────────────────────────────────────────────────┘   │
│            │                           │                     │
│            ▼                           ▼                     │
│  ┌─────────────────┐         ┌─────────────────┐           │
│  │  MessageModel   │         │ SessionController│           │
│  │  - remainingSec │         │ - 会话切换时保持  │           │
│  │  - elapsedSec   │         │   TimeManager    │           │
│  │  - isOvertime   │         │   继续运行       │           │
│  └─────────────────┘         └─────────────────┘           │
└─────────────────────────────────────────────────────────────┘
            │
            ▼ (信号/属性绑定)
┌─────────────────────────────────────────────────────────────┐
│                    QML Frontend                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                  MessageItem.qml                     │   │
│  │  - 只负责显示 remainingSec / elapsedSec              │   │
│  │  - 移除所有 Timer 和动画逻辑                          │   │
│  │  - 直接显示 C++ 传入的值                              │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 实施步骤

### Step 1：创建 ProcessingTimeManager 类

**新建文件**：
- `include/EnhanceVision/core/ProcessingTimeManager.h`
- `src/core/ProcessingTimeManager.cpp`

**职责**：
- 管理所有处理中任务的倒计时
- 提供 `registerTask(messageId, predictedTotalSec)` 方法
- 提供 `unregisterTask(messageId)` 方法
- 每秒更新所有任务的 `elapsedSec` 和 `remainingSec`
- 发出信号通知 QML 更新

### Step 2：扩展 Message 数据结构

**修改文件**：`include/EnhanceVision/models/DataTypes.h`

**新增字段**：
```cpp
struct Message {
    // ... 现有字段 ...
    qint64 predictedTotalSec;    // 预测总时间（秒）
    qint64 elapsedSec;           // 已用时间（秒），由 TimeManager 更新
    qint64 remainingSec;         // 剩余时间（秒），由 TimeManager 更新
    bool isOvertime;             // 是否超时
};
```

### Step 3：扩展 MessageModel

**修改文件**：
- `include/EnhanceVision/models/MessageModel.h`
- `src/models/MessageModel.cpp`

**新增角色**：
- `PredictedTotalSecRole` - 预测总时间
- `ElapsedSecRole` - 已用时间
- `RemainingSecRole` - 剩余时间
- `IsOvertimeRole` - 是否超时

**新增方法**：
- `updateTimeInfo(messageId, elapsedSec, remainingSec, isOvertime)` - 由 TimeManager 调用

### Step 4：集成 ProcessingTimeManager

**修改文件**：
- `include/EnhanceVision/controllers/SessionController.h`
- `src/controllers/SessionController.cpp`

**集成点**：
- 在任务开始处理时调用 `timeManager->registerTask()`
- 在任务完成/取消/失败时调用 `timeManager->unregisterTask()`
- 会话切换时保持 TimeManager 继续运行

### Step 5：简化 MessageItem.qml

**修改文件**：`qml/components/MessageItem.qml`

**移除**：
- `elapsedTimer` Timer 组件
- `_displayRemainingSec` 及其 Behavior 动画
- `_skipRemainingTimeAnimation` 标志位
- `_isRestoring` 标志位
- `_needsRestore` 标志位
- 所有恢复逻辑 (`onPersistedProcessingStartTimeChanged` 等)

**保留**：
- 直接从模型读取 `remainingSec`、`elapsedSec`、`isOvertime`
- 简单的显示逻辑

### Step 6：更新 MessageList.qml

**修改文件**：`qml/components/MessageList.qml`

**变更**：
- 将新的角色绑定到 delegate 属性
- 移除 `persistedProcessingStartTime` 相关逻辑

## 详细实现

### ProcessingTimeManager.h

```cpp
#ifndef ENHANCEVISION_PROCESSINGTIMEMANAGER_H
#define ENHANCEVISION_PROCESSINGTIMEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QDateTime>

namespace EnhanceVision {

class MessageModel;

struct TaskTimeInfo {
    QString messageId;
    qint64 processingStartTime;  // 处理开始时间戳（毫秒）
    qint64 predictedTotalSec;    // 预测总时间（秒）
    qint64 elapsedSec;           // 已用时间（秒）
    qint64 remainingSec;         // 剩余时间（秒）
    bool isOvertime;             // 是否超时
};

class ProcessingTimeManager : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingTimeManager(QObject *parent = nullptr);
    ~ProcessingTimeManager() override;

    void setMessageModel(MessageModel* model);

    // 注册任务开始处理
    void registerTask(const QString& messageId, 
                      qint64 predictedTotalSec,
                      qint64 processingStartTime = 0);

    // 注销任务（完成/取消/失败）
    void unregisterTask(const QString& messageId);

    // 获取任务时间信息
    TaskTimeInfo getTaskTimeInfo(const QString& messageId) const;

    // 检查任务是否已注册
    bool isTaskRegistered(const QString& messageId) const;

signals:
    // 时间信息更新信号（每秒发出）
    void timeInfoUpdated(const QString& messageId,
                         qint64 elapsedSec,
                         qint64 remainingSec,
                         bool isOvertime);

private slots:
    void onTimerTick();

private:
    MessageModel* m_messageModel = nullptr;
    QTimer* m_updateTimer = nullptr;
    QHash<QString, TaskTimeInfo> m_tasks;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGTIMEMANAGER_H
```

### ProcessingTimeManager.cpp

```cpp
#include "EnhanceVision/core/ProcessingTimeManager.h"
#include "EnhanceVision/models/MessageModel.h"

namespace EnhanceVision {

ProcessingTimeManager::ProcessingTimeManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
{
    connect(m_updateTimer, &QTimer::timeout, 
            this, &ProcessingTimeManager::onTimerTick);
    m_updateTimer->start(1000);  // 每秒更新
}

ProcessingTimeManager::~ProcessingTimeManager()
{
    m_updateTimer->stop();
}

void ProcessingTimeManager::setMessageModel(MessageModel* model)
{
    m_messageModel = model;
}

void ProcessingTimeManager::registerTask(const QString& messageId, 
                                          qint64 predictedTotalSec,
                                          qint64 processingStartTime)
{
    TaskTimeInfo info;
    info.messageId = messageId;
    info.predictedTotalSec = predictedTotalSec;
    info.processingStartTime = (processingStartTime > 0) 
        ? processingStartTime 
        : QDateTime::currentMSecsSinceEpoch();
    info.elapsedSec = 0;
    info.remainingSec = predictedTotalSec;
    info.isOvertime = false;

    m_tasks.insert(messageId, info);
}

void ProcessingTimeManager::unregisterTask(const QString& messageId)
{
    m_tasks.remove(messageId);
}

TaskTimeInfo ProcessingTimeManager::getTaskTimeInfo(const QString& messageId) const
{
    return m_tasks.value(messageId);
}

bool ProcessingTimeManager::isTaskRegistered(const QString& messageId) const
{
    return m_tasks.contains(messageId);
}

void ProcessingTimeManager::onTimerTick()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        TaskTimeInfo& info = it.value();
        
        // 计算已用时间
        info.elapsedSec = (now - info.processingStartTime) / 1000;
        
        // 计算剩余时间（允许负数表示超时）
        info.remainingSec = info.predictedTotalSec - info.elapsedSec;
        
        // 判断是否超时
        info.isOvertime = info.remainingSec < 0;

        // 发出更新信号
        emit timeInfoUpdated(info.messageId, 
                             info.elapsedSec, 
                             info.remainingSec, 
                             info.isOvertime);

        // 更新 MessageModel
        if (m_messageModel) {
            m_messageModel->updateTimeInfo(info.messageId, 
                                           info.elapsedSec, 
                                           info.remainingSec, 
                                           info.isOvertime);
        }
    }
}

} // namespace EnhanceVision
```

### MessageItem.qml 简化后

```qml
Rectangle {
    id: root
    
    // ... 其他属性 ...
    
    // 直接从模型读取时间信息（由 C++ 后端更新）
    property real predictedTotalSec: 0
    property real elapsedSec: 0
    property real remainingSec: 0
    property bool isOvertime: false
    
    // 基于时间的进度百分比
    readonly property real _timeBasedProgress: {
        if (predictedTotalSec <= 0) return 0
        if (root.allFilesSettled) return 100
        var progressPct = (elapsedSec / predictedTotalSec) * 100
        return Math.min(99, Math.max(0, progressPct))
    }
    
    // 显示剩余时间（直接使用，无需动画）
    function formatTime(sec) {
        var absSec = Math.abs(sec)
        var m = Math.floor(absSec / 60)
        var s = Math.floor(absSec % 60)
        return (sec < 0 ? "-" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }
    
    // 移除所有 Timer 和动画逻辑
    // ...
}
```

## 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/EnhanceVision/core/ProcessingTimeManager.h` | 新建 | 时间管理器头文件 |
| `src/core/ProcessingTimeManager.cpp` | 新建 | 时间管理器实现 |
| `include/EnhanceVision/models/DataTypes.h` | 修改 | Message 结构体新增字段 |
| `include/EnhanceVision/models/MessageModel.h` | 修改 | 新增角色和方法 |
| `src/models/MessageModel.cpp` | 修改 | 实现新角色和方法 |
| `include/EnhanceVision/controllers/SessionController.h` | 修改 | 集成 TimeManager |
| `src/controllers/SessionController.cpp` | 修改 | 集成 TimeManager |
| `qml/components/MessageItem.qml` | 修改 | 移除 Timer 和动画，简化显示 |
| `qml/components/MessageList.qml` | 修改 | 绑定新角色 |
| `CMakeLists.txt` | 修改 | 添加新源文件 |

## 预期效果

1. **后台静默倒计时**：切换会话后，C++ 中的 Timer 继续运行
2. **直接显示正确值**：切换回来时，QML 直接显示 C++ 传入的最新值
3. **无动画跳变**：移除了 QML 中的动画逻辑，显示更直接
4. **架构更清晰**：业务逻辑在 C++，QML 只负责显示

## 风险评估

- **中等风险**：涉及架构变更，需要仔细测试
- **兼容性**：需要确保会话切换时数据正确恢复
- **测试点**：
  1. 正常处理时倒计时是否正确
  2. 切换会话后返回是否显示正确时间
  3. 超时情况下是否正确显示负数
  4. 任务完成/取消后是否停止计时
