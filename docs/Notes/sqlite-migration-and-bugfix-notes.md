# SQLite 数据库迁移 & insertMessage Bug 修复笔记

## 概述

将 EnhanceVision 项目从 JSON 文件存储迁移到 SQLite 数据库，并修复了迁移过程中发现的 `insertMessage` "Parameter count mismatch" 关键 Bug。

**创建日期**: 2026-04-02
**作者**: AI Assistant
**相关计划**: `.trae/documents/sqlite-migration-plan.md`

---

## 一、变更概述

### 1. 新增文件

| 文件路径 | 功能描述 |
|----------|----------|
| `include/EnhanceVision/services/DatabaseService.h` | 核心数据库服务头文件（CRUD、事务、迁移） |
| `src/services/DatabaseService.cpp` | 核心数据库服务实现（线程安全 threadDb()） |
| `include/EnhanceVision/services/ThumbnailCacheService.h` | LRU 内存缓存（L1 层）头文件 |
| `src/services/ThumbnailCacheService.cpp` | LRU 缓存实现（maxCount=500, 200MB） |
| `include/EnhanceVision/services/StatisticsService.h` | 统计查询服务头文件 |
| `src/services/StatisticsService.cpp` | 统计查询实现（处理统计、模型使用等） |
| `include/EnhanceVision/services/DataBackupService.h` | 数据备份服务头文件 |
| `src/services/DataBackupService.cpp` | 备份服务实现（全量/快速/自动/ZIP） |
| `include/EnhanceVision/services/DataRestoreService.h` | 数据恢复服务头文件 |
| `src/services/DataRestoreService.cpp` | 恢复服务实现（验证/恢复/预览） |

### 2. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `CMakeLists.txt` | 添加 Qt6::Sql 依赖 + 9 个新源文件 |
| `include/EnhanceVision/controllers/SessionController.h` | 添加 DatabaseService 集成接口 |
| `src/controllers/SessionController.cpp` | 重写 saveSessions/loadSessions 为 DB 路径 + 旧版回退 |
| `include/EnhanceVision/providers/ThumbnailProvider.h` | 添加缓存服务和数据库服务注入 |
| `src/providers/ThumbnailProvider.cpp` | 实现三层缓存查找：L1 LRU → L2 DB → L3 生成 |
| `src/app/Application.cpp` | 初始化 DatabaseService 单例并注入各组件 |
| `.gitignore` | 更新忽略规则 |
| `docs/ApplicationData/StorageDirectory.md` | 更新存储目录说明 |

---

## 二、实现的功能特性

- ✅ **SQLite 数据库核心服务**：完整 CRUD 操作、事务支持、WAL 模式
- ✅ **线程安全数据库访问**：threadDb() 方法为每个线程创建独立连接
- ✅ **双层缩略图缓存**：L1 LRU 内存缓存 + L2 SQLite BLOB 存储
- ✅ **三层缩略图查找**：内存 → 数据库 → 异步生成
- ✅ **级联删除**：外键 CASCADE 自动清理关联数据
- ✅ **JSON→SQLite 一键迁移**：V1→V2 迁移带备份保护
- ✅ **数据备份/恢复**：全量备份、快速备份、自动定时备份
- ✅ **统计服务**：处理统计、模型使用分布、媒体类型分析
- ✅ **旧版 JSON 回退**：DB 不可用时自动降级到原始 JSON 存储
- ✅ **Bug 修复**：insertMessage VALUES 占位符数量不匹配问题

---

## 三、技术实现细节

### 3.1 线程安全设计（threadDb）

```cpp
QSqlDatabase DatabaseService::threadDb() const
{
    const QString baseConn = "enhancevision_conn";
    const QString threadConn = baseConn + "_" + QString::number(
        reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (!QSqlDatabase::contains(threadConn)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", threadConn);
        db.setDatabaseName(m_dbPath);
        if (!db.open()) { return m_db; }
        db.exec("PRAGMA foreign_keys = ON");
        db.exec("PRAGMA synchronous = NORMAL");
    }
    return QSqlDatabase::database(threadConn);
}
```

**设计决策**：
- 使用 `QThread::currentThreadId()` 作为连接名后缀确保每线程唯一连接
- 所有 `QSqlQuery query(m_db)` 改为 `QSqlQuery query(threadDb())`
- 方法标记为 `const`（不修改成员变量，仅操作全局 QSqlDatabase 注册表）
- 连接创建时自动设置 PRAGMA 优化参数

### 3.2 数据库 Schema（6 张表）

| 表名 | 用途 | 关键字段 |
|------|------|----------|
| `sessions` | 会话元数据 | id, title, created_at, updated_at, tag |
| `messages` | 消息记录 | id, session_id, mode, status, ai_params... (16列) |
| `media_files` | 媒体文件 | id, message_id, source_path, output_path, type |
| `pending_files` | 待处理队列 | id, session_id, source_path, mode |
| `thumbnails` | 缩略图 BLOB | path, thumbnail_data, last_accessed |
| `app_metadata` | 应用元数据 | key, value（schema_version 等） |

### 3.3 三层缩略图缓存架构

```
请求缩略图 → [L1] ThumbnailCacheService (LRU, 500项, 200MB)
           ↓ 未命中
           → [L2] DatabaseService.loadThumbnail (SQLite BLOB)
           ↓ 未命中
           → [L3] 异步生成 (AIEngine/ShaderProcessor)
           ↓ 生成完成
           → 同时写入 L1 和 L2
```

---

## 四、遇到的问题及解决方案

### 问题 1："Failed to save session" — Parameter count mismatch（关键 Bug）

**现象**：用户使用 AI 推理模式处理多媒体文件时，弹出 "Failed to save session: ac144981-..." 错误提示。

**排查过程**：

1. **初步假设**：QSqlDatabase 跨线程不安全（m_db 在主线程创建，saveSessions 通过 QtConcurrent::run 在工作线程调用）
2. **第一轮修复**：实现 `threadDb()` 方法创建每线程独立连接
3. **编译错误**：`threadDb()` 被 `const` 方法调用但未标记 const → 添加 const 限定符
4. **错误仍存在**！添加详细诊断日志后发现真正根因

**根本原因**：

```
messages 表定义：16 列（id → ai_model_params）
INSERT 列名列表： 16 列 ✅
VALUES ? 占位符： 15 个 ❌ ← 缺少最后一个！
addBindValue 调用： 16 次 ❌ ← 多绑定一个值
```

`insertMessage()` 的 SQL 语句中 VALUES 子句只有 **15 个 `?`**，但列名列表有 **16 列**（最后一个 `ai_model_params` 缺少对应占位符），而代码绑定了 **16 个值**。

**解决方案**：

```diff
- ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
+ ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
```

在 VALUES 子句中添加第 16 个 `?` 占位符。

**教训**：
- SQL INSERT 语句的列数和占位符数必须严格匹配
- 当添加新列时必须同步更新 VALUES 子句
- 使用详细诊断日志（绑定索引计数）可快速定位此类问题

### 问题 2：QSqlDatabase 线程安全问题

**现象**：通过 QtConcurrent::run 在工作线程执行 DB 操作时出现不稳定行为。

**原因**：Qt 文档明确说明——QSqlDatabase 连接只能在创建它的线程中使用。跨线程共享连接会导致未定义行为。

**解决方案**：实现 `threadDb()` 方法，使用线程 ID 作为连接名后缀，为每个线程创建独立的数据库连接。

### 问题 3：saveThumbnail FOREIGN KEY constraint failed

**现象**：日志中出现 `saveThumbnail failed: "FOREIGN KEY constraint failed"` 警告。

**原因**：缩略图保存时对应的 media_files 记录尚未插入数据库（时序问题）。ThumbnailProvider 在生成缩略图后立即保存到 DB，但 media_file 可能在后续才插入。

**状态**：非关键问题，不影响功能正确性（缩略图会在下次访问时重新生成并保存）。

---

## 五、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| AI 推理模式处理文件 | 会话正常保存，无错误弹窗 | ✅ 通过 |
| 多次连续 AI 推理 | 所有会话均保存成功 | ✅ 通过 |
| 应用程序启动加载 | 从 DB 正确加载历史会话 | ✅ 通过 |
| 编译构建 | 零错误零警告 | ✅ 通过 |

### 日志验证

修复后的运行日志确认：
- 所有 `insertMessage` 调用成功：`lastError: ""`
- 所有会话保存成功：`All sessions saved successfully`
- 无 "Failed to save session" 错误

---

## 六、性能影响

| 指标 | 变更前（JSON） | 变更后（SQLite） | 影响 |
|------|----------------|------------------|------|
| 会话保存速度 | 同步文件写入 | 事务批量写入 | ⬆️ 提升 |
| 会话加载速度 | 解析整个 JSON 文件 | 按需 SQL 查询 | ⬆️ 显著提升 |
| 缩略图持久化 | 仅内存（重启丢失） | DB BLOB 持久化 | ✅ 新增能力 |
| 搜索/过滤能力 | 全量遍历 | SQL INDEX 查询 | ⬆️ 显著提升 |

---

## 七、后续工作

- [ ] 优化 saveThumbnail 的外键约束时序问题
- [ ] 为高频查询路径添加数据库索引
- [ ] 实现 Settings 中缓存管理的真实 DB 数据展示
- [ ] 添加数据库连接池管理（避免线程频繁创建/销毁连接）

---

## 八、参考资料

- Qt SQL Module 文档：https://doc.qt.io/qt-6/sql-module.html
- SQLite WAL 模式：https://www.sqlite.org/wal.html
- Qt Thread-Support in SQL：QSqlDatabase 只能在创建线程中使用
