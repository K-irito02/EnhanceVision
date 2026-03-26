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

```cpp
// main.cpp
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

**代码位置：** `SessionController.cpp:780-783`

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

**代码位置：** `SettingsController.cpp:34-36`

```cpp
QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
QString settingsFile = QDir(configPath).filePath("EnhanceVision/settings.ini");
m_settings = new QSettings(settingsFile, QSettings::IniFormat, this);
```

**数据内容：**
- 主题设置 (`appearance/theme`)
- 语言设置 (`appearance/language`)
- 侧边栏展开状态 (`sidebar/expanded`)
- 最大并发任务数 (`performance/maxConcurrent`)
- 最大并发会话数 (`performance/maxConcurrentSessions`)
- 每条消息最大并发文件数 (`performance/maxConcurrentFilesPerMessage`)
- 默认保存路径 (`behavior/defaultSavePath`)
- 自动保存结果 (`behavior/autoSave`)
- 音量设置 (`audio/volume`)

**INI 结构示例：**
```ini
[appearance]
theme=dark
language=zh_CN

[sidebar]
expanded=true

[performance]
maxConcurrent=2
maxConcurrentSessions=1
maxConcurrentFilesPerMessage=2

[behavior]
defaultSavePath=C:/Users/<用户名>/Pictures/EnhanceVision
autoSave=false

[audio]
volume=80
```

---

## 处理结果目录

### 1. AI 增强处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\processed\ai\
```

**代码位置：** `ProcessingController.cpp:633`

```cpp
const QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/processed/ai";
```

**文件命名规则：**
```
<原文件名>_ai_enhanced_<日期时间>_<UUID前8位>.<扩展名>
```

**示例：**
```
photo_ai_enhanced_20260327_120530_abc12345.png
video_ai_enhanced_20260327_120530_def67890.mp4
```

---

### 2. Shader 图像处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\
```

**代码位置：** `ProcessingController.cpp:788`

```cpp
QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed";
```

**文件命名规则：**
```
<UUID>.png
```

---

### 3. Shader 视频处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\processed\shader_video\
```

**代码位置：** `ProcessingController.cpp:912`

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

**代码位置：** `SettingsController.cpp:39-40`

```cpp
QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");
```

**用途：** 用户导出处理结果时的默认保存位置。

---

## 日志文件

### 1. 运行时日志

**存储路径：**
```
<应用程序目录>/logs/runtime_output.log
```

**代码位置：** `main.cpp:163`

```cpp
logFilePath = "logs/runtime_output.log";
logFile = new QFile(logFilePath);
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

**代码位置：** `main.cpp:76`

```cpp
L"logs/crash.log"
```

**用途：** 记录应用程序崩溃时的异常信息。

---

## AI 模型文件

### 模型目录搜索顺序

**代码位置：** `ProcessingController.cpp:123-133`

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

**JSON 结构示例：**
```json
{
    "categories": {
        "super_resolution": {
            "name": "超分辨率",
            "icon": "zoom_in"
        }
    },
    "models": [
        {
            "id": "realesrgan-x4plus",
            "name": "RealESRGAN x4plus",
            "category": "super_resolution",
            "paramFile": "realesrgan-x4plus.param",
            "binFile": "realesrgan-x4plus.bin"
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

**代码位置：** `ModelRegistry.cpp:143-144`

```cpp
info.paramPath = m_modelsRootPath + "/" + paramFile;
info.binPath = m_modelsRootPath + "/" + binFile;
```

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

4. **日志覆盖**：运行时日志每次启动会覆盖，不会累积。

5. **跨平台兼容**：代码使用 `QStandardPaths` 确保跨平台兼容性，Linux 和 macOS 路径会自动适配。
