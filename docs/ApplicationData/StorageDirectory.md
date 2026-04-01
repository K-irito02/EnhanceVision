# EnhanceVision 应用程序数据存储目录

本文档记录 EnhanceVision 应用程序的所有数据存储路径。

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
C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json
```

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
C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini
```

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
| 上次正常退出 | `system/lastExitClean` | `true` | 上次是否正常退出 |

---

## 处理结果目录

### AI 图像处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\images\
```

**文件命名规则：**
```
<原文件名>_ai_<日期时间毫秒>_<UUID前8位>.<扩展名>
```

**示例：**
```
photo_ai_20260402_120530_123_abc12345.png
```

---

### AI 视频处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\videos\
```

**文件命名规则：**
```
<原文件名>_ai_<日期时间毫秒>_<UUID前8位>.mp4
```

**示例：**
```
video_ai_20260402_120530_456_def67890.mp4
```

---

### Shader 图像处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\images\
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

### Shader 视频处理结果

**存储路径：**
```
C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\videos\
```

**文件命名规则：**
```
<原文件名>_shader_<UUID前8位>.mp4
```

**示例：**
```
video_shader_abc12345.mp4
```

---

## 导出与保存路径

### 默认导出路径

**存储路径：**
```
C:\Users\<用户名>\Pictures\EnhanceVision\
```

**用途：** 用户导出处理结果时的默认保存位置。

---

### FileUtils 备用保存路径

**存储路径：**
```
C:\Users\<用户名>\Documents\EnhanceVision\output\
```

**用途：** FileUtils 工具类提供的备用保存路径。

---

## 缩略图缓存

### 缩略图存储方式

**存储类型：** 内存缓存（不持久化到磁盘）

**说明：**
- 缩略图在应用运行时存储在内存中
- 应用关闭后缩略图缓存自动清空
- 下次启动时从原始文件重新生成缩略图

---

## 日志文件

### 运行时日志

**存储路径：**
```
<应用程序目录>/logs/runtime_output.log
```

**特点：**
- 每次启动覆盖旧日志
- 记录所有调试、信息、警告、错误输出
- 包含时间戳和日志级别

---

### 崩溃日志

**存储路径：**
```
<应用程序目录>/logs/crash.log
```

**特点：**
- 追加模式写入，保留历史崩溃记录
- 仅 Windows 平台支持
- 包含时间戳和异常代码

---

## AI 模型文件

### 模型目录搜索顺序

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
<模型目录>/<模型ID>.param
<模型目录>/<模型ID>.bin
```

---

## 路径映射表

### 完整路径汇总

| 数据类型 | Qt 路径类型 | 实际路径 (Windows) |
|---------|------------|-------------------|
| 会话数据 | `AppDataLocation` | `C:\Users\<用户名>\AppData\Roaming\EnhanceVision\EnhanceVision\EnhanceVision\EnhanceVision\sessions.json` |
| 应用设置 | `ConfigLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\settings.ini` |
| AI 图像结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\images\` |
| AI 视频结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\ai\videos\` |
| Shader 图像结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\images\` |
| Shader 视频结果 | `AppLocalDataLocation` | `C:\Users\<用户名>\AppData\Local\EnhanceVision\EnhanceVision\shader\videos\` |
| 默认导出路径 | `PicturesLocation` | `C:\Users\<用户名>\Pictures\EnhanceVision\` |
| FileUtils 保存路径 | `DocumentsLocation` | `C:\Users\<用户名>\Documents\EnhanceVision\output\` |
| 缩略图缓存 | 内存 | 不持久化到磁盘 |
| 运行时日志 | 应用目录 | `<应用目录>/logs/runtime_output.log` |
| 崩溃日志 | 应用目录 | `<应用目录>/logs/crash.log` |
| AI 模型 | 应用目录 | `<应用目录>/models/` |

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

2. **组织名嵌套**：由于 `setOrganizationName("EnhanceVision")` 和 `setApplicationName("EnhanceVision")` 相同，导致路径中出现多层 `EnhanceVision` 目录。

3. **模型目录**：模型文件不存储在用户数据目录，而是随应用程序分发，位于应用安装目录下。

4. **日志覆盖**：运行时日志每次启动会覆盖，不会累积；崩溃日志采用追加模式，保留历史记录。

5. **跨平台兼容**：代码使用 `QStandardPaths` 确保跨平台兼容性，Linux 和 macOS 路径会自动适配。

6. **崩溃恢复**：应用启动时会检查 `system/lastExitClean` 标志，若检测到异常退出会自动禁用自动重处理功能。

7. **自定义数据路径**：用户可在设置中指定自定义数据路径，所有处理结果将存储到该路径下。

8. **缩略图不持久化**：缩略图为内存缓存，应用重启后需重新生成。
