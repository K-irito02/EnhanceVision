# EnhanceVision 应用程序数据存储目录

本文档记录 EnhanceVision 应用程序的所有数据存储路径。

## 核心路径概念

### effectiveDataPath

`effectiveDataPath()` 是应用程序数据存储的核心路径方法，定义于 `SettingsController`：

- 若用户设置了自定义数据路径（`behavior/customDataPath`），则使用自定义路径
- 否则回退到 `QStandardPaths::AppLocalDataLocation`

以下目录均基于 `effectiveDataPath`：
- AI 处理结果（`ai/images`、`ai/videos`）
- Shader 处理结果（`shader/images`、`shader/videos`）
- 缩略图缓存（`thumbnails/`、`thumbnail_meta.db`）
- 任务恢复快照（`system/recovery_snapshot.json`）
- 日志文件（`logs/`）

---

## 用户数据目录

### Windows 系统路径说明

| Qt 标准路径 | Windows 实际路径 |
|------------|-----------------|
| `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision` |
| `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision` |
| `ConfigLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision` |
| `PicturesLocation` | `C:\Users\<用户名>\Pictures` |
| `DocumentsLocation` | `C:\Users\<用户名>\Documents` |

---

## 配置文件

### 会话数据 (sessions.json)

**存储路径：**
```
{AppDataLocation}/EnhanceVision/sessions.json
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json
```

**代码位置：** `SessionController::sessionsFilePath()`

**数据内容：**
- 所有会话的元数据（ID、名称、创建时间、修改时间）
- 每个会话中的消息列表
- 消息中的媒体文件引用
- AI 处理参数和 Shader 参数
- 会话置顶状态和排序索引

---

### 应用设置 (settings.ini)

**存储路径：**
```
{ConfigLocation}/settings.ini
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini
```

**代码位置：** `SettingsController::loadSettings()` / `SettingsController::saveSettings()`

**数据内容：**

| 设置项 | 键名 | 默认值 | 说明 |
|-------|------|-------|------|
| 主题 | `appearance/theme` | `dark` | 界面主题 |
| 语言 | `appearance/language` | `zh_CN` | 界面语言 |
| 侧边栏展开 | `sidebar/expanded` | `true` | 侧边栏是否展开 |
| 默认保存路径 | `behavior/defaultSavePath` | `Pictures/EnhanceVision` | 导出文件的默认路径 |
| 自动保存结果 | `behavior/autoSave` | `false` | 是否自动保存处理结果 |
| 自定义数据路径 | `behavior/customDataPath` | 空 | 自定义缓存存储路径 |
| 音量 | `audio/volume` | `80` | 播放音量 (0-100) |
| Shader 自动重处理 | `reprocess/shaderEnabled` | `true` | Shader 效果自动重新处理 |
| AI 自动重处理 | `reprocess/aiEnabled` | `true` | AI 效果自动重新处理 |
| 视频自动播放 | `video/autoPlay` | `true` | 视频加载后自动播放 |
| 切换时自动播放 | `video/autoPlayOnSwitch` | `true` | 切换消息时自动播放视频 |
| 恢复播放位置 | `video/restorePosition` | `true` | 切换消息时恢复播放位置 |
| 任务暂停模式 | `task/pauseMode` | `1` | 暂停模式（0=模式一，1=模式二） |
| 上次正常退出 | `system/lastExitClean` | `true` | 上次是否正常退出 |
| 上次退出原因 | `system/lastExitReason` | 空 | 上次退出的原因描述 |
| 上次 Shader 重处理状态 | `system/prevAutoReprocessShader` | `true` | 异常退出前 Shader 自动重处理的启用状态 |
| 上次 AI 重处理状态 | `system/prevAutoReprocessAI` | `true` | 异常退出前 AI 自动重处理的启用状态 |

---

### UI 状态文件 (ui_state.json)

**存储路径：**
```
{ConfigLocation}/ui_state.json
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\ui_state.json
```

**代码位置：** `UIStateController::getStateFilePath()`

**数据内容：**
- 窗口位置和大小
- 侧边栏宽度
- 分割器位置等 UI 布局状态

---

## 处理结果目录

### AI 图像处理结果

**存储路径：**
```
{effectiveDataPath}/ai/images/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\images\
```

**代码位置：** `SettingsController::getAIImagePath()`

**文件命名规则：**
```
<原文件名>_ai_enhanced_<日期yyyyMMdd_HHmmss_zzz>_<UUID前8位>.<原扩展名>
```

**示例：**
```
photo_ai_enhanced_20260402_120530_123_abc12345.png
```

**说明：** 图像文件保持原格式，若原文件无扩展名则默认使用 `.png`。

---

### AI 视频处理结果

**存储路径：**
```
{effectiveDataPath}/ai/videos/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\videos\
```

**代码位置：** `SettingsController::getAIVideoPath()`

**文件命名规则：**
```
<原文件名>_ai_enhanced_<日期yyyyMMdd_HHmmss_zzz>_<UUID前8位>.mp4
```

**示例：**
```
video_ai_enhanced_20260402_120530_456_def67890.mp4
```

**说明：** 视频文件统一输出为 `.mp4` 格式。

---

### Shader 图像处理结果

**存储路径：**
```
{effectiveDataPath}/shader/images/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\images\
```

**代码位置：** `SettingsController::getShaderImagePath()`

**文件命名规则：**
```
<UUID>.png
```

**示例：**
```
a1b2c3d4-e5f6-7890-abcd-ef1234567890.png
```

---

### Shader 视频处理结果

**存储路径：**
```
{effectiveDataPath}/shader/videos/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\videos\
```

**代码位置：** `SettingsController::getShaderVideoPath()`

**文件命名规则：**
```
<原文件名>_shader_<UUID前8位>.<原扩展名>
```

**示例：**
```
video_shader_abc12345.mp4
clip_shader_def67890.avi
```

**说明：** Shader 视频保留原始文件扩展名。

---

## 缩略图缓存

### 缩略图存储方式

**存储类型：** 磁盘持久化（SQLite 数据库 + PNG 文件）

**缩略图目录：**
```
{effectiveDataPath}/thumbnails/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\thumbnails\
```

**元数据库：**
```
{effectiveDataPath}/thumbnail_meta.db
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\thumbnail_meta.db
```

**代码位置：** `ThumbnailDatabase::initialize()`

**缩略图文件命名规则：**
```
<MD5哈希前16位>.png
```

**示例：**
```
a1b2c3d4e5f67890.png
```

**说明：**
- 缩略图以 PNG 格式持久化存储在磁盘上
- 元数据（原始路径、尺寸、修改时间等）存储在 SQLite 数据库中
- 数据库使用 WAL 模式，支持并发读取
- 缩略图文件名由缓存键的 MD5 哈希前 16 位生成
- 若数据库初始化失败，回退为纯内存模式

---

## 任务恢复快照

### 恢复快照 (recovery_snapshot.json)

**存储路径：**
```
{effectiveDataPath}/system/recovery_snapshot.json
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\system\recovery_snapshot.json
```

**代码位置：** `TaskRecoveryController::snapshotFilePath()`

**数据内容：**
- 正在执行的任务上下文
- 任务关联的会话和消息 ID
- 任务处理参数
- 用于应用异常退出后的任务恢复

---

## 导出与保存路径

### 默认导出路径

**存储路径：**
```
{PicturesLocation}/EnhanceVision/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\Pictures\EnhanceVision\
```

**代码位置：** `SettingsController` 初始化 + `FileController::getDefaultSavePath()`

**用途：** 用户导出处理结果时的默认保存位置。

---

### FileUtils 备用保存路径

**存储路径：**
```
{DocumentsLocation}/EnhanceVision/output/
```

**Windows 实际路径：**
```
C:\Users\<用户名>\Documents\EnhanceVision\output\
```

**代码位置：** `FileUtils::getDefaultSavePath()`

**用途：** 当设置中的默认保存路径为空时，FileUtils 提供的备用保存路径。

---

## 日志文件

### 运行时日志

**存储路径：**
```
{AppLocalDataLocation}/logs/runtime_output.log
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\logs\runtime_output.log
```

**代码位置：** `main.cpp` → `ensureLogDirectory()`

**特点：**
- 每次启动覆盖旧日志
- 默认只保留 warning/error/fatal；信息级与调试级输出仅在排障时短期开启
- 包含时间戳和日志级别

---

### 崩溃日志

**存储路径：**
```
{AppLocalDataLocation}/logs/crash.log
```

**Windows 实际路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\logs\crash.log
```

**代码位置：** `main.cpp`

**特点：**
- 追加模式写入，保留历史崩溃记录
- 仅 Windows 平台支持
- 包含时间戳和异常代码

---

### 日志路径说明

> **注意：** 日志路径在代码中存在两处定义：
> - **实际写入路径**（`main.cpp`）：`{AppLocalDataLocation}/logs/` — 日志文件实际写入此目录
> - **设置页统计路径**（`SettingsController::getLogPath()`）：`{applicationDirPath}/logs` — 用于设置页计算日志占用空间
>
> 这意味着设置页显示的日志大小可能与实际日志目录不一致。此为已知问题。

---

## AI 模型文件

### 模型目录搜索顺序

**搜索路径（按优先级）：**

| 优先级 | 路径 | 说明 |
|--------|------|------|
| 1 | `{applicationDirPath}/models/` | 发布模式 |
| 2 | `{applicationDirPath}/../resources/models/` | macOS Bundle |
| 3 | `{applicationDirPath}/resources/models/` | Linux 安装 |
| 4 | `{applicationDirPath}/../../resources/models/` | 开发模式 |

**代码位置：** `ProcessingController` 和 `ProcessingEngine` 中的模型目录搜索链

---

### 模型配置文件 (models.json)

**存储路径：**
```
{模型目录}/models.json
```

**代码位置：** `ModelRegistry::initialize()`

**数据内容：**
- 可用模型列表
- 模型类别元数据
- 模型参数文件路径
- 模型处理参数（归一化、分块大小等）

---

### 模型权重文件

**文件类型：**
- `.param` - NCNN 模型参数文件
- `.bin` - NCNN 模型权重文件

**存储路径：**
```
{模型目录}/{模型ID}.param
{模型目录}/{模型ID}.bin
```

---

## 翻译文件

### 应用翻译

**存储路径：**
```
:/i18n/app_{语言代码}.qm
```

**说明：** 嵌入在 Qt 资源系统（qrc）中，如 `:/i18n/app_zh_CN.qm`。

**代码位置：** `Application::loadTranslation()`

### Qt 翻译搜索路径

| 优先级 | 路径 | 说明 |
|--------|------|------|
| 1 | `{applicationDirPath}/translations/` | 本地 Qt 翻译 |
| 2 | `{QLibraryInfo::TranslationsPath}` | Qt 安装目录翻译 |

---

## 路径映射表

### 完整路径汇总

| 数据类型 | Qt 路径类型 | 实际路径 (Windows) |
|---------|------------|-------------------|
| 会话数据 | `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json` |
| 应用设置 | `ConfigLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini` |
| UI 状态 | `ConfigLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\ui_state.json` |
| AI 图像结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\images\` |
| AI 视频结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\videos\` |
| Shader 图像结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\images\` |
| Shader 视频结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\videos\` |
| 缩略图文件 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\thumbnails\` |
| 缩略图元数据库 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\thumbnail_meta.db` |
| 任务恢复快照 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\system\recovery_snapshot.json` |
| 默认导出路径 | `PicturesLocation` | `C:\Users\<用户名>\Pictures\EnhanceVision\` |
| FileUtils 保存路径 | `DocumentsLocation` | `C:\Users\<用户名>\Documents\EnhanceVision\output\` |
| 运行时日志 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\logs\runtime_output.log` |
| 崩溃日志 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\logs\crash.log` |
| AI 模型 | 应用目录 | `{应用目录}\models\` |

> **注意：** 标注 `AppLocalDataLocation` 的路径在用户设置了自定义数据路径时，将变为自定义路径下的对应子目录。

---

## 数据清理指南

### 清空会话数据

删除以下文件即可清空所有会话：
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json
```

### 清空处理结果

删除以下目录：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\
```

### 清空缩略图缓存

删除以下文件和目录：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\thumbnails\
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\thumbnail_meta.db
```

### 清空任务恢复快照

删除以下文件：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\system\recovery_snapshot.json
```

### 清空日志文件

删除以下目录：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\logs\
```

### 重置应用设置

删除以下文件：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini
C:\Users\<用户名>\AppData\Local\EnhanceVision\ui_state.json
```

### 完全清理

删除以下目录：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\
```

---

## 注意事项

1. **路径差异**：`AppDataLocation` 和 `AppLocalDataLocation` 在 Windows 上分别对应 `Roaming` 和 `Local` 目录。

2. **组织名嵌套**：由于 `setOrganizationName("EnhanceVision")` 和 `setApplicationName("EnhanceVision")` 相同，导致路径中出现多层 `EnhanceVision` 目录。

3. **模型目录**：模型文件不存储在用户数据目录，而是随应用程序分发，位于应用安装目录下。

4. **日志覆盖**：运行时日志每次启动会覆盖，不会累积；崩溃日志采用追加模式，保留历史记录。

5. **跨平台兼容**：代码使用 `QStandardPaths` 确保跨平台兼容性，Linux 和 macOS 路径会自动适配。

6. **崩溃恢复**：应用启动时会检查 `system/lastExitClean` 标志，若检测到异常退出会自动禁用自动重处理功能，并将之前的重处理状态保存在 `system/prevAutoReprocessShader` 和 `system/prevAutoReprocessAI` 中。

7. **自定义数据路径**：用户可在设置中指定自定义数据路径（`behavior/customDataPath`），设置后所有基于 `effectiveDataPath` 的目录（AI/Shader 处理结果、缩略图、任务恢复快照、日志）将存储到该路径下。会话数据（`sessions.json`）和应用设置（`settings.ini`、`ui_state.json`）不受自定义路径影响。

8. **缩略图持久化**：缩略图使用 SQLite 数据库持久化存储在磁盘上，包含元数据库（`thumbnail_meta.db`）和图片文件（`thumbnails/` 目录）。若数据库初始化失败，回退为纯内存模式。

9. **日志路径不一致**：日志文件实际写入 `{AppLocalDataLocation}/logs/`（由 `main.cpp` 控制），但 `SettingsController::getLogPath()` 返回 `{applicationDirPath}/logs` 用于设置页统计。两者可能指向不同目录。

10. **会话数据路径特殊性**：`sessions.json` 使用 `AppDataLocation`（Roaming），而非 `AppLocalDataLocation`（Local），且代码中额外追加了 `EnhanceVision/` 子目录，导致路径嵌套更深。
