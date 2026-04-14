# EnhanceVision 应用程序数据存储目录

本文档描述当前代码实现下的真实存储目录策略，以运行时逻辑为准。

## 设计原则

- 安装目录用于放置程序文件与随安装部署的静态资源
- 小型持久化元数据统一收敛到单一配置根目录
- 大体积、可迁移、用户明确关心的运行时数据统一放在“应用数据目录”
- “默认导出路径”独立配置，仅决定后续导出/自动保存的输出位置
- 路径仅在真实需要写入时懒创建，避免无用途的僵尸子目录

## 配置根目录

当前版本将用户态元数据统一收敛到：

- `%LOCALAPPDATA%\EnhanceVision`

该目录下固定包含：

| 数据 | 路径 |
|------|------|
| 设置文件 | `%LOCALAPPDATA%\EnhanceVision\settings.ini` |
| UI 状态 | `%LOCALAPPDATA%\EnhanceVision\ui_state.json` |
| 会话数据 | `%LOCALAPPDATA%\EnhanceVision\sessions.json` |
| 安装维护意图 | `%LOCALAPPDATA%\EnhanceVision\install_maintenance.json` |

## 配置键

以下两个用户配置键继续沿用：

| 键名 | 含义 |
|------|------|
| `behavior/customDataPath` | 应用数据根目录 |
| `behavior/defaultSavePath` | 默认导出根目录 |

## 路径解析规则

### 应用数据目录

运行时统一通过 `SettingsController::resolveEffectiveDataPath()` / `effectiveDataPath()` 解析。

- 若 `behavior/customDataPath` 有效且可写，则使用该目录
- 若配置为空、无效、不可写或落入受保护目录，则回退到安全默认目录
- 安全默认目录为 `%LOCALAPPDATA%\EnhanceVision\data`
- 设置页展示的是当前生效目录，而不是原始配置字符串
- 当发生回退时，设置页会暴露回退状态与原因

### 默认导出路径

运行时统一通过 `SettingsController::effectiveDefaultSavePath()` 解析。

- 若 `behavior/defaultSavePath` 有效且可写，则使用该目录
- 若配置为空或无效，则回退到安全默认目录
- `FileUtils::getDefaultSavePath()` 的平台默认值为 `%USERPROFILE%\Pictures\EnhanceVision`
- 安装器会根据当前机器盘符情况给出更适合的安装期默认值，但最终仍以运行时校验后的生效路径为准

## 安装器行为

安装器当前将“安装目录”和“默认导出路径”放在同一页配置：

- 安装目录：程序文件目录
- 应用数据目录：自动跟随安装目录，固定解析为 `安装目录\data`
- 默认导出路径：独立可选目录

### 受保护目录策略

- 若安装目录位于 `Program Files`、`Windows` 等受保护位置，安装器会明确提醒
- 该提醒不是强制阻断，用户可继续安装
- 因应用数据目录随安装目录派生，若用户坚持安装到受保护目录，应预期更高的权限风险

## 固定留在 Windows 配置区的数据

以下数据不再散落在多个 `QStandardPaths` 目录，而是统一保留在配置根目录中：

| 数据 | 路径 |
|------|------|
| `settings.ini` | `%LOCALAPPDATA%\EnhanceVision\settings.ini` |
| `ui_state.json` | `%LOCALAPPDATA%\EnhanceVision\ui_state.json` |
| `sessions.json` | `%LOCALAPPDATA%\EnhanceVision\sessions.json` |

## 迁移到应用数据目录的数据

以下运行时数据统一位于 `effectiveDataPath()` 之下：

| 数据类型 | 真实路径 |
|----------|----------|
| AI 图像结果 | `{effectiveDataPath}/ai/images` |
| AI 视频结果 | `{effectiveDataPath}/ai/videos` |
| Shader 图像结果 | `{effectiveDataPath}/shader/images` |
| Shader 视频结果 | `{effectiveDataPath}/shader/videos` |
| 缩略图文件 | `{effectiveDataPath}/thumbnails` |
| 缩略图数据库 | `{effectiveDataPath}/thumbnail_meta.db*` |
| 任务恢复快照 | `{effectiveDataPath}/system/recovery_snapshot.json` |
| 运行日志 | `{effectiveDataPath}/logs/runtime_output.log` |
| 崩溃日志 | `{effectiveDataPath}/logs/crash.log` |

## 安装器升级维护

当安装器检测到旧版本用户数据时，会在升级页提供三种互斥动作：

- 保留现有数据并继续使用旧目录
- 迁移旧数据到本次安装的数据目录
- 删除旧数据并以新目录全新开始

安装器本身不再直接执行业务层迁移，而是只写入一次性维护意图文件。应用在首启阶段先消费该意图，再初始化日志、缩略图库、UI 状态和会话恢复链路。

### 首启维护覆盖范围

- 运行时数据目录：`ai/`、`shader/`、`thumbnails/`、`thumbnail_meta.db*`、`logs/`、`system/`
- 配置根目录元数据：`sessions.json`、`ui_state.json`
- `settings.ini` 中的 `behavior/customDataPath`

## 设置页与运行期目录变更

设置页“数据存储”区域仅展示并维护当前生效目录状态：

- 当前生效的应用数据目录
- 当前生效的默认导出路径
- 目录回退状态与回退原因

设置页不再提供“迁移/清理旧数据目录”按钮。

若用户在运行期修改“应用数据目录”，应用会立即执行受控迁移：

- 迁移运行时数据到新目录
- 重写已加载会话中的结果路径
- 刷新缩略图缓存与数据库持久化状态
- 失败时回滚到旧配置，不进入半迁移状态

设置页“缓存管理”的统计与清理范围始终跟随当前真实生效的 `effectiveDataPath()`，不会再因为升级场景的旧目录探测偏差而落到空目录。

## 日志与启动语言

- 启动初期日志目录不再硬编码到 `AppLocalDataLocation`
- 应用会先读取 `settings.ini` 中的 `behavior/customDataPath`，再将日志写到当前生效数据根目录下的 `logs/`
- 首次启动若设置文件尚未写入语言，运行时会回退读取安装器语言，保证首启语言与安装器选择一致

## 运行期校验建议

每次涉及存储目录、安装器或语言改动后，至少验证以下场景：

1. 新安装后设置页显示路径与安装器选择一致
2. 处理图片、视频、缩略图、恢复快照和日志后，对应目录下出现真实文件
3. 默认导出后，文件落在设置页展示的导出目录
4. 旧数据迁移后，会话内历史处理结果仍可正常识别
5. 从安装器完成页直接启动时，首次语言正确，拖拽上传可用
