# 持久化存储机制实现笔记

## 概述

实现 EnhanceVision 项目的持久化存储机制，包括会话数据持久化、音量设置持久化以及设置页面 UI 完善。

**创建日期**: 2025-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

无新增组件，扩展现有控制器功能。

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `include/EnhanceVision/controllers/SessionController.h` | 添加 JSON 序列化方法声明 |
| `src/controllers/SessionController.cpp` | 实现 `saveSessions()` 和 `loadSessions()`，添加 JSON 序列化辅助方法 |
| `include/EnhanceVision/controllers/SettingsController.h` | 添加 `volume` 属性声明 |
| `src/controllers/SettingsController.cpp` | 实现音量持久化 |
| `src/app/Application.cpp` | 添加启动时加载会话、退出时保存会话的调用 |
| `qml/pages/SettingsPage.qml` | 完善设置页面 UI，添加行为设置、音频设置分组 |
| `qml/components/EmbeddedMediaViewer.qml` | 音量控制与 SettingsController 同步 |
| `qml/components/MediaViewerWindow.qml` | 音量控制与 SettingsController 同步 |

### 3. 实现的功能特性

- ✅ 会话数据持久化（JSON 文件存储）
- ✅ 音量设置持久化（QSettings 存储）
- ✅ 设置页面行为设置分组（默认保存路径、自动保存结果）
- ✅ 设置页面音频设置分组（默认音量滑块）
- ✅ 设置页面性能设置完善（最大并发任务连接到 SettingsController）
- ✅ 媒体播放器音量与全局设置同步

---

## 二、存储设计

### 2.1 全局配置存储

**存储技术**: QSettings + INI 格式  
**存储路径**: `%LocalAppData%/EnhanceVision/settings.ini`

| 配置项 | 存储键 | 默认值 |
|--------|--------|--------|
| 主题设置 | `appearance/theme` | "dark" |
| 语言设置 | `appearance/language` | "zh_CN" |
| 侧边栏展开 | `sidebar/expanded` | true |
| 最大并发任务 | `performance/maxConcurrent` | 2 |
| 默认保存路径 | `behavior/defaultSavePath` | Pictures/EnhanceVision |
| 自动保存结果 | `behavior/autoSave` | false |
| 音量设置 | `audio/volume` | 80 |

### 2.2 会话数据存储

**存储技术**: JSON 文件  
**存储路径**: `%LocalAppData%/EnhanceVision/sessions.json`

**JSON 结构**:
```json
{
  "version": 1,
  "lastActiveSessionId": "uuid-xxx",
  "sessionCounter": 5,
  "sessions": [
    {
      "id": "uuid-xxx",
      "name": "未命名会话 1",
      "createdAt": "2025-03-21T10:30:00",
      "modifiedAt": "2025-03-21T11:45:00",
      "isPinned": false,
      "sortIndex": 0,
      "messages": [...],
      "pendingFiles": [...]
    }
  ]
}
```

---

## 三、技术实现细节

### 3.1 会话序列化

会话数据通过以下方法序列化为 JSON：

```cpp
QJsonObject SessionController::sessionToJson(const Session& session) const;
Session SessionController::jsonToSession(const QJsonObject& json) const;
QJsonObject SessionController::messageToJson(const Message& message) const;
Message SessionController::jsonToMessage(const QJsonObject& json) const;
QJsonObject SessionController::mediaFileToJson(const MediaFile& file) const;
MediaFile SessionController::jsonToMediaFile(const QJsonObject& json) const;
QJsonObject SessionController::shaderParamsToJson(const ShaderParams& params) const;
ShaderParams SessionController::jsonToShaderParams(const QJsonObject& json) const;
```

### 3.2 原子写入

为保证数据完整性，使用临时文件 + 重命名方式写入 JSON：

```cpp
QString tempPath = filePath + ".tmp";
QFile file(tempPath);
file.write(doc.toJson());
file.close();

QFile oldFile(filePath);
if (oldFile.exists()) {
    oldFile.remove();
}
file.rename(filePath);
```

### 3.3 触发时机

- **加载**: `Application::initialize()` 中调用 `m_sessionController->loadSessions()`
- **保存**: `Application::~Application()` 中调用 `m_sessionController->saveSessions()`

---

## 四、可靠性措施

1. **原子写入**: JSON 文件使用临时文件 + 重命名方式写入
2. **错误处理**: 加载失败时使用默认值，记录警告日志
3. **版本控制**: JSON 文件包含版本号，便于未来迁移
4. **路径安全**: 使用 `QStandardPaths` 获取标准存储路径

---

## 五、设置页面 UI 改进

### 5.1 新增配置分组

**行为设置**:
- 默认保存路径（带文件夹选择器）
- 自动保存结果开关

**音频设置**:
- 默认音量滑块（0-100）

### 5.2 完善现有配置

- 最大并发任务：连接到 SettingsController
- 主题切换：保存到 SettingsController

---

## 六、测试验证

- ✅ 创建多个会话，关闭程序后重新打开验证数据恢复
- ✅ 在设置页面修改各项配置，关闭程序后重新打开验证设置保持
- ✅ 修改音量设置，关闭程序后重新打开验证设置保持
- ✅ 项目构建成功，无编译错误
