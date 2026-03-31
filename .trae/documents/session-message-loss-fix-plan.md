# 会话标签消息消失问题修复计划

## 问题描述

用户报告：当对会话标签进行置顶/取消置顶或拖拽排序后，某些情况下会导致会话标签中的消息全部消失。

## 根本原因分析

### 核心问题：索引缓存未同步更新

在 `SessionController` 中维护了三个关键的索引缓存：
- `m_sessionRowById`: 会话ID → 会话在列表中的行号
- `m_messageRowBySessionId`: 会话ID → (消息ID → 消息行号)
- `m_messageToSessionId`: 消息ID → 会话ID

**问题所在**：

1. **置顶操作**：`SessionController::pinSession()` 调用 `SessionModel::pinSession()`，后者调用 `sortSessions()` 重新排序会话列表，但 `SessionController` 中的索引缓存没有被更新。

2. **拖拽排序**：`SessionController::moveSession()` 调用 `SessionModel::moveSession()`，会话位置改变后，索引缓存同样没有被更新。

### 问题代码位置

#### SessionController.cpp 第377-381行
```cpp
void SessionController::pinSession(const QString& sessionId, bool pinned)
{
    m_sessionModel->pinSession(sessionId, pinned);
    emit sessionPinned(sessionId, pinned);
    // 缺少: rebuildSessionMessageIndex();
}
```

#### SessionController.cpp 第383-387行
```cpp
void SessionController::moveSession(int fromIndex, int toIndex)
{
    m_sessionModel->moveSession(fromIndex, toIndex);
    emit sessionMoved(fromIndex, toIndex);
    // 缺少: rebuildSessionMessageIndex();
}
```

### 消息消失的具体流程

1. 用户置顶/取消置顶会话或拖拽排序
2. `SessionModel` 重新排序会话列表
3. `SessionController` 中的 `m_sessionRowById` 缓存失效（行号与会话ID不匹配）
4. 用户切换会话时，`getSession()` 通过失效的索引查找会话
5. 可能返回错误的会话或 nullptr
6. 消息被写入/读取到错误的会话，或无法找到会话导致消息"消失"

## 修复方案

### 方案1：在排序后重建索引缓存（推荐）

**优点**：
- 实现简单直接
- 不改变现有架构
- 确保索引始终正确

**修改点**：
1. 在 `SessionController::pinSession()` 末尾添加 `rebuildSessionMessageIndex()`
2. 在 `SessionController::moveSession()` 末尾添加 `rebuildSessionMessageIndex()`

### 方案2：使用信号机制自动重建索引

**优点**：
- 解耦更好
- 自动化程度高

**修改点**：
1. 在 `SessionModel` 中添加 `sessionsReordered()` 信号
2. 在 `sortSessions()` 和 `moveSession()` 后发出该信号
3. `SessionController` 连接该信号并自动重建索引

### 方案3：改进排序方法使用 moveRows

**优点**：
- 保持视图状态（选中、滚动位置）
- 动画效果更自然
- 更符合 Qt Model/View 模式

**缺点**：
- 实现复杂度高
- 需要计算精确的移动路径

## 推荐方案：方案1 + 方案2 结合

采用方案1的简洁实现，同时引入方案2的信号机制，确保未来扩展时索引同步的可靠性。

## 实施步骤

### 步骤1：修改 SessionModel.h
- 添加 `sessionsReordered()` 信号

### 步骤2：修改 SessionModel.cpp
- 在 `sortSessions()` 末尾发出 `sessionsReordered()` 信号
- 在 `moveSession()` 末尾发出 `sessionsReordered()` 信号

### 步骤3：修改 SessionController.cpp
- 在构造函数中连接 `sessionsReordered()` 信号到 `rebuildSessionMessageIndex()`
- 移除 `pinSession()` 和 `moveSession()` 中手动调用 `rebuildSessionMessageIndex()` 的需求（通过信号自动处理）

### 步骤4：验证修复
- 构建并运行项目
- 测试置顶/取消置顶功能
- 测试拖拽排序功能
- 验证消息不再消失

## 测试用例

1. **置顶测试**：
   - 创建多个会话，每个会话添加消息
   - 置顶一个会话，验证消息仍存在
   - 取消置顶，验证消息仍存在
   - 切换到其他会话，验证消息正确

2. **拖拽排序测试**：
   - 创建多个会话，每个会话添加消息
   - 拖拽排序会话
   - 切换到各个会话，验证消息正确

3. **混合操作测试**：
   - 置顶 + 拖拽排序 + 切换会话
   - 验证所有操作后消息仍正确

## 风险评估

- **低风险**：修改仅涉及索引同步，不影响核心业务逻辑
- **性能影响**：`rebuildSessionMessageIndex()` 时间复杂度为 O(n*m)，但对于正常使用场景（几十个会话，每个会话几十条消息）影响可忽略

## 文件修改清单

| 文件 | 修改内容 |
|------|----------|
| `include/EnhanceVision/models/SessionModel.h` | 添加 `sessionsReordered()` 信号 |
| `src/models/SessionModel.cpp` | 在排序方法中发出信号 |
| `src/controllers/SessionController.cpp` | 连接信号到索引重建方法 |
