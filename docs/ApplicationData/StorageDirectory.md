# EnhanceVision 应用程序数据存储目录

本文档描述当前代码实现下的真实存储目录策略，以运行时逻辑为准。

## 设计原则

- 安装目录用于放置程序文件与随安装部署的静态资源
- 小型配置数据继续留在 Windows 用户配置区
- 大体积、可迁移、用户明确关心的运行时数据统一放在“应用数据目录”
- “默认导出路径”独立配置，仅决定后续导出/自动保存的输出位置
- 路径仅在真实需要写入时懒创建，避免无用途的僵尸子目录

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

以下数据不跟随“应用数据目录”迁移，仍保留在系统配置区：

| 数据 | 路径 |
|------|------|
| `settings.ini` | `%LOCALAPPDATA%\EnhanceVision\settings.ini` |
| `ui_state.json` | `%LOCALAPPDATA%\EnhanceVision\ui_state.json` |
| `sessions.json` | `%APPDATA%\EnhanceVision\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json` |

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

## 设置页与旧数据处理

设置页“数据存储”区域展示以下运行时状态：

- 当前生效的应用数据目录
- 当前生效的默认导出路径
- 目录回退状态与回退原因
- 是否检测到旧应用数据目录

当 `previousDataPath` 真实存在且有数据时，设置页才显示：

- `迁移`：把旧目录数据迁移到当前目录，冲突文件自动以 `_legacy_<timestamp>` 后缀避让
- `清理`：删除旧目录中的全部旧数据，不影响当前目录

### 迁移保护

迁移不仅复制文件，还会执行以下一致性处理：

- 重写会话中持久化的结果路径，避免会话仍指向旧目录
- 清空并重建缩略图缓存/数据库状态，避免 UI 仍引用旧路径
- 成功后才删除旧目录

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
