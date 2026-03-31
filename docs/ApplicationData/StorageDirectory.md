# EnhanceVision 应用程序数据存储目录

本文档详细记录 EnhanceVision 应用程序的所有数据存储路径。

## 目录

1. [用户数据目录](#用户数据目录)
2. [配置文件](#配置文件)
3. [处理结果目录](#处理结果目录)
4. [日志文件](#日志文件)
5. [AI 模型文件](#ai-模型文件)
6. [路径映射表](#路径映射表)

---

## 用户数据目录

### Windows 系统路径说明

| Qt 标准路径 | Windows 实际路径 |
|------------|-----------------|
| `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\<组织名>\<应用名>` |
| `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\<组织名>\<应用名>` |
| `ConfigLocation` | `C:\Users\<用户名>\AppData\Local\<组织名>` |
| `PicturesLocation` | `C:\Users\<用户名>\Pictures` |
| `DocumentsLocation` | `C:\Users\<用户名>\Documents` |

### 应用程序标识

**代码位置：** `main.cpp:150-153`

```cpp
QApplication::setApplicationName("EnhanceVision");
QApplication::setApplicationVersion("0.1.0");
QApplication::setOrganizationName("EnhanceVision");
```

---

## 配置文件

### 1. 会话数据 (sessions.json)

**存储路径：**
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json
```

**代码位置：** `SessionController.cpp:833-837`

```cpp
QString SessionController::sessionsFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dataPath).filePath("EnhanceVision/sessions.json");
}
```

**数据内容：**
- 所有会话的元数据（ID、名称、创建时间、修改时间）
- 每个会话中的消息列表
- 消息中的媒体文件引用
- AI 处理参数和 Shader 参数
- 会话置顶状态和排序索引

**JSON 结构示例：**
```json
{
    "version": 1,
    "lastActiveSessionId": "session_xxx",
    "sessionCounter": 5,
    "sessions": [
        {
            "id": "session_xxx",
            "name": "默认会话",
            "createdAt": "2026-03-27T10:00:00",
            "modifiedAt": "2026-03-27T12:00:00",
            "isPinned": false,
            "sortIndex": 0,
            "messages": [...]
        }
    ]
}
```

---

### 2. 应用设置 (settings.ini)

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini
```

**代码位置：** `SettingsController.cpp:40-42`

```cpp
QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
QString settingsFile = QDir(configPath).filePath("EnhanceVision/settings.ini");
m_settings = new QSettings(settingsFile, QSettings::IniFormat, this);
```

**数据内容：**

| 设置项 | 键名 | 默认值 | 说明 |
|-------|------|-------|------|
| 主题 | `appearance/theme` | `dark` | 界面主题 |
| 语言 | `appearance/language` | `zh_CN` | 界面语言 |
| 侧边栏展开 | `sidebar/expanded` | `true` | 侧边栏是否展开 |
| 默认保存路径 | `behavior/defaultSavePath` | `Pictures/EnhanceVision` | 导出文件的默认路径 |
| 自动保存结果 | `behavior/autoSave` | `false` | 是否自动保存处理结果 |
| 音量 | `audio/volume` | `80` | 播放音量 (0-100) |
| Shader 自动重处理 | `reprocess/shaderEnabled` | `true` | Shader 效果自动重新处理 |
| AI 自动重处理 | `reprocess/aiEnabled` | `true` | AI 效果自动重新处理 |
| 视频自动播放 | `video/autoPlay` | `true` | 视频加载后自动播放 |
| 切换时自动播放 | `video/autoPlayOnSwitch` | `true` | 切换消息时自动播放视频 |
| 恢复播放位置 | `video/restorePosition` | `true` | 切换消息时恢复播放位置 |
| 上次正常退出 | `system/lastExitClean` | `true` | 上次是否正常退出 |
| 上次退出原因 | `system/lastExitReason` | - | 上次退出的原因 |
| 上次 Shader 重处理状态 | `system/prevAutoReprocessShader` | - | 崩溃恢复前的 Shader 重处理状态 |
| 上次 AI 重处理状态 | `system/prevAutoReprocessAI` | - | 崩溃恢复前的 AI 重处理状态 |

**INI 结构示例：**
```ini
[appearance]
theme=dark
language=zh_CN

[sidebar]
expanded=true

[behavior]
defaultSavePath=C:/Users/<用户名>/Pictures/EnhanceVision
autoSave=false

[audio]
volume=80

[reprocess]
shaderEnabled=true
aiEnabled=true

[video]
autoPlay=true
autoPlayOnSwitch=true
restorePosition=true

[system]
lastExitClean=true
prevAutoReprocessShader=true
prevAutoReprocessAI=true
```

---

## 处理结果目录

### 1. AI 增强处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\processed\ai\
```

**代码位置：** `ProcessingController.cpp:590`

```cpp
const QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/processed/ai";
```

**文件命名规则：**
```
<原文件名>_ai_enhanced_<日期时间毫秒>_<UUID前8位>.<扩展名>
```

**命名格式说明：**
- 日期时间格式：`yyyyMMdd_HHmmss_zzz`（精确到毫秒）
- UUID：取前 8 位字符
- 视频文件统一输出为 `.mp4` 格式
- 图像文件保持原格式，无格式时默认 `.png`

**示例：**
```
photo_ai_enhanced_20260327_120530_123_abc12345.png
video_ai_enhanced_20260327_120530_456_def67890.mp4
```

---

### 2. Shader 图像处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\
```

**代码位置：** `ProcessingController.cpp:530`

```cpp
QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed";
```

**文件命名规则：**
```
<UUID>.png
```

**示例：**
```
a1b2c3d4-e5f6-7890-abcd-ef1234567890.png
```

---

### 3. Shader 视频处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\shader_video\
```

**代码位置：** `ProcessingController.cpp:848`

```cpp
QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed/shader_video";
```

**文件命名规则：**
```
<原文件名>_shader_<UUID前8位>.<扩展名>
```

**示例：**
```
video_shader_abc12345.mp4
```

---

### 4. 默认保存路径

**存储路径：**
```
C:\Users\<用户名>\Pictures\EnhanceVision\
```

**代码位置：** `SettingsController.cpp:45-46`

```cpp
QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");
```

**用途：** 用户导出处理结果时的默认保存位置。

---

### 5. FileUtils 默认保存路径

**存储路径：**
```
C:\Users\<用户名>\Documents\EnhanceVision\output\
```

**代码位置：** `FileUtils.cpp:103-108`

```cpp
QString FileUtils::getDefaultSavePath()
{
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString savePath = QDir(documentsPath).filePath("EnhanceVision/output");
    ensureDirectory(savePath);
    return savePath;
}
```

**用途：** FileUtils 工具类提供的备用保存路径。

---

## 日志文件

### 1. 运行时日志

**存储路径：**
```
<应用程序目录>/logs/runtime_output.log
```

**代码位置：** `main.cpp:157-167`

```cpp
// 创建日志目录
QDir logDir("logs");
if (!logDir.exists()) {
    logDir.mkpath(".");
}

// 使用固定的日志文件名，每次启动覆盖
logFilePath = "logs/runtime_output.log";
logFile = new QFile(logFilePath);

if (logFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qInstallMessageHandler(messageHandler);
    qInfo() << "Log file created:" << logFilePath;
}
```

**特点：**
- 每次启动覆盖旧日志
- 记录所有 `qDebug`、`qInfo`、`qWarning`、`qCritical` 输出
- 包含时间戳和日志级别

**日志格式：**
```
[2026-03-27 12:00:00.000] [Info] Log file created: logs/runtime_output.log
[2026-03-27 12:00:00.100] [Info] Application started
```

---

### 2. 崩溃日志

**存储路径：**
```
<应用程序目录>/logs/crash.log
```

**代码位置：** `main.cpp:76-100`

```cpp
// 写入崩溃日志文件
HANDLE hFile = CreateFileW(
    L"logs/crash.log",
    FILE_APPEND_DATA,
    FILE_SHARE_READ,
    nullptr,
    OPEN_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    nullptr
);
```

**用途：** 记录应用程序崩溃时的异常信息。

**特点：**
- 追加模式写入，保留历史崩溃记录
- 仅 Windows 平台支持
- 包含时间戳和异常代码

**日志格式：**
```
[2026-03-27 12:00:00.000] [CRASH] Exception: 0xC0000005 at 0x00007FF6ABC12345
```

---

## AI 模型文件

### 模型目录搜索顺序

**代码位置：** `ProcessingController.cpp:77-87`

```cpp
QString modelsPath = QCoreApplication::applicationDirPath() + "/models";
if (!QDir(modelsPath).exists()) {
    modelsPath = QCoreApplication::applicationDirPath() + "/../resources/models";
}
if (!QDir(modelsPath).exists()) {
    modelsPath = QCoreApplication::applicationDirPath() + "/resources/models";
}
if (!QDir(modelsPath).exists()) {
    modelsPath = QDir(QCoreApplication::applicationDirPath()).filePath("../../resources/models");
}
m_modelRegistry->initialize(modelsPath);
```

**搜索路径（按优先级）：**
1. `<应用目录>/models/`
2. `<应用目录>/../resources/models/`
3. `<应用目录>/resources/models/`
4. `<应用目录>/../../resources/models/`

---

### 模型配置文件 (models.json)

**存储路径：**
```
<模型目录>/models.json
```

**代码位置：** `ModelRegistry.cpp:45`

```cpp
QString jsonPath = modelsRootPath + "/models.json";
```

**数据内容：**
- 可用模型列表
- 模型类别元数据
- 模型参数文件路径
- 模型处理参数（归一化、分块大小等）

**JSON 结构示例：**
```json
{
    "categories": {
        "super_resolution": {
            "name": "超分辨率",
            "name_en": "Super Resolution",
            "icon": "zoom_in",
            "order": 1
        }
    },
    "models": [
        {
            "id": "realesrgan-x4plus",
            "name": "RealESRGAN x4plus",
            "description": "通用超分辨率模型",
            "description_en": "General super resolution model",
            "category": "super_resolution",
            "scaleFactor": 4,
            "inputChannels": 3,
            "outputChannels": 3,
            "tileSize": 0,
            "tilePadding": 10,
            "inputBlob": "input",
            "outputBlob": "output",
            "paramFile": "realesrgan-x4plus.param",
            "binFile": "realesrgan-x4plus.bin",
            "normMean": [0, 0, 0],
            "normScale": [1, 1, 1],
            "denormScale": [1, 1, 1],
            "denormMean": [0, 0, 0]
        }
    ]
}
```

---

### 模型权重文件

**文件类型：**
- `.param` - NCNN 模型参数文件
- `.bin` - NCNN 模型权重文件

**存储路径：**
```
<模型目录>/<模型ID>.param
<模型目录>/<模型ID>.bin
```

**代码位置：** `ModelRegistry.cpp` 加载时读取

---

## 路径映射表

### 完整路径汇总

| 数据类型 | Qt 路径类型 | 实际路径 (Windows) |
|---------|------------|-------------------|
| 会话数据 | `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json` |
| 应用设置 | `ConfigLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini` |
| AI 处理结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\processed\ai\` |
| Shader 图像结果 | `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\` |
| Shader 视频结果 | `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\shader_video\` |
| 默认保存路径 | `PicturesLocation` | `C:\Users\<用户名>\Pictures\EnhanceVision\` |
| FileUtils 保存路径 | `DocumentsLocation` | `C:\Users\<用户名>\Documents\EnhanceVision\output\` |
| 运行时日志 | 应用目录 | `<应用目录>/logs/runtime_output.log` |
| 崩溃日志 | 应用目录 | `<应用目录>/logs/crash.log` |
| AI 模型 | 应用目录 | `<应用目录>/models/` |

---

## 数据清理指南

### 清空会话数据

删除以下文件即可清空所有会话：
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json
```

### 清空处理结果

删除以下目录：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\processed\
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\
```

### 重置应用设置

删除以下文件：
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini
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

2. **组织名嵌套**：由于 `setOrganizationName("EnhanceVision")` 和 `setApplicationName("EnhanceVision")` 相同，导致路径中出现三层 `EnhanceVision` 目录。

3. **模型目录**：模型文件不存储在用户数据目录，而是随应用程序分发，位于应用安装目录下。

4. **日志覆盖**：运行时日志每次启动会覆盖，不会累积；崩溃日志采用追加模式，保留历史记录。

5. **跨平台兼容**：代码使用 `QStandardPaths` 确保跨平台兼容性，Linux 和 macOS 路径会自动适配。

6. **崩溃恢复**：应用启动时会检查 `system/lastExitClean` 标志，若检测到异常退出会自动禁用自动重处理功能。

---

## 代码位置索引

| 功能 | 文件 | 行号 |
|------|------|------|
| 应用标识设置 | `main.cpp` | 150-153 |
| 日志目录创建 | `main.cpp` | 157-161 |
| 运行时日志路径 | `main.cpp` | 164 |
| 崩溃日志路径 | `main.cpp` | 77 |
| 会话文件路径 | `SessionController.cpp` | 833-837 |
| 会话目录确保 | `SessionController.cpp` | 839-847 |
| 设置文件路径 | `SettingsController.cpp` | 40-42 |
| 默认保存路径 | `SettingsController.cpp` | 45-46 |
| 设置加载 | `SettingsController.cpp` | 383-408 |
| 设置保存 | `SettingsController.cpp` | 365-381 |
| 模型目录搜索 | `ProcessingController.cpp` | 77-87 |
| Shader 图像输出 | `ProcessingController.cpp` | 530 |
| AI 处理输出 | `ProcessingController.cpp` | 590 |
| Shader 视频输出 | `ProcessingController.cpp` | 848 |
| 模型配置加载 | `ModelRegistry.cpp` | 45 |
| FileUtils 保存路径 | `FileUtils.cpp` | 103-108 |
