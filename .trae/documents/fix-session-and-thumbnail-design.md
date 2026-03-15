# 会话管理与缩略图显示问题修复计划

## 问题分析

### 问题 1：QML 调用错误

**错误信息**：
```
qrc:/qt/qml/EnhanceVision/qml/components/MediaThumbnailStrip.qml:89: Error: Unable to determine callable overload. Candidates are:
    index(int,int)
    index(int,int,QModelIndex)
```

**原因**：`mediaModel.index(i)` 调用方式错误，QAbstractListModel 的 `index()` 方法需要两个参数：
- `index(row, column)` 或 `index(row, column, parent)`
- 正确调用方式：`mediaModel.index(i, 0)`

### 问题 2：会话管理架构缺陷

**当前架构问题**：
1. `MessageModel` 和 `FileModel` 是全局单例，没有与会话绑定
2. `Session` 结构体有 `messages` 字段但未使用
3. 切换会话时不会更新消息和文件列表
4. `ensureActiveSession()` 每次检查 `rowCount() == 0`，但活动会话可能已存在

**用户反馈的问题**：
- 同一会话中发送多条消息，却创建了多个会话标签
- 创建新会话后，界面没有变成干净窗口
- 会话切换时，程序如何显示？

### 问题 3：缩略图数据获取方式错误

**当前代码**：
```qml
var data = mediaModel.data(mediaModel.index(i), 257)  // 期望返回对象
var filePath = data.filePath  // data 实际是 QString，不是对象
```

**问题**：`FileModel::data()` 返回单个角色值，不是包含所有字段的对象。

---

## 设计方案

### 会话管理架构重设计

```
┌─────────────────────────────────────────────────────────────────┐
│                    SessionController                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ sessionModel                                             │    │
│  │  └─ m_sessions[]                                         │    │
│  │       ├─ Session 1 (isActive: true)                      │    │
│  │       │   ├─ id: "xxx"                                   │    │
│  │       │   ├─ name: "未命名会话 1"                         │    │
│  │       │   ├─ messages: []  ←─┐                           │    │
│  │       │   └─ files: []      │ 同步                       │    │
│  │       │                       │                           │    │
│  │       ├─ Session 2          │                           │    │
│  │       │   ├─ messages: []  ←┤                           │    │
│  │       │   └─ files: []      │                           │    │
│  │       └─ ...                 │                           │    │
│  └──────────────────────────────│───────────────────────────┘    │
│                                 │                                │
│                                 ▼                                │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ 全局模型（用于 QML 绑定）                                 │    │
│  │  ├─ messageModel (当前活动会话的消息)                     │    │
│  │  └─ fileModel (当前活动会话的待处理文件)                  │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### 会话生命周期

```
应用启动
    │
    ▼
没有会话（侧边栏为空）
    │
    ▼
用户上传文件 → 添加到 fileModel（不创建会话）
    │
    ▼
用户点击发送 → ensureActiveSession() → 创建会话（如果需要）
    │                                    → 将 fileModel 数据关联到会话
    │                                    → 发送处理任务
    │
    ▼
会话创建后 → 锁定在此会话中
    │
    ▼
后续发送消息 → 检查是否有活动会话 → 有则复用，不创建新会话
    │
    ▼
用户切换会话 → switchSession() → 保存当前会话数据
    │                            → 加载目标会话数据到全局模型
    │                            → 更新 UI
    │
    ▼
用户新建会话 → createSession() → 清空全局模型
    │                            → 界面变成干净窗口
```

---

## 实施计划

### 任务 1：修复 MediaThumbnailStrip.qml 数据获取

**文件**：`qml/components/MediaThumbnailStrip.qml`

**修改**：
1. 修复 `index()` 调用：`mediaModel.index(i, 0)`
2. 使用正确的数据获取方式：
   - 方案 A：逐个获取角色值
   - 方案 B：使用 ListView delegate 自动绑定

**正确代码**：
```qml
function _rebuildFiltered() {
    filteredModel.clear()
    if (!mediaModel) return
    
    if (typeof mediaModel.data === "function") {
        for (var i = 0; i < mediaModel.rowCount(); i++) {
            var idx = mediaModel.index(i, 0)  // 修复：添加 column 参数
            var filePath = mediaModel.data(idx, 258)  // FilePathRole = 258
            var fileName = mediaModel.data(idx, 259)  // FileNameRole = 259
            var mediaType = mediaModel.data(idx, 262) // MediaTypeRole = 262
            var thumbnail = mediaModel.data(idx, 263) // ThumbnailRole = 263
            var status = mediaModel.data(idx, 266)    // StatusRole = 266
            var resultPath = mediaModel.data(idx, 267) // ResultPathRole = 267
            
            if (!onlyCompleted || status === 2) {
                filteredModel.append({
                    "origIndex": i,
                    "filePath": filePath || "",
                    "fileName": fileName || "",
                    "mediaType": mediaType !== undefined ? mediaType : 0,
                    "thumbnail": thumbnail || "",
                    "status": status !== undefined ? status : 0,
                    "resultPath": resultPath || ""
                })
            }
        }
    }
}
```

### 任务 2：修复 MessageItem.qml 缩略图显示

**文件**：`qml/components/MessageItem.qml`

**检查**：确保消息中的多媒体文件缩略图正确显示

### 任务 3：重构会话管理架构

#### 3.1 修改 Session 数据结构

**文件**：`include/EnhanceVision/models/DataTypes.h`

**添加**：
```cpp
struct Session {
    QString id;
    QString name;
    QDateTime createdAt;
    QDateTime modifiedAt;
    QList<Message> messages;
    QList<MediaFile> pendingFiles;  // 新增：待处理文件列表
    bool isActive;
    bool isSelected;
    bool isPinned;
    int sortIndex;
};
```

#### 3.2 修改 SessionController

**文件**：`src/controllers/SessionController.cpp`

**修改 `ensureActiveSession()`**：
```cpp
QString SessionController::ensureActiveSession()
{
    // 如果已有活动会话，直接返回
    if (!m_sessionModel->activeSessionId().isEmpty()) {
        return m_sessionModel->activeSessionId();
    }
    
    // 没有活动会话，创建新会话
    return createSession();
}
```

**添加会话数据同步方法**：
```cpp
void SessionController::syncToSession(const QString &sessionId);
void SessionController::loadFromSession(const QString &sessionId);
```

#### 3.3 修改 SessionModel::switchSession()

**文件**：`src/models/SessionModel.cpp`

**修改**：
```cpp
void SessionModel::switchSession(const QString &sessionId)
{
    // 1. 保存当前会话数据
    QString currentId = m_activeSessionId;
    
    // 2. 更新 isActive 状态
    for (Session &session : m_sessions) {
        session.isActive = (session.id == sessionId);
    }
    
    // 3. 更新活动会话 ID
    m_activeSessionId = sessionId;
    
    // 4. 发出信号，通知 UI 更新
    emit activeSessionChanged();
    emit dataChanged(index(0), index(m_sessions.size() - 1), {IsActiveRole});
}
```

#### 3.4 添加 SessionController::syncCurrentSession()

**功能**：将当前全局模型数据同步到活动会话

```cpp
void SessionController::syncCurrentSession()
{
    QString activeId = m_sessionModel->activeSessionId();
    if (activeId.isEmpty()) return;
    
    // 获取当前消息和文件列表
    // 同步到 Session 结构体
}
```

#### 3.5 添加 SessionController::loadSessionData()

**功能**：从会话加载数据到全局模型

```cpp
void SessionController::loadSessionData(const QString &sessionId)
{
    // 获取会话数据
    // 更新 MessageModel 和 FileModel
}
```

### 任务 4：修改 ProcessingController

**文件**：`src/controllers/ProcessingController.cpp`

**修改 `sendToProcessing()`**：
- 发送后不要清空 fileModel
- 将处理结果关联到当前会话

### 任务 5：修改 MainPage.qml

**文件**：`qml/pages/MainPage.qml`

**添加会话切换监听**：
```qml
Connections {
    target: sessionController
    
    function onActiveSessionChanged() {
        // 更新 UI 显示
    }
}
```

### 任务 6：构建验证

- 清理日志
- CMake 配置
- 编译项目
- 检查错误和警告
- 循环修复直到无问题

---

## 文件修改清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `qml/components/MediaThumbnailStrip.qml` | 修改 | 修复 index() 调用和数据获取方式 |
| `qml/components/MessageItem.qml` | 检查 | 确保缩略图正确显示 |
| `include/EnhanceVision/models/DataTypes.h` | 修改 | 添加 pendingFiles 字段 |
| `include/EnhanceVision/controllers/SessionController.h` | 修改 | 添加数据同步方法 |
| `src/controllers/SessionController.cpp` | 修改 | 重构 ensureActiveSession()，添加同步逻辑 |
| `src/models/SessionModel.cpp` | 修改 | 完善会话切换逻辑 |
| `src/controllers/ProcessingController.cpp` | 修改 | 发送后不清空文件列表 |
| `qml/pages/MainPage.qml` | 修改 | 添加会话切换监听 |

---

## 预期效果

1. **会话创建**：
   - 应用启动后没有会话标签
   - 上传文件后不创建会话
   - 点击发送后才创建会话（如果没有活动会话）
   - 后续发送消息复用当前会话

2. **会话切换**：
   - 切换会话时，消息列表和文件列表正确更新
   - 界面显示目标会话的内容

3. **缩略图显示**：
   - 上传文件后，多媒体展示区域显示缩略图
   - 消息中的多媒体文件显示缩略图
