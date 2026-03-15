# 会话管理与消息模型架构修复计划

## 问题分析

### 当前架构缺陷

1. **消息双重存储**：
   - `Session.messages` 字段存在但从未使用
   - `MessageModel.m_messages` 是全局单例，所有会话共享同一消息列表

2. **会话与消息无关联**：
   - 消息不绑定到 Session
   - 切换会话时消息列表不变
   - 清空/删除会话不影响消息

3. **两个 SessionModel 实例**：
   - `Application::m_sessionModel` (暴露给 QML 但未使用)
   - `SessionController::m_sessionModel` (实际使用)

4. **ensureActiveSession 逻辑问题**：
   - 只检查 `activeSessionId.isEmpty()`，不检查是否有选中会话

---

## 设计方案

### 新架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│                    SessionController                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ sessionModel                                             │    │
│  │  └─ m_sessions[]                                         │    │
│  │       ├─ Session 1 (isActive: true)                      │    │
│  │       │   ├─ id: "xxx"                                   │    │
│  │       │   ├─ name: "未命名会话 1"                         │    │
│  │       │   └─ messages: []  ←─ 消息存储在会话中            │    │
│  │       │                                                  │    │
│  │       ├─ Session 2                                       │    │
│  │       │   └─ messages: []                                │    │
│  │       └─ ...                                             │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                 │                                │
│                                 │ 切换会话时同步                  │
│                                 ▼                                │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ messageModel (视图模型)                                   │    │
│  │  └─ m_messages: 当前活动会话的消息副本                    │    │
│  │      (用于 QML ListView 绑定)                            │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 会话生命周期

```
应用启动
    │
    ▼
没有会话或会话未选中状态（侧边栏有会话但未高亮，消息列表为空）
    │
    ▼
用户上传文件 → 添加到 fileModel（不创建会话）
    │
    ▼
用户点击发送 → ensureActiveSession()
    │           ├─ 如果用户已手动选中会话 → 复用当前会话
    │           └─ 如果用户未选中会话 → 创建新会话并锁定选中
    │
    ▼
会话创建/锁定后 → 发送处理任务 → 消息添加到当前会话
    │
    ▼
后续发送消息 → 复用当前会话（因为已锁定选中）
    │
    ▼
用户切换会话 → switchSession()
    │              ├─ 保存当前会话消息到 Session.messages
    │              └─ 加载目标会话消息到 MessageModel
    │              └─ 标记为"用户已选中"状态
    │
    ▼
用户新建会话 → createSession() → 清空 MessageModel
    │                              → 锁定选中到新会话
    │
    ▼
用户删除会话 → deleteSession() → 删除会话及消息
    │                              → 如果删除的是当前选中会话，变为"未选中"状态
    │
    ▼
用户清空会话 → clearSession() → 清空消息，保留会话
```

### 关键状态区分

```
┌─────────────────────────────────────────────────────────────────┐
│  状态 1: 应用首次启动                                            │
│  - sessionModel.rowCount() == 0                                 │
│  - activeSessionId.isEmpty() == true                            │
│  - 用户未选中任何会话                                             │
│  - 消息列表为空                                                  │
│  - 发送消息 → 创建新会话并锁定                                     │
├─────────────────────────────────────────────────────────────────┤
│  状态 2: 有会话但用户未选中                                       │
│  - sessionModel.rowCount() > 0                                  │
│  - activeSessionId.isEmpty() == true                            │
│  - 用户未选中任何会话（侧边栏会话不高亮）                           │
│  - 消息列表为空                                                  │
│  - 发送消息 → 创建新会话并锁定                                     │
├─────────────────────────────────────────────────────────────────┤
│  状态 3: 用户已选中会话                                          │
│  - sessionModel.rowCount() > 0                                  │
│  - activeSessionId.isEmpty() == false                           │
│  - 用户已选中某个会话（侧边栏会话高亮）                             │
│  - 消息列表显示该会话的消息                                        │
│  - 发送消息 → 复用当前会话                                        │
└─────────────────────────────────────────────────────────────────┘
```

---

## 实施任务

### [ ] Task 1: 修改 MessageModel 支持会话关联
- **Priority**: P0
- **Depends On**: None
- **Description**: 
  - 为 MessageModel 添加 `setCurrentSession()` 方法
  - 添加 `syncToSession()` 方法保存消息到会话
  - 添加 `loadFromSession()` 方法从会话加载消息
- **Success Criteria**:
  - MessageModel 可以切换显示不同会话的消息
- **Test Requirements**:
  - `programmatic` TR-1.1: 切换会话后，MessageModel 的消息列表正确更新
  - `human-judgement` TR-1.2: 切换会话时 UI 正确显示对应消息

### [ ] Task 2: 修改 SessionController 实现会话数据同步
- **Priority**: P0
- **Depends On**: Task 1
- **Description**: 
  - 修改 `createSession()` 创建会话但不自动选中，清空 MessageModel
  - 修改 `switchSession()` 在切换前保存当前消息，切换后加载目标消息，标记为"用户已选中"
  - 修改 `deleteSession()` 删除会话时清理相关消息，如果删除的是当前选中会话则变为"未选中"状态
  - 修改 `clearSession()` 清空会话时清空 MessageModel
  - 修改 `ensureActiveSession()` 只在 activeSessionId 为空时创建新会话并锁定选中
  - **关键**：createSession() 不再自动设置 isActive=true，只有用户手动点击或发送消息时才锁定选中
- **Success Criteria**:
  - 会话操作正确同步消息数据
  - 应用启动时即使有会话也是未选中状态
- **Test Requirements**:
  - `programmatic` TR-2.1: 新建会话后 MessageModel 为空，且新会话被锁定选中
  - `programmatic` TR-2.2: 切换会话后显示正确的消息
  - `programmatic` TR-2.3: 删除会话后相关消息被清理
  - `programmatic` TR-2.4: 清空会话后消息被清空但会话保留
  - `programmatic` TR-2.5: 应用启动时 activeSessionId 为空（即使有会话）

### [ ] Task 3: 修改 ProcessingController 关联消息到会话
- **Priority**: P0
- **Depends On**: Task 2
- **Description**: 
  - 修改 `sendToProcessing()` 将消息添加到当前活动会话
  - 确保消息与会话正确关联
- **Success Criteria**:
  - 发送的处理任务正确关联到当前会话
- **Test Requirements**:
  - `programmatic` TR-3.1: 发送消息后，当前会话的 messages 列表包含该消息
  - `programmatic` TR-3.2: 切换会话后发送消息，消息添加到新会话

### [ ] Task 4: 修改 QML 层交互逻辑
- **Priority**: P1
- **Depends On**: Task 2, Task 3
- **Description**: 
  - 修改 Sidebar.qml 新建会话按钮逻辑
  - 确保 MainPage.qml 发送消息时正确调用 ensureActiveSession
  - 添加会话切换时的 UI 更新
- **Success Criteria**:
  - QML 层正确响应用户操作
- **Test Requirements**:
  - `human-judgement` TR-4.1: 新建会话后界面干净无消息
  - `human-judgement` TR-4.2: 切换会话后显示正确的消息列表
  - `human-judgement` TR-4.3: 删除会话后界面正确更新

### [ ] Task 5: 构建验证和测试
- **Priority**: P0
- **Depends On**: Task 1-4
- **Description**: 
  - 清理日志
  - CMake 配置
  - 编译项目
  - 检查错误和警告
  - 循环修复直到无问题
- **Success Criteria**:
  - 项目编译成功，无错误无警告
- **Test Requirements**:
  - `programmatic` TR-5.1: 编译退出代码为 0
  - `programmatic` TR-5.2: 无编译错误
  - `programmatic` TR-5.3: 无项目代码警告

---

## 文件修改清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `include/EnhanceVision/models/MessageModel.h` | 修改 | 添加会话关联方法声明 |
| `src/models/MessageModel.cpp` | 修改 | 实现会话关联逻辑 |
| `include/EnhanceVision/controllers/SessionController.h` | 修改 | 添加 MessageModel 引用、createAndSelectSession 方法 |
| `src/controllers/SessionController.cpp` | 修改 | 实现会话数据同步、修改 createSession 不自动选中 |
| `src/controllers/ProcessingController.cpp` | 修改 | 关联消息到会话 |
| `src/app/Application.cpp` | 修改 | 连接 SessionController 和 MessageModel |
| `qml/components/Sidebar.qml` | 修改 | 新建会话按钮调用 createAndSelectSession |
| `qml/pages/MainPage.qml` | 检查 | 发送消息逻辑 |

---

## 关键代码修改

### MessageModel 新增方法

```cpp
// MessageModel.h
class MessageModel : public QAbstractListModel {
public:
    // 新增方法
    void setCurrentSessionId(const QString& sessionId);
    QString currentSessionId() const;
    
    void syncToSession(Session& session);
    void loadFromSession(const Session& session);
    
private:
    QString m_currentSessionId;
};
```

### SessionController 修改

```cpp
// SessionController.h
class SessionController : public QObject {
public:
    void setMessageModel(MessageModel* model);
    
private:
    MessageModel* m_messageModel = nullptr;
    
    void saveCurrentSessionMessages();
    void loadSessionMessages(const QString& sessionId);
};
```

### ensureActiveSession 修改

```cpp
QString SessionController::ensureActiveSession()
{
    // 如果有活动会话，直接返回（复用）
    if (!m_sessionModel->activeSessionId().isEmpty()) {
        return m_sessionModel->activeSessionId();
    }
    
    // 没有活动会话，创建新会话并锁定
    QString newId = createSession();
    switchSession(newId);  // 锁定到新会话
    return newId;
}
```

### switchSession 修改

```cpp
void SessionController::switchSession(const QString& sessionId)
{
    QString currentId = m_sessionModel->activeSessionId();
    if (currentId == sessionId) return;
    
    // 1. 保存当前会话的消息
    if (!currentId.isEmpty() && m_messageModel) {
        Session* currentSession = m_sessionModel->getSession(currentId);
        if (currentSession) {
            m_messageModel->syncToSession(*currentSession);
        }
    }
    
    // 2. 切换会话
    m_sessionModel->switchSession(sessionId);
    m_activeSessionId = sessionId;
    
    // 3. 加载新会话的消息
    if (m_messageModel) {
        Session* newSession = m_sessionModel->getSession(sessionId);
        if (newSession) {
            m_messageModel->loadFromSession(*newSession);
        }
    }
    
    emit sessionSwitched(sessionId);
    emit activeSessionChanged();
}
```

### createSession 修改

```cpp
QString SessionController::createSession(const QString& name)
{
    Session session;
    session.id = generateSessionId();
    session.name = name.isEmpty() ? generateDefaultSessionName() : name;
    session.createdAt = QDateTime::currentDateTime();
    session.modifiedAt = QDateTime::currentDateTime();
    session.isActive = false;  // 不自动选中，只有用户手动操作才选中
    session.isSelected = false;
    session.isPinned = false;
    session.sortIndex = m_sessionModel->rowCount();

    m_sessionModel->addSession(session);

    emit sessionCreated(session.id);
    emit sessionCountChanged();

    return session.id;
}

// 新增：创建会话并锁定选中（用于新建会话按钮）
QString SessionController::createAndSelectSession(const QString& name)
{
    QString sessionId = createSession(name);
    switchSession(sessionId);  // 锁定选中
    return sessionId;
}
```
