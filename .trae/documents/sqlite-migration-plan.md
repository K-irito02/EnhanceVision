# SQLite 数据库迁移方案

## 一、现状分析

### 1.1 当前数据存储方式

| 数据类型 | 存储方式 | 文件/位置 | 问题 |
|---------|---------|----------|------|
| 会话数据 | JSON 文件 | `data/sessions.json` | 大数据量时性能差、全量读写 |
| 消息数据 | 嵌套在会话 JSON 中 | 同上 | 无法单独查询消息 |
| 媒体文件 | 嵌套在消息 JSON 中 | 同上 | 缩略图未持久化 |
| 缩略图 | 内存 QHash | `ThumbnailProvider::m_thumbnails` | **重启丢失、内存溢出导致灰图** |
| 应用设置 | QSettings | 系统注册表/INI | 功能正常，暂不迁移 |
| 处理队列 | 内存 | `ProcessingModel::m_tasks` | 运行时数据，无需持久化 |

### 1.2 数据关系模型（当前）

```
Session (1)
  ├── Message (N)
  │     └── MediaFile (N)  ← thumbnail 在内存中，不持久化
  └── pendingFiles (N)     ← 待处理文件列表
```

### 1.3 核心痛点

1. **缩略图灰图问题**：`ThumbnailProvider` 使用纯内存缓存（QHash），应用重启或内存压力下缓存被清除后显示灰色占位图
2. **JSON 全量读写**：每次保存/加载都需序列化/反序列化全部数据，数据量大时性能急剧下降
3. **无法高效查询**：无法按条件筛选消息（如按日期、状态、模式），统计功能难以实现
4. **并发写入风险**：异步保存使用临时文件+rename，但无事务保护

---

## 二、数据库架构设计

### 2.1 技术选型

- **数据库引擎**：SQLite 3（Qt6 内置 `Qt6::Sql` 模块）
- **访问方式**：原生 SQLite C API（通过 `QSqlDatabase` + 自定义封装）
- **连接管理**：单例连接池（主线程写 + 读线程读，WAL 模式支持并发读）

### 2.2 ER 图与表结构设计

```
┌──────────────────────┐       ┌──────────────────────┐
│      sessions        │       │       messages       │
├──────────────────────┤       ├──────────────────────┤
│ id TEXT PK           │──┐    │ id TEXT PK           │
│ name TEXT            │  └───>│ session_id TEXT FK   │──┐
│ created_at INTEGER   │       │ timestamp INTEGER    │  │
│ modified_at INTEGER  │       │ mode INTEGER         │  │
│ is_active INTEGER    │       │ status INTEGER       │  │
│ is_selected INTEGER  │       │ progress INTEGER     │  │
│ is_pinned INTEGER    │       │ queue_position INT   │  │
│ sort_index INTEGER   │       │ error_message TEXT   │  │
└──────────────────────┘       │ parameters TEXT      │  │
                               │ shader_params TEXT   │  │
┌──────────────────────┐       │ ai_model_id TEXT     │  │
│   pending_files      │       │ ai_category INTEGER  │  │
├──────────────────────┤       │ ai_use_gpu INTEGER   │  │
│ id TEXT PK           │       │ ai_tile_size INTEGER │  │
│ session_id TEXT FK   │──┐    │ ai_model_params TEXT │  │
│ file_path TEXT       │  │    └──────────────────────┘  │
│ file_name TEXT       │  │              │               │
│ file_size INTEGER    │  │              │               │
│ media_type INTEGER   │  │    ┌─────────┴───────────────┘
│ duration INTEGER     │  │    │
│ width INTEGER        │  │    ▼
│ height INTEGER       │  │  ┌──────────────────────┐
│ status INTEGER       │  ──>│     media_files      │
│ result_path TEXT     │     ├──────────────────────┤
└──────────────────────┘     │ id TEXT PK           │
                             │ message_id TEXT FK   │
┌──────────────────────┐     │ pending_file_id FK   │
│     thumbnails       │     │ file_path TEXT       │
├──────────────────────┤     │ original_path TEXT   │
│ media_file_id TEXT PK│     │ file_name TEXT       │
│ thumbnail BLOB       │     │ file_size INTEGER    │
│ width INTEGER        │     │ media_type INTEGER   │
│ height INTEGER       │     │ duration INTEGER     │
│ updated_at INTEGER   │     │ resolution_w INTEGER │
└──────────────────────┘     │ resolution_h INTEGER │
                             │ status INTEGER       │
                             │ result_path TEXT     │
                             └──────────────────────┘
```

### 2.3 详细建表 SQL

```sql
-- ============================================================
-- 1. 会话表
-- ============================================================
CREATE TABLE IF NOT EXISTS sessions (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL DEFAULT '',
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    modified_at     INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    is_active       INTEGER NOT NULL DEFAULT 0,
    is_selected     INTEGER NOT NULL DEFAULT 0,
    is_pinned       INTEGER NOT NULL DEFAULT 0,
    sort_index      INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_sessions_sort ON sessions(is_pinned DESC, sort_index ASC);
CREATE INDEX IF NOT EXISTS idx_sessions_modified ON sessions(modified_at DESC);

-- ============================================================
-- 2. 消息表
-- ============================================================
CREATE TABLE IF NOT EXISTS messages (
    id                  TEXT PRIMARY KEY,
    session_id          TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    timestamp           INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    mode                INTEGER NOT NULL DEFAULT 0,
    status              INTEGER NOT NULL DEFAULT 0,
    progress            INTEGER NOT NULL DEFAULT 0,
    queue_position      INTEGER NOT NULL DEFAULT -1,
    error_message       TEXT DEFAULT '',
    -- 复杂参数以 JSON 字符串存储（灵活且可扩展）
    parameters          TEXT DEFAULT '{}',
    shader_params       TEXT DEFAULT '{}',
    -- AI 参数拆分为独立列（便于查询和统计）
    ai_model_id         TEXT DEFAULT '',
    ai_category         INTEGER NOT NULL DEFAULT 0,
    ai_use_gpu          INTEGER NOT NULL DEFAULT 1,
    ai_tile_size        INTEGER NOT NULL DEFAULT 0,
    ai_auto_tile_size   INTEGER NOT NULL DEFAULT 1,
    ai_model_params     TEXT DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_messages_session ON messages(session_id);
CREATE INDEX IF NOT EXISTS idx_messages_status ON messages(status);
CREATE INDEX IF NOT EXISTS idx_messages_mode ON messages(mode);
CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp DESC);

-- ============================================================
-- 3. 媒体文件表
-- ============================================================
CREATE TABLE IF NOT EXISTS media_files (
    id              TEXT PRIMARY KEY,
    message_id      TEXT REFERENCES messages(id) ON DELETE CASCADE,
    pending_file_id TEXT REFERENCES pending_files(id) ON DELETE SET NULL,
    file_path       TEXT NOT NULL DEFAULT '',
    original_path   TEXT DEFAULT '',
    file_name       TEXT NOT NULL DEFAULT '',
    file_size       INTEGER NOT NULL DEFAULT 0,
    media_type      INTEGER NOT NULL DEFAULT 0,
    duration        INTEGER NOT NULL DEFAULT 0,
    resolution_w    INTEGER NOT NULL DEFAULT 0,
    resolution_h    INTEGER NOT NULL DEFAULT 0,
    status          INTEGER NOT NULL DEFAULT 0,
    result_path     TEXT DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_media_files_message ON media_files(message_id);
CREATE INDEX IF NOT EXISTS idx_media_files_pending ON media_files(pending_file_id);
CREATE INDEX IF NOT EXISTS idx_media_files_status ON media_files(status);

-- ============================================================
-- 4. 待处理文件表（独立于 message 的 pendingFiles）
-- ============================================================
CREATE TABLE IF NOT EXISTS pending_files (
    id          TEXT PRIMARY KEY,
    session_id  TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    file_path   TEXT NOT NULL DEFAULT '',
    file_name   TEXT NOT NULL DEFAULT '',
    file_size   INTEGER NOT NULL DEFAULT 0,
    media_type  INTEGER NOT NULL DEFAULT 0,
    duration    INTEGER NOT NULL DEFAULT 0,
    width       INTEGER NOT NULL DEFAULT 0,
    height      INTEGER NOT NULL DEFAULT 0,
    status      INTEGER NOT NULL DEFAULT 0,
    result_path TEXT DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_pending_files_session ON pending_files(session_id);

-- ============================================================
-- 5. 缩略图表（BLOB 存储 + LRU 元信息）
-- ============================================================
CREATE TABLE IF NOT EXISTS thumbnails (
    media_file_id   TEXT PRIMARY KEY REFERENCES media_files(id) ON DELETE CASCADE,
    thumbnail       BLOB,
    width           INTEGER NOT NULL DEFAULT 256,
    height          INTEGER NOT NULL DEFAULT 256,
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s','now'))
);

-- ============================================================
-- 6. 应用元数据表（替代原 JSON 的 root 字段）
-- ============================================================
CREATE TABLE IF NOT EXISTS app_metadata (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- 插入默认元数据
INSERT OR IGNORE INTO app_metadata(key, value) VALUES ('version', '2');
INSERT OR IGNORE INTO app_metadata(key, value) VALUES ('session_counter', '0');
INSERT OR IGNORE INTO app_metadata(key, value) VALUES ('last_active_session_id', '');

-- 启用外键约束和 WAL 模式
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA cache_size = -4096;  -- 4MB 页缓存
```

---

## 三、缩略图 LRU 缓存策略

### 3.1 双层缓存架构

```
                    ┌─────────────────────┐
                    │   QML Image 请求     │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │  L1: 内存缓存        │  ← QMap<QString, QImage>
                    │  (最近使用的 N 张)    │  ← 容量上限: 200MB 或 500张
                    └──────────┬──────────┘
                               │ 未命中
                    ┌──────────▼──────────┐
                    │  L2: SQLite BLOB     │  ← 持久化存储
                    │  (thumbnails 表)     │  ← 所有历史缩略图
                    └──────────┬──────────┘
                               │ 未命中
                    ┌──────────▼──────────┐
                    │  L3: 异步生成        │  ← ThumbnailGenerator
                    │  (从源文件生成)       │  → 存入 L2 + L1
                    └─────────────────────┘
```

### 3.2 LRU 实现细节

- **容器**：`QList<QPair<QString, QImage>>` + `QHash<QString, QList::iterator>` 实现 O(1) 查找 + O(1) 淘汰
- **容量控制**：
  - 最大条目数：500 条
  - 最大内存：200 MB（动态计算每张图片大小）
  - 淘汰策略：LRU（最少最近使用优先淘汰）
- **写入时机**：
  - 缩略图生成完成后同时写入 DB（L2）和内存（L1）
  - 批量写入优化：攒批提交（每 10 张或 5 秒一次）

### 3.3 ThumbnailProvider 改造要点

```cpp
// 改造后的 requestImage 流程
QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // 1. 查 L1 内存缓存（现有逻辑保留）
    if (m_lruCache.contains(key)) return m_lruCache.get(key);

    // 2. 【新增】查 L2 数据库缓存
    QImage dbThumbnail = m_dbService->loadThumbnail(mediaFileId);
    if (!dbThumbnail.isNull()) {
        m_lruCache.put(key, dbThumbnail);  // 放入 L1
        return dbThumbnail;
    }

    // 3. 异步生成（现有逻辑保留）
    generateThumbnailAsync(filePath, key, targetSize);

    // 4. 返回占位图（现有逻辑保留）
    return placeholder;
}
```

---

## 四、统计功能设计

### 4.1 统计查询 SQL

```sql
-- 按日/周/月统计处理数量和成功率
SELECT
    date(timestamp, 'unixepoch', 'localtime') AS date,
    COUNT(*) AS total,
    SUM(CASE WHEN status = 3 THEN 1 ELSE 0 END) AS completed,
    SUM(CASE WHEN status = 4 THEN 1 ELSE 0 END) AS failed,
    ROUND(100.0 * SUM(CASE WHEN status = 3 THEN 1 ELSE 0 END) / COUNT(*), 1) AS success_rate
FROM messages
WHERE timestamp >= ? AND timestamp <= ?
GROUP BY date ORDER BY date;

-- 各 AI 模型使用频率
SELECT
    ai_model_id,
    ai_category,
    COUNT(*) AS usage_count,
    ROUND(100.0 * COUNT(*) / (SELECT COUNT(*) FROM messages WHERE ai_model_id != ''), 1) AS percentage
FROM messages
WHERE ai_model_id != ''
GROUP BY ai_model_id ORDER BY usage_count DESC;

-- Shader 参数分布（亮度直方图）
SELECT
    CAST(json_extract(shader_params, '$.brightness') * 10 AS INTEGER) / 10.0 AS brightness_bucket,
    COUNT(*) AS count
FROM messages
WHERE mode = 0 AND shader_params != '{}'
GROUP BY brightness_bucket ORDER BY brightness_bucket;

-- 视频/图片处理比例
SELECT
    mf.media_type,
    COUNT(DISTINCT mf.id) AS file_count,
    ROUND(100.0 * COUNT(DISTINCT mf.id) / (SELECT COUNT(*) FROM media_files), 1) AS percentage
FROM media_files mf
JOIN messages m ON mf.message_id = m.id
GROUP BY mf.media_type;

-- 分辨率分布
SELECT
    CASE
        WHEN mf.resolution_w >= 3840 THEN '4K+'
        WHEN mf.resolution_w >= 2560 THEN '2K~4K'
        WHEN mf.resolution_w >= 1920 THEN '1080p~2K'
        WHEN mf.resolution_w >= 1280 THEN '720p~1080p'
        ELSE '<720p'
    END AS resolution_range,
    COUNT(*) AS count
FROM media_files mf
GROUP BY resolution_range ORDER BY count DESC;
```

### 4.2 新增 StatisticsService

提供统一的统计接口供 QML 调用：

```cpp
class StatisticsService : public QObject {
    Q_OBJECT
public:
    // 处理统计
    Q_INVOKABLE QVariantMap getProcessingStats(const QString &startDate, const QString &endDate);

    // AI 模型使用统计
    Q_INVOKABLE QVariantList getModelUsageStats();

    // Shader 参数分布
    Q_INVOKABLE QVariantMap getShaderParamDistribution(const QString &paramName);

    // 媒体类型分布
    Q_INVOKABLE QVariantList getMediaTypeDistribution();

    // 分辨率分布
    Q_INVOKABLE QVariantList getResolutionDistribution();
};
```

---

## 五、新增/修改文件清单

### 5.1 新增文件

| 文件路径 | 职责 |
|---------|------|
| `include/EnhanceVision/services/DatabaseService.h` | 数据库核心服务：连接管理、CRUD、事务、迁移 |
| `src/services/DatabaseService.cpp` | 数据库服务实现 |
| `include/EnhanceVision/services/ThumbnailCacheService.h` | 缩略图 LRU 缓存管理（L1 内存层） |
| `src/services/ThumbnailCacheService.cpp` | 缩略图缓存实现 |
| `include/EnhanceVision/services/StatisticsService.h` | 统计查询服务 |
| `src/services/StatisticsService.cpp` | 统计查询实现 |
| `include/EnhanceVision/services/DataBackupService.h` | 数据备份服务（完整备份/快速备份/自动备份/ZIP导出） |
| `src/services/DataBackupService.cpp` | 备份服务实现 |
| `include/EnhanceVision/services/DataRestoreService.h` | 数据恢复服务（校验/预览/恢复） |
| `src/services/DataRestoreService.cpp` | 恢复服务实现 |

### 5.2 修改文件

| 文件路径 | 修改内容 |
|---------|---------|
| `CMakeLists.txt` | 添加 `Qt6::Sql` 依赖；添加新源文件 |
| `include/EnhanceVision/controllers/SessionController.h` | 移除 JSON 序列化方法声明；改为调用 DatabaseService |
| `src/controllers/SessionController.cpp` | 重写 saveSessions/loadSessions 为数据库操作；移除所有 jsonTo*/toJson* 方法 |
| `include/EnhanceVision/providers/ThumbnailProvider.h` | 集成 ThumbnailCacheService；改造缓存策略 |
| `src/providers/ThumbnailProvider.cpp` | requestImage 增加 L2 查询；生成后同步写入 DB |
| `include/EnhanceVision/app/Application.h` | 注册 DatabaseService 和 StatisticsService |
| `src/app/Application.cpp` | 初始化顺序调整：DB → Sessions → Thumbnails → UI |

### 5.3 可选修改（后续优化）

| 文件路径 | 说明 |
|---------|------|
| `include/EnhanceVision/controllers/SettingsController.h` | 设置也可迁入 DB 的 `app_metadata` 表（非必须） |
| `qml/pages/SettingsPage.qml` | 新增统计面板入口 |

---

## 六、实施步骤（按执行顺序）

### Phase 1：基础设施搭建

**步骤 1.1 — CMakeLists.txt 添加 Sql 依赖**
- 在 `find_package(Qt6 REQUIRED COMPONENTS ...)` 中添加 `Sql`
- 在 `target_link_libraries` 中添加 `Qt6::Sql`
- 添加新源文件到 `qt_add_executable`

**步骤 1.2 — 创建 DatabaseService**
- 创建 `DatabaseService.h/cpp`
- 实现单例模式（类似 SettingsController）
- 核心功能：
  - `initialize()` — 打开/创建数据库、执行建表 SQL、启用 WAL + 外键
  - `db()` — 返回 QSqlDatabase 引用
  - 事务支持：`beginTransaction()` / `commitTransaction()` / `rollbackTransaction()`
  - 元数据 CRUD：`getMetadata()` / `setMetadata()`

**步骤 1.3 — 实现 DatabaseService 核心 CRUD**

Sessions 操作：
- `insertSession(const Session&) -> bool`
- `updateSession(const Session&) -> bool`
- `deleteSession(const QString& id) -> bool`
- `loadAllSessions() -> QList<Session>`
- `loadSessionMessages(const QString& sessionId) -> QList<Message>`
- `updateSessionCounter(int counter) -> void`

Messages 操作：
- `insertMessage(const QString& sessionId, const Message&) -> bool`
- `updateMessage(const Message&) -> bool`
- `deleteMessage(const QString& id) -> bool`
- `updateMessageProgress(const QString& id, int progress) -> bool`
- `updateMessageStatus(const QString& id, int status) -> bool`

MediaFiles 操作：
- `insertMediaFile(const QString& messageId, const MediaFile&) -> QString`
- `updateMediaFile(const MediaFile&) -> bool`
- `deleteMediaFile(const QString& id) -> bool`
- `loadMessageMediaFiles(const QString& messageId) -> QList<MediaFile>`

PendingFiles 操作：
- `insertPendingFile(const QString& sessionId, const MediaFile&) -> bool`
- `loadPendingFiles(const QString& sessionId) -> QList<MediaFile>`
- `clearPendingFiles(const QString& sessionId) -> bool`

Thumbnails 操作：
- `saveThumbnail(const QString& mediaFileId, const QImage& thumb) -> bool`
- `loadThumbnail(const QString& mediaFileId) -> QImage`
- `deleteThumbnail(const QString& mediaFileId) -> bool`
- `thumbnailExists(const QString& mediaFileId) -> bool`

批量操作：
- `saveFullSession(const Session&, const QList<Message>&) -> bool` （事务内完整保存一个会话）
- `batchUpdateThumbnails(const QVector<ThumbnailBatch>&) -> bool`

### Phase 2：缩略图系统改造

**步骤 2.1 — 创建 ThumbnailCacheService**
- LRU 缓存实现（O(1) get/put/evict）
- 配置参数：maxCount=500, maxMemoryBytes=200*1024*1024
- 接口：
  - `get(const QString& key) -> QImage` （查 L1，未命中返回空）
  - `put(const QString& key, const QImage& image)` （放入 L1，超限自动淘汰）
  - `remove(const QString& key)`
  - `clear()`
  - `currentMemoryUsage() -> qint64`
  - `currentCount() -> int`

**步骤 2.2 — 改造 ThumbnailProvider**
- 注入 `ThumbnailCacheService*` 和 `DatabaseService*`
- `requestImage()` 流程改造为三层查找（L1→L2→L3）
- `onThumbnailReady()` 中增加 DB 写入
- `generatePlaceholderImage()` 保持不变
- 保留 `normalizeFilePath()` / `normalizeKey()` 不变

**步骤 2.3 — 缩略图写入策略**
- 单张写入：每次 onThumbnailReady 后立即写 DB（保证不丢）
- 内存缓存延迟放入 LRU（避免频繁操作链表）
- 批量优化：使用 prepared statement 减少编译开销

### Phase 3：SessionController 迁移

**步骤 3.1 — 重写 saveSessions/loadSessions**
- `saveSessions()` → 调用 `DatabaseService::saveFullSession()` 遍历所有会话
- `loadSessions()` → 调用 `DatabaseService::loadAllSessions()` 加载
- `saveSessionsImmediately()` → 同步事务保存
- `saveCurrentSessionMessages()` → 只保存当前活动会话的消息

**步骤 3.2 — 移除 JSON 序列化代码**
- 删除以下方法（约 200 行）：
  - `sessionToJson()` / `jsonToSession()`
  - `messageToJson()` / `jsonToMessage()`
  - `mediaFileToJson()` / `jsonToMediaFile()`
  - `shaderParamsToJson()` / `jsonToShaderParams()`
  - `parametersToJson()` / `jsonToParameters()`
  - `sessionsFilePath()` / `ensureDataDirectory()` 中的 JSON 相关部分
- 保留 `ensureDataDirectory()` 用于数据库文件所在目录的创建

**步骤 3.3 — 增量更新适配**
- `syncCurrentMessagesToSession()` → 调用 DB 增量更新
- `updateMessageInSession()` → 调用 `DatabaseService::updateMessage()`
- 消息状态/进度更新 → 直接调 DB 更新（避免先改内存再全量保存）

### Phase 4：统计功能实现

**步骤 4.1 — 创建 StatisticsService**
- 实现上述所有统计 SQL 查询
- 返回格式统一为 QVariantMap/QVariantList（方便 QML 消费）
- 添加缓存机制（统计数据 5 分钟内不重复查询）

**步骤 4.2 — 集成到 Application**
- 创建实例并注册为 QML 上下文属性
- QML 中可通过 `statisticsService.xxx()` 调用

### Phase 5：Application 初始化流程调整

**步骤 5.1 — 调整初始化顺序**

```
原有顺序:
  SettingsController → crashRecovery → markAppRunning → AutoSaveService → loadSessions → restoreThumbnails → checkAndAutoRetry

新顺序:
  SettingsController → DatabaseService.initialize()
    → crashRecovery → markAppRunning → AutoSaveService
    → loadSessions (from DB) → restoreThumbnails (from DB)
    → preloadThumbnailsToLRU (将近期缩略图预加载到 L1)
    → checkAndAutoRetry → QML load
```

**步骤 5.2 — 析构顺序调整**
- `~Application()` 中确保 `DatabaseService` 最后关闭
- 关闭前 flush 所有待写入的缩略图

### Phase 6：构建验证与测试

**步骤 6.1 — 编译验证**
- 确认 Qt6::Sql 正确链接
- 确认所有新文件编译通过
- 确认无废弃 API 警告

**步骤 6.2 — 功能验证**
- [ ] 创建会话 → 关闭应用 → 重新打开 → 会话存在
- [ ] 添加消息（Shader/AI） → 处理完成 → 缩略图显示正常
- [ ] 重启应用 → 缩略图直接从 DB 加载（无灰图闪烁）
- [ ] 切换会话 → 消息正确加载
- [ ] 删除会话 → 级联删除消息/媒体文件/缩略图
- [ ] 批量操作（多选删除）→ 事务正确
- [ ] 自动保存 → 数据一致
- [ ] 崩溃恢复 → WAL 日志回滚正确

**步骤 6.3 — 性能验证**
- [ ] 500 个会话 × 每会话 50 条消息的加载时间 < 2秒
- [ ] 缩略图 L1 命中率 > 90%
- [ ] LRU 淘汰后内存稳定在 200MB 以内
- [ ] 并发读写无死锁

---

## 七、注意事项与风险缓解

### 7.1 向后兼容
- 首次启动检测旧 `sessions.json` 文件是否存在，若存在则执行一次性迁移
- 迁移脚本：读取 JSON → 逐条 INSERT 到 SQLite → 迁移成功后备份并删除 JSON
- `app_metadata.version` 用于标识数据版本，便于未来 Schema 变更

### 7.2 线程安全
- SQLite WAL 模式允许多个读连接 + 1 个写连接
- 主线程负责写操作（通过 DatabaseService）
- 缩略图读取可在工作线程进行（只读 SELECT）
- 使用 `QMutex` 保护 LRU 缓存的并发访问

### 7.3 数据库文件位置
- 默认路径：`{customDataPath}/enhancevision.db`（与原 data 目录同级）
- 通过 `SettingsController::effectiveDataPath()` 获取基础目录
- 若 customDataPath 为空则使用 `QStandardPaths::AppDataLocation`

### 7.4 缩略图 BLOB 大小控制
- 存储统一尺寸：256×256，Format_ARGB32_Premultiplied（约 256KB/张）
- 1000 张 ≈ 250MB（可接受范围）
- 可配置压缩质量（如 JPEG 压缩到 ~20KB/张，但增加 CPU 开销）
- 初期建议不压缩，后续根据实际体积决定

### 7.5 不迁移的数据
- **SettingsController/QSettings**：保持不变，INI 格式适合键值对配置
- **ProcessingModel（任务队列）**：运行时内存数据，不需要持久化
- **FrameCache**：视频帧缓存，纯运行时数据

---

## 八、级联删除与数据一致性保障（关键）

> **用户核心需求 1**：确保会话标签清空/删除操作时，数据库中对应的数据也被正确清理。

### 8.1 当前清理操作的完整映射

以下表格列出了所有需要适配的清理操作及其对应的数据库变更：

| 操作 | 当前实现位置 | 当前行为 | 迁移后需增加的 DB 操作 |
|------|------------|---------|---------------------|
| `deleteSession(id)` | SessionController:239 | 删除磁盘 result 文件 → 删除 SessionModel 记录 | **+** `DELETE FROM sessions WHERE id=?` （CASCADE 自动删 messages→media_files→thumbnails） |
| `clearSession(id)` | SessionController:275 | 删除磁盘 result 文件 → 清空 SessionModel 消息列表 | **+** `DELETE FROM messages WHERE session_id=?` + `DELETE FROM pending_files WHERE session_id=?` （CASCADE 自动删 media_files→thumbnails） |
| `deleteSelectedSessions()` | SessionController:315 | 循环调用 deleteSessionMediaFiles + deleteSession | 同上，批量事务执行 |
| `clearAllSessionMessages()` | SessionController:1399 | 遍历所有会话清空消息 | **+** `DELETE FROM messages` + `DELETE FROM pending_files` + 清理孤立 thumbnails |
| `clearAllSessionMessagesByMode(mode)` | SessionController:1427 | 按模式过滤消息后移除 | **+** `DELETE FROM messages WHERE mode=?` + 清理关联 thumbnails |
| `clearAllShaderVideoMessages()` | SessionController:1473 | 移除 Shader 模式的视频消息 | **+** 复杂 JOIN DELETE + 清理关联 thumbnails |
| `clearMediaFilesByModeAndType(mode,type)` | SessionController:1528 | 按模式和类型移除媒体文件 | **+** 多表联合 DELETE + 清理关联 thumbnails |
| `deleteAllSessions()` | SessionController:1606 | 全量删除所有会话和磁盘文件 | **+** `DELETE FROM sessions` （CASCADE 清空全部）或直接 `VACUUM` |
| `deleteSessionMediaFiles(session)` | SessionController:1649 | 只删除磁盘上的 resultPath 文件 | 无需改，但需确认 DB 中 status/result_path 也更新 |

### 8.2 外键 CASCADE 策略详解

建表中已定义的外键级联规则：

```
sessions ──DELETE CASCADE──▶ messages ──DELETE CASCADE──▶ media_files ──DELETE CASCADE──▶ thumbnails
   │                            │
   └──DELETE CASCADE──▶ pending_files    ◀──DELETE SET NULL──┘ (media_files.pending_file_id)
```

**具体效果**：
```sql
-- 执行 DELETE FROM sessions WHERE id = 'xxx'
-- 自动触发：
--   1. DELETE FROM messages WHERE session_id = 'xxx'
--   2. 对每条被删 message：DELETE FROM media_files WHERE message_id = ?
--   3. 对每个被删 media_file：DELETE FROM thumbnails WHERE media_file_id = ?
--   4. DELETE FROM pending_files WHERE session_id = 'xxx'
--   5. media_files 中引用被删 pending_file 的记录：pending_file_id = NULL
```

**关键前提**：必须在每次连接/操作前执行 `PRAGMA foreign_keys = ON;`

### 8.3 DatabaseService 新增清理接口

```cpp
class DatabaseService {
public:
    // ===== 会话级清理 =====
    
    /** 删除单个会话（含级联） */
    bool deleteSession(const QString& sessionId);
    
    /** 清空会话的消息和待处理文件（保留会话标签本身） */
    bool clearSessionData(const QString& sessionId);
    
    /** 批量删除多个会话（事务内） */
    bool deleteSessions(const QStringList& sessionIds);
    
    // ===== 按条件清理 =====
    
    /** 按处理模式清除所有会话中的消息 */
    int clearAllMessagesByMode(int mode);
    
    /** 清除 Shader 模式下所有视频消息 */
    int clearAllShaderVideoMessages();
    
    /** 按模式+媒体类型清除媒体文件（返回被影响的行数） */
    int clearMediaFilesByModeAndType(int mode, int mediaType);
    
    /** 清空所有消息（保留会话标签） */
    int clearAllMessages();
    
    /** 删除全部数据（相当于重置数据库，慎用） */
    bool deleteAllData();

    // ===== 缩略图专项清理 =====
    
    /** 清除指定路径前缀的所有缩略图（用于缓存管理） */
    int clearThumbnailsByPathPrefix(const QString& pathPrefix);
    
    /** 清除所有缩略图 */
    int clearAllThumbnails();
    
    /** 清除孤立的缩略图（media_file_id 已不存在的记录） */
    int cleanupOrphanedThumbnails();
    
    // ===== 统计查询（供缓存管理面板使用） =====
    
    /** 获取数据库文件大小 */
    qint64 databaseFileSize() const;
    
    /** 获取缩略图总数量 */
    int thumbnailCount() const;
    
    /** 获取缩略图 BLOB 总大小 */
    qint64 totalThumbnailSize() const;
    
    /** 获取各表的行数统计 */
    QVariantMap getTableStats() const;
};
```

### 8.4 SessionController 改造后的清理方法示例

以 `deleteSession()` 为例展示改造后的完整流程：

```cpp
void SessionController::deleteSession(const QString& sessionId)
{
    QString activeId = m_sessionModel->activeSessionId();
    bool isActiveSession = (sessionId == activeId);
    
    // 1. 取消该会话的处理任务（保持不变）
    if (m_processingController) {
        m_processingController->cancelSessionTasks(sessionId);
    }
    
    // 2. 删除磁盘上的 result 文件（保持不变）
    Session* session = getSession(sessionId);
    if (session) {
        int deletedCount = deleteSessionMediaFiles(*session);  // 仅删磁盘文件
        qInfo() << "[SessionController] Deleted" << deletedCount << "disk files for session:" << sessionId;
    }
    
    // 3. 【新增】从数据库中删除会话（CASCADE 自动清理 messages/media_files/thumbnails/pending_files）
    if (m_dbService) {
        m_dbService->deleteSession(sessionId);
    }
    
    // 4. 从内存模型中删除（保持不变）
    m_sessionModel->deleteSession(sessionId);
    rebuildSessionMessageIndex();
    
    // 5. 处理活动会话切换（保持不变）
    if (isActiveSession) { /* ... */ }
    
    emit sessionDeleted(sessionId);
    emit sessionCountChanged();
}
```

以 `clearMediaFilesByModeAndType()` 为例展示复杂清理：

```cpp
void SessionController::clearMediaFilesByModeAndType(int mode, int mediaType)
{
    ProcessingMode targetMode = static_cast<ProcessingMode>(mode);
    MediaType targetMediaType = static_cast<MediaType>(mediaType);
    
    // 1. 【新增】先从数据库按条件删除媒体文件（事务内）
    if (m_dbService) {
        m_dbService->beginTransaction();
        
        // 找出符合条件的 media_file IDs
        QList<QString> affectedMessageIds = m_dbService->findMessageIdsByModeAndType(mode, mediaType);
        
        // 删除这些媒体文件（关联的 thumbnails 由 CASCADE 自动清理）
        for (const QString& msgId : affectedMessageIds) {
            m_dbService->deleteMediaFilesByModeAndType(msgId, mediaType);
        }
        
        // 检查是否有消息因失去所有媒体文件而变为空，若是则也删除消息
        for (const QString& msgId : affectedMessageIds) {
            if (m_dbService->getMessageMediaFileCount(msgId) == 0) {
                m_dbService->deleteMessage(msgId);
            }
        }
        
        m_dbService->commitTransaction();
    }
    
    // 2. 更新内存模型（保持原有逻辑，但从 DB 加载最新状态）
    // ... 后续逻辑同原实现 ...
}
```

---

## 九、缓存管理功能适配（关键）

> **用户核心需求 2**：确保设置中的「缓存管理」功能真实反映并清理相关数据。

### 9.1 当前缓存管理的完整数据流

```
用户点击 "清除 AI 图像缓存"
    │
    ▼
SettingsController::clearAIImageData()
    ├── ① ThumbnailProvider::clearThumbnailsByPathPrefix(aiImagePath)  ← 清内存缓存
    ├── ② clearDirectory(aiImagePath)                                  ← 清磁盘目录
    └── ③ SessionController::clearMediaFilesByModeAndType(AI, Image) ← 清消息/媒体文件记录
         │
         ▼ (迁移后)
    DatabaseService 操作:
         ├── DELETE FROM media_files WHERE ... (mode=AI, type=Image)
         ├── DELETE FROM 变空的消息
         └── DELETE FROM 关联的 thumbnails (CASCADE)
```

### 9.2 SettingsController 改造要点

**新增属性**（反映数据库占用情况）：

```cpp
// SettingsController.h 新增
Q_PROPERTY(qint64 databaseSize READ databaseSize NOTIFY dataSizeChanged)
Q_PROPERTY(qint64 thumbnailDbSize READ thumbnailDbSize NOTIFY dataSizeChanged)
Q_PROPERTY(int thumbnailDbCount READ thumbnailDbCount NOTIFY dataSizeChanged)
```

**改造 `refreshDataSize()` 方法**：

```cpp
void SettingsController::refreshDataSize()
{
    // 原有：磁盘目录大小计算（保持不变）
    m_aiImageSize = calculateDirectorySize(getAIImagePath());
    m_aiVideoSize = calculateDirectorySize(getAIVideoPath());
    m_shaderImageSize = calculateDirectorySize(getShaderImagePath());
    m_shaderVideoSize = calculateDirectorySize(getShaderVideoPath());
    m_logSize = calculateDirectorySize(getLogPath());
    
    // 原有：磁盘文件计数（保持不变）
    m_aiImageFileCount = countFilesInDirectory(getAIImagePath());
    m_aIVideoFileCount = countFilesInDirectory(getAIVideoPath());
    m_shaderImageFileCount = countFilesInDirectory(getShaderImagePath());
    m_shaderVideoFileCount = countFilesInDirectory(getShaderVideoPath());
    
    // 【新增】数据库统计信息
    if (m_dbService) {
        m_databaseSize = m_dbService->databaseFileSize();       // enhancevision.db 文件大小
        m_thumbnailDbSize = m_dbService->totalThumbnailSize();   // BLOB 总大小
        m_thumbnailDbCount = m_dbService->thumbnailCount();      // 缩略图条数
    }
    
    emit dataSizeChanged();
}
```

**改造各 `clear*Data()` 方法** — 以 `clearAIImageData()` 为例：

```cpp
bool SettingsController::clearAIImageData()
{
    QString path = getAIImagePath();
    
    // ① 清 L1 内存缓存（保持不变）
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    // ② 清 L2 数据库缩略图（【新增】）
    if (m_dbService) {
        m_dbService->clearThumbnailsByPathPrefix(path);
    }
    
    // ③ 清磁盘文件（保持不变）
    bool success = clearDirectory(path);
    
    // ④ 清 DB 中的消息/媒体文件记录（改造为 DB 操作）
    if (success && m_sessionController) {
        m_sessionController->clearMediaFilesByModeAndType(1, 0);  // 内部改为调 DB
    }
    
    // ⑤ 刷新统计（自动包含 DB 大小变化）
    refreshDataSize();
    
    return success;
}
```

**改造 `clearAllCache()` 方法**：

```cpp
bool SettingsController::clearAllCache()
{
    bool success = true;
    
    // 清各类磁盘缓存（保持不变）
    success &= clearDirectory(getAIImagePath());
    success &= clearDirectory(getAIVideoPath());
    success &= clearDirectory(getShaderImagePath());
    success &= clearDirectory(getShaderVideoPath());
    success &= clearDirectory(getLogPath());
    
    // 清内存缩略图缓存（保持不变）
    ThumbnailProvider* tp = ThumbnailProvider::instance();
    if (tp) tp->clearAll();
    
    // 【新增】清数据库缩略图
    if (m_dbService) {
        m_dbService->clearAllThumbnails();
        // 同时清理孤立数据（可选 VACUUM 回收空间）
        m_dbService->cleanupOrphanedThumbnails();
    }
    
    refreshDataSize();
    return success;
}
```

### 9.3 设置页面 UI 适配建议

在现有缓存管理面板中增加数据库信息显示：

```
┌─────────────────────────────────────────────┐
│  缓存管理                                    │
├─────────────────────────────────────────────┤
│  可清理: 1.23 GB                             │
│                                             │
│  ├─ AI 图像:     320 MB  (128 文件)  [清除] │
│  ├─ AI 视频:     450 MB  (32 文件)   [清除] │
│  ├─ Shader 图像: 180 MB  (256 文件)  [清除] │
│  ├─ Shader 视频: 95 MB   (16 文件)   [清除] │
│  ├─ 日志:        12 MB   (8 文件)    [清除] │
│  │                                         │
│  ├─ 【新增】数据库: 45 MB                   │
│  │     └─ 缩略图: 28 MB (1,024 张)  [清除] │
│  │                                         │
│  └─ [一键清除全部]                           │
└─────────────────────────────────────────────┘
```

### 9.4 缓存清理完整矩阵

| 用户操作 | L1 内存缓存 | L2 DB 缩略图 | DB 消息/媒体文件 | 磁盘文件 | 效果验证方式 |
|---------|:-----------:|:-------------:|:----------------:|:--------:|:-----------:|
| 清除 AI 图像 | ✅ byPathPrefix | ✅ byPathPrefix | ✅ mode=AI+type=Img | ✅ aiImagePath | `refreshDataSize()` 四项均归零 |
| 清除 AI 视频 | ✅ byPathPrefix | ✅ byPathPrefix | ✅ mode=AI+type=Vid | ✅ aiVideoPath | 同上 |
| 清除 Shader 图像 | ✅ byPathPrefix | ✅ byPathPrefix | ✅ mode=Shd+type=Img | ✅ shaderImagePath | 同上 |
| 清除 Shader 视频 | ✅ byPathPrefix | ✅ byPathPrefix | ✅ mode=Shd+type=Vid | ✅ shaderVideoPath | 同上 |
| 清除日志 | N/A | N/A | N/A | ✅ logPath | logSize 归零 |
| 一键全清 | ✅ clearAll | ✅ clearAllThumb | ✅ 通过 SessionCtrl | ✅ 全部目录 | totalCacheSize 归零 |
| 删除某会话 | ✅ bySession | 🔄 CASCADE | 🔄 CASCADE | ✅ resultPaths | 会话消失，消息数为 0 |
| 清空某会话 | ✅ bySession | ✅ delBySession | ✅ delBySession | ✅ resultPaths | 消息数为 0，会话保留 |

---

## 十、实施步骤补充（基于八、九两节）

### Phase 3 补充 — SessionController 清理方法改造

**步骤 3.4 — 重写所有清理方法**

在步骤 3.1~3.3 完成基础 CRUD 迁移后，逐一改造以下方法：

1. `deleteSession()` — 加 `m_dbService->deleteSession(id)`
2. `clearSession()` — 加 `m_dbService->clearSessionData(id)`
3. `deleteSelectedSessions()` — 加 `m_dbService->deleteSessions(ids)`
4. `clearAllSessionMessages()` — 加 `m_dbService->clearAllMessages()`
5. `clearAllSessionMessagesByMode()` — 加 `m_dbService->clearAllMessagesByMode()`
6. `clearAllShaderVideoMessages()` — 加 `m_dbService->clearAllShaderVideoMessages()`
7. `clearMediaFilesByModeAndType()` — 加 `m_dbService->clearMediaFilesByModeAndType()`
8. `deleteAllSessions()` — 加 `m_dbService->deleteAllData()`

每个方法的改造遵循统一模式：**先操作 DB（事务），再操作内存模型，最后通知 UI**。

### Phase 5 补充 — SettingsController 缓存管理适配

**步骤 5.3 — SettingsController 注入 DatabaseService**
- 添加 `setDatabaseService(DatabaseService*)` 方法
- 在 Application::setupQmlContext() 中完成注入

**步骤 5.4 — 改造 refreshDataSize()**
- 增加 DB 统计信息的获取和暴露

**步骤 5.5 — 改造 6 个 clear*Data() 方法**
- 每个方法增加 DB 缩略图清理步骤
- 确保 `refreshDataSize()` 能反映真实的 DB 占用变化

### Phase 6 补充 — 验证用例扩展

**步骤 6.4 — 级联删除验证**
- [ ] 删除一个包含 10 条消息的会话 → DB 中 sessions/messages/media_files/thumbnails 行数均正确减少
- [ ] 清空会话 A 的消息 → 会话 A 仍在，但其 messages 表记录为 0
- [ ] 清除 AI 图像缓存 → AI 图像目录为空 + DB 中对应 mode=AI/type=Image 的 media_files 被删除 + thumbnails 被清理
- [ ] 一键清除全部缓存 → 所有磁盘目录为空 + DB 中 thumbnails 表为空 + messages 表为空
- [ ] 清除后再刷新设置页面 → 各项数值均为 0

**步骤 6.5 — 数据一致性边界测试**
- [ ] 删除会话过程中应用崩溃 → WAL 回滚保证数据一致（不会出现孤儿记录）
- [ ] 并发清理 + 自动保存 → 无死锁、无数据损坏
- [ ] 清理后立即创建新会话/新消息 → 正常工作，无残留脏数据影响

---

## 十一、应用更新与数据迁移策略

> **用户核心需求 1**：应用更新安装后，一切正常工作，数据不丢失。

### 11.1 数据目录布局设计

采用标准化的可移植数据目录结构：

```
{UserDataDir}/                          ← 用户数据根目录（可配置/可迁移）
├── enhancevision.db                     ← SQLite 主数据库（含所有业务数据）
├── enhancevision.db-wal                 ← WAL 日志文件（运行时自动生成）
├── enhancevision.db-shm                 ← WAL 共享内存文件
├── enhancevision.db.bak                 ← 自动备份（每次 Schema 变更前）
│
├── ai_images/                           ← AI 处理结果图片
├── ai_videos/                           ← AI 处理结果视频
├── shader_images/                       ← Shader 处理结果图片
├── shader_videos/                       ← Shader 处理结果视频
├── logs/                                ← 应用日志
│
└── backups/                             ← 用户手动备份目录（需求 3）
    ├── backup_2026-04-02_143022/
    │   ├── enhancevision.db
    │   ├── ai_images/
    │   ├── ai_videos/
    │   ├── shader_images/
    │   ├── shader_videos/
    │   └── logs/
    └── backup_2026-05-15_091203/
        └── ...
```

**关键原则**：
- **数据与程序分离**：所有用户数据存放在 `QStandardPaths::AppDataLocation` 或自定义路径下，不在安装目录中
- **更新安全**：覆盖安装程序文件不会触碰任何用户数据

### 11.2 数据库版本管理与自动迁移

```sql
-- app_metadata 表中的 version 字段作为 Schema 版本号
-- 初始版本 = 2（从 JSON 迁移后的版本）

-- 版本变更历史表：
-- v1: JSON 文件时代（已废弃，仅用于首次迁移识别）
-- v2: 首次 SQLite 迁移版本（当前初始版本）
-- v3+: 未来 Schema 变更预留
```

**DatabaseService 中的迁移框架**：

```cpp
class DatabaseService {
private:
    static constexpr int CURRENT_SCHEMA_VERSION = 2;
    
    /** 当前数据库的 schema 版本 */
    int m_schemaVersion = 0;
    
    /**
     * @brief 检查并执行 Schema 迁移
     * 在 initialize() 中调用，确保数据库结构与代码一致
     */
    void checkAndMigrate();
    
    /**
     * @brief 执行从 fromVersion 到 toVersion 的增量迁移
     * @param fromVersion 当前 DB 版本
     * @param toVersion 目标版本（CURRENT_SCHEMA_VERSION）
     */
    bool migrateSchema(int fromVersion, int toVersion);
    
    // 各版本的迁移函数
    bool migrateV1toV2();   // JSON → SQLite（一次性）
    bool migrateV2toV3();   // 未来：如新增字段/表
    bool migrateV3toV4();   // 预留
};
```

**initialize() 完整流程**：

```cpp
void DatabaseService::initialize()
{
    QString dbPath = resolveDatabasePath();
    
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "enhancevision_conn");
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        qCritical() << "[DB] Failed to open database:" << db.lastError().text();
        return;
    }
    
    // 启用 WAL 和外键
    execPragma("PRAGMA foreign_keys = ON");
    execPragma("PRAGMA journal_mode = WAL");
    execPragma("PRAGMA synchronous = NORMAL");
    
    // 创建表（IF NOT EXISTS 保证幂等）
    createTables();
    
    // 检查版本并执行迁移
    checkAndMigrate();
    
    m_initialized = true;
}

void DatabaseService::checkAndMigrate()
{
    int dbVersion = getMetadataInt("version", 1);  // 默认 v1（JSON 时代或全新）
    
    if (dbVersion < CURRENT_SCHEMA_VERSION) {
        qInfo() << "[DB] Migrating schema from v" << dbVersion << "to v" << CURRENT_SCHEMA_VERSION;
        
        // 迁移前自动备份
        createBackupBeforeMigration(dbVersion);
        
        // 逐步迁移
        for (int v = dbVersion; v < CURRENT_SCHEMA_VERSION; ++v) {
            bool ok = runMigrationStep(v, v + 1);
            if (!ok) {
                qCritical() << "[DB] Migration failed at step v" << v << "->v" << (v+1);
                // 回滚到备份
                restoreFromBackup();
                break;
            }
        }
        
        setMetadataInt("version", CURRENT_SCHEMA_VERSION);
    } else if (dbVersion > CURRENT_SCHEMA_VERSION) {
        qWarning() << "[DB] Database version" << dbVersion 
                   << "is newer than application version" << CURRENT_SCHEMA_VERSION;
        // 用户可能降级了应用版本，谨慎处理
    }
}
```

### 11.3 应用更新的完整场景

| 场景 | 数据状态 | 处理方式 |
|------|---------|---------|
| **正常更新** (v1.0→v1.1, 无 Schema 变更) | DB 存在，版本匹配 | 直接打开，无需操作 |
| **带 Schema 变更的更新** (v1.0→v2.0) | DB 存在，版本较低 | `checkAndMigrate()` 自动执行增量迁移 |
| **全新安装** | 无 DB 文件 | `createTables()` 创建新库，version=2 |
| **从 JSON 版本升级** | 有 `sessions.json`，无 DB | `migrateV1toV2()` 读取 JSON 并导入 |
| **降级使用** (v2.0→v1.0) | DB 版本高于应用 | 警告但不阻止（只读模式或忽略新字段） |

### 11.4 更新后首次启动检测

在 Application::initialize() 中增加：

```cpp
// 检测是否为更新后首次启动
QString lastRunVersion = SettingsController::instance()->getSetting("system/lastAppVersion", "");
QString currentVersion = QApplication::applicationVersion();

if (lastRunVersion != currentVersion && !lastRunVersion.isEmpty()) {
    qInfo() << "[App] Version changed:" << lastRunVersion << "->" << currentVersion;
    
    // 执行更新后检查
    if (m_dbService) {
        m_dbService->checkAndMigrate();  // 确保兼容性
    }
    
    emit appUpdated(lastRunVersion, currentVersion);
}

SettingsController::instance()->setSetting("system/lastAppVersion", currentVersion);
```

---

## 十二、数据恢复功能

> **用户核心需求 2**：应用更新后支持用户选择文件夹恢复数据。

### 12.1 恢复服务设计

```cpp
// include/EnhanceVision/services/DataRestoreService.h
class DataRestoreService : public QObject {
    Q_OBJECT
    
public:
    explicit DataRestoreService(QObject* parent = nullptr);
    
    /**
     * @brief 检查指定目录是否包含有效的备份数据
     * @param path 待检查的目录路径
     * @return 检查结果（有效/无效/部分有效及详情）
     */
    Q_INVOKABLE QVariantMap validateBackupSource(const QString& path);
    
    /**
     * @brief 从指定目录恢复数据
     * @param sourcePath 源数据目录（用户选择的备份目录）
     * @return 是否成功
     * @note 恢复前会自动备份当前数据
     */
    Q_INVOKABLE bool restoreFromPath(const QString& sourcePath);
    
    /**
     * @brief 仅恢复数据库文件（不含媒体文件）
     */
    Q_INVOKABLE bool restoreDatabaseOnly(const QString& sourceDbPath);
    
    /**
     * @brief 获取可恢复的数据摘要信息
     * @param path 源目录
     * @return 包含会话数、消息数、缩略图数等的摘要
     */
    Q_INVOKABLE QVariantMap getRestorePreview(const QString& path);
    
signals:
    void restoreProgress(int percent, const QString& stage);
    void restoreCompleted(bool success, const QString& message);
    void restoreError(const QString& error);
    
private:
    bool copyDirectoryRecursive(const QString& src, const QString& dst,
                                std::function<void(int)> progressCallback);
    QVariantMap analyzeDataDirectory(const QString& path);
};
```

### 12.2 恢复流程

```
用户点击 "恢复数据"
    │
    ▼
打开文件夹选择对话框
    │
    ▼
validateBackupSource(selectedPath)
    │
    ├── 无效 → 提示 "所选目录不包含有效的备份数据"
    │
    └── 有效 → 显示 getRestorePreview() 摘要
         │
         │  ┌──────────────────────────────┐
         │  │  发现以下数据：               │
         │  │  • 会话: 23 个              │
         │  │  • 消息: 347 条             │
         │  │  • 缩略图: 892 张           │
         │  │  • AI 结果: 128 个文件      │
         │  │  • Shader 结果: 256 个文件  │
         │  │                              │
         │  │  ⚠️ 恢复将覆盖当前所有数据   │
         │  │  [取消]  [确认恢复]          │
         │  └──────────────────────────────┘
         │
         ▼ (用户确认)
    自动备份当前数据 → backups/pre_restore_{timestamp}/
         │
         ▼
    关闭数据库连接
         │
         ▼
    复制源文件到数据目录:
    ├── enhancevision.db          (30%)
    ├── ai_images/                (50%)
    ├── ai_videos/                (60%)
    ├── shader_images/            (80%)
    ├── shader_videos/            (90%)
    └── logs/                    (100%)
         │
         ▼
    重新打开数据库 → loadSessions()
         │
         ▼
    emit restoreCompleted(true)
```

### 12.3 恢复安全性保障

1. **恢复前强制备份**：将当前数据整体复制到 `backups/pre_restore_{timestamp}/`
2. **原子性替换**：先复制到临时位置，全部完成后再替换正式文件
3. **数据库关闭**：恢复期间必须关闭 DB 连接，避免文件锁定冲突
4. **校验恢复后完整性**：重新打开 DB 后执行 `PRAGMA integrity_check`

---

## 十三、数据备份功能

> **用户核心需求 3**：支持用户数据备份。

### 13.1 备份服务设计

```cpp
// include/EnhanceVision/services/DataBackupService.h
class DataBackupService : public QObject {
    Q_OBJECT
    
public:
    explicit DataBackupService(QObject* parent = nullptr);
    
    /**
     * @brief 创建完整备份
     * @param targetDir 目标备份目录（为空则自动生成到 backups/ 下）
     * @return 备份路径，失败返回空字符串
     */
    Q_INVOKABLE QString createFullBackup(const QString& targetDir = QString());
    
    /**
     * @brief 创建快速备份（仅备份数据库，不含大文件）
     * 适用于频繁自动备份场景
     */
    Q_INVOKABLE QString createQuickBackup();
    
    /**
     * @brief 获取备份列表
     * @return 备份信息列表（路径、大小、时间、包含内容摘要）
     */
    Q_INVOKABLE QVariantList getBackupList();
    
    /**
     * @brief 删除指定备份
     */
    Q_INVOKABLE bool deleteBackup(const QString& backupPath);
    
    /**
     * @brief 设置自动备份策略
     * @param intervalHours 间隔小时数（0=禁用）
     * @param maxBackups 最大保留备份数量
     */
    Q_INVOKABLE void setAutoBackupPolicy(int intervalHours, int maxBackups = 10);
    
    /**
     * @brief 导出数据为可移植包（zip 格式）
     * @param targetPath 输出 zip 文件路径
     */
    Q_INVOKABLE bool exportAsZip(const QString& targetPath);
    
signals:
    void backupProgress(int percent, const QString& stage);
    void backupCompleted(const QString& backupPath, bool success);
    void backupError(const QString& error);
    
private:
    QTimer* m_autoBackupTimer = nullptr;
    int m_autoBackupIntervalHours = 0;
    int m_maxAutoBackups = 10;
    
    QString generateBackupName() const;
    bool compressBackup(const QString& srcDir, const QString& zipPath);
    void cleanupOldBackups();
};
```

### 13.2 备份实现要点

**完整备份流程**：

```cpp
QString DataBackupService::createFullBackup(const QString& targetDir)
{
    QString backupDir = targetDir.isEmpty()
        ? mDataDir + "/backups/backup_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss")
        : targetDir;
    
    QDir().mkpath(backupDir);
    
    emit backupProgress(0, "preparing");
    
    // 1. 复制数据库（需先关闭 WAL，或使用 SQLite Backup API）
    emit backupProgress(5, "database");
    QFile::copy(mDbPath, backupDir + "/enhancevision.db");
    
    // 2. 复制各数据目录
    struct CopyTask { QString src; QString dst; QString label; };
    QVector<CopyTask> tasks = {
        { mAiImagePath,   backupDir + "/ai_images",   "ai_images" },
        { mAIVideoPath,  backupDir + "/ai_videos",    "ai_videos" },
        { mShaderImagePath, backupDir + "/shader_images", "shader_images" },
        { mShaderVideoPath, backupDir + "/shader_videos", "shader_videos" },
        { mLogPath,       backupDir + "/logs",         "logs" },
    };
    
    int perTask = 90 / tasks.size();
    for (int i = 0; i < tasks.size(); ++i) {
        emit backupProgress(5 + perTask * i, tasks[i].label);
        copyDirectoryRecursive(tasks[i].src, tasks[i].dst);
    }
    
    // 3. 写入备份元信息
    emit backupProgress(95, "metadata");
    writeBackupMetadata(backupDir);  // 记录版本号、时间、文件清单等
    
    emit backupProgress(100, "done");
    
    // 4. 清理旧备份
    cleanupOldBackups();
    
    emit backupCompleted(backupDir, true);
    return backupDir;
}
```

**SQLite 在线备份（零停机）**：

```cpp
// 使用 SQLite Online Backup API，可在数据库使用中完成备份
bool DatabaseService::onlineBackup(const QString& backupPath)
{
    sqlite3* srcDb = ...;   // 当前数据库连接
    sqlite3* backupDb = ...; // 新建的备份文件连接
    
    sqlite3_backup* pBackup = sqlite3_backup_init(backupDb, "main", srcDb, "main");
    if (!pBackup) return false;
    
    int rc;
    do {
        rc = sqlite3_backup_step(pBackup, 100);  // 每步拷贝 100 页
        // 可在此处 yield 给 UI 线程更新进度
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);
    
    sqlite3_backup_finish(pBackup);
    return rc == SQLITE_DONE;
}
```

### 13.3 自动备份策略

```cpp
void DataBackupService::setAutoBackupPolicy(int intervalHours, int maxBackups)
{
    m_autoBackupIntervalHours = intervalHours;
    m_maxAutoBackups = maxBackups;
    
    if (!m_autoBackupTimer) {
        m_autoBackupTimer = new QTimer(this);
        connect(m_autoBackupTimer, &QTimer::timeout, this, [this]() {
            // 仅在空闲时自动备份（没有正在进行的处理任务）
            if (!m_isProcessingActive) {
                createQuickBackup();
            }
        });
    }
    
    if (intervalHours > 0) {
        m_autoBackupTimer->start(intervalHours * 3600 * 1000);
    } else {
        m_autoBackupTimer->stop();
    }
}
```

### 13.4 设置页面 UI 集成建议

在设置页面新增「数据管理」区域：

```
┌─────────────────────────────────────────────────┐
│  数据管理                                        │
├─────────────────────────────────────────────────┤
│                                                 │
│  ┌───────────────────────────────────────────┐  │
│  │  备份                                     │  │
│  │  最近备份: 2026-04-02 14:30 (123 MB)     │  │
│  │  [立即完整备份]  [快速备份(仅DB)]          │  │
│  │  自动备份: [✓] 每 24 小时 保留最近 5 份   │  │
│  │  [导出为 ZIP]                              │  │
│  └───────────────────────────────────────────┘  │
│                                                 │
│  ┌───────────────────────────────────────────┐  │
│  │  恢复                                     │  │
│  │  从备份恢复数据...                         │  │
│  │  ⚠️ 恢复将覆盖当前数据，恢复前会自动备份    │  │
│  └───────────────────────────────────────────┘  │
│                                                 │
│  ┌───────────────────────────────────────────┐  │
│  │  备份历史                                  │  │
│  │  📦 backup_2026-04-02_143022  123 MB  [删除]│  │
│  │  📦 backup_2026-03-28_091203  98 MB   [删除]│  │
│  │  📦 pre_restore_2026-04-01_200000 145 MB  │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

---

## 十四、实施步骤补充（基于十一~十三节）

### Phase 7 — 数据安全与服务层实现

**步骤 7.1 — 创建 DataBackupService**
- 实现 `createFullBackup()` / `createQuickBackup()`
- 实现 SQLite Online Backup API 集成
- 实现自动备份定时器
- 实现备份元信息写入和读取

**步骤 7.2 — 创建 DataRestoreService**
- 实现 `validateBackupSource()` 校验逻辑
- 实现 `restoreFromPath()` 完整恢复流程
- 实现 `getRestorePreview()` 数据预览
- 实现恢复前的自动备份保护

**步骤 7.3 — DatabaseService 增加迁移能力**
- 实现 `checkAndMigrate()` 版本检测框架
- 实现 `migrateV1toV2()` JSON→SQLite 一次性迁移
- 实现 `createBackupBeforeMigration()` / `restoreFromBackup()`
- 在 `initialize()` 中集成迁移调用

**步骤 7.4 — SettingsPage UI 扩展**
- 新增「数据管理」区域（备份/恢复/历史）
- 新增文件夹选择对话框（用于恢复）
- 新增备份进度显示

### Phase 8 补充 — 验证用例扩展

**步骤 8.1 — 更新迁移验证**
- [ ] 从 v1(JSON) 升级到 v2(SQLite) → 所有会话/消息/缩略图正确迁移
- [ ] Schema 变更升级(v2→v3模拟) → 自动备份 → 迁移成功 → 数据完整
- [ ] 降级使用(v2→v1模拟) → 不崩溃，新字段被忽略
- [ ] 全新安装 → 空 DB 正常创建，version=2

**步骤 8.2 — 备份恢复验证**
- [ ] 创建完整备份 → 备份目录结构正确 → 元信息文件存在
- [ ] 删除部分数据 → 从备份恢复 → 数据完全还原
- [ ] 恢复前自动备份(pre_restore_) → 可回滚到恢复前状态
- [ ] 恢复无效目录 → validateBackupSource 返回错误 → 不执行恢复
- [ ] 自动备份按计划触发 → 备份数量不超过上限 → 旧备份自动清理
- [ ] 导出为 ZIP → ZIP 文件可解压且内容完整
