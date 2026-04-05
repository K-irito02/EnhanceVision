---
alwaysApply: false
globs: ['**/*.cpp', '**/*.h', '**/*.hpp']
description: 'C++代码规范 - 命名、接口设计、内存安全、线程安全'
trigger: glob
---
# C++ 代码规范

## 命名规范

| 类型 | 风格 | 示例 |
|------|------|------|
| 类/枚举 | PascalCase | `AIEngine` |
| 函数/变量 | camelCase | `processImage()` |
| 成员变量 | `m_`前缀 | `m_modelPath` |
| 常量 | `k`前缀 | `kMaxThreads` |
| 信号 | `xxxChanged` | `progressChanged()` |

## Qt 接口规范

1. `Q_PROPERTY` 必须带 `NOTIFY` 信号
2. `Q_INVOKABLE` 仅暴露轻量同步或异步入口
3. 阻塞操作不得直接暴露给 QML 调用

## 内存管理

1. **优先 RAII**
2. **QObject 明确父子关系**
3. **GPU 资源及时释放**

## 线程安全

1. **共享数据必须加锁**：`QMutexLocker` 保护队列、状态
2. **跨线程信号使用队列连接**：`Qt::QueuedConnection`
3. **原子操作使用 `std::atomic`**：状态标志、进度值
4. **推理前快照参数**：工作线程只持快照，不访问主线程对象

## 错误处理

1. **错误必须可观测**：返回值/信号/日志
2. **禁止吞错**：所有异常路径必须有处理
3. **GPU OOM 自动恢复**：清理内存 → 重试 → 降级分块
4. **进度只前进不后退**：避免 UI 倒退体验

## 日志规范

1. **关键路径必须有日志**：初始化、任务开始/结束、错误
2. **日志格式统一**：`[ClassName][DEBUG] message`
3. **禁止输出敏感路径**：使用相对路径或 `[FILE]` 占位
