# 文件添加失败修复与格式统一重构

## 概述

修复 3.5GB 视频文件添加失败问题，统一支持格式管理，修复 Shader 模式下源件/成果切换时加载动画不消失的 Bug。

**创建日期**: 2026-05-09

---

## 变更概述

### 新增文件

| 文件路径 | 功能描述 |
|----------|----------|
| `include/EnhanceVision/utils/SupportedFormats.h` | 统一格式注册表头文件 |
| `src/utils/SupportedFormats.cpp` | 统一格式注册表实现 |

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/models/FileModel.h` | 移除 `kMaxFileSize` 和 `isSizeValid()` |
| `src/models/FileModel.cpp` | 移除大小检查逻辑，格式判断委托给 `SupportedFormats` |
| `include/EnhanceVision/controllers/FileController.h` | 移除 `m_supportedImageFormats`/`m_supportedVideoFormats` 成员 |
| `src/controllers/FileController.cpp` | 格式判断和对话框过滤器委托给 `SupportedFormats` |
| `src/utils/ImageUtils.cpp` | `isImageFile()`/`isVideoFile()` 委托给 `SupportedFormats` |
| `src/core/video/VideoProcessorFactory.cpp` | `isVideoFile()` 委托给 `SupportedFormats` |
| `qml/pages/MainPage.qml` | 添加错误反馈连接，扩展 FileDialog 格式过滤器，更新 `_addDemoFiles` |
| `qml/components/EmbeddedMediaViewer.qml` | 修复 Shader 模式下源件/成果切换加载动画不消失 |
| `qml/components/VideoPlaybackController.qml` | 添加 5 秒安全超时防止 `isLoading` 永久卡住 |
| `CMakeLists.txt` | 添加 `SupportedFormats` 源文件 |

---

## 实现的功能特性

- ✅ 移除 2GB 文件大小限制，支持任意大小的视频/图片文件
- ✅ 创建 `SupportedFormats` 统一格式注册表，消除 6+ 处格式列表不一致
- ✅ 扩展视频格式支持：新增 wmv/webm/m4v/mpg/mpeg/ts/mts/m2ts/3gp
- ✅ 扩展图片格式支持：新增 gif/ico/svg
- ✅ 添加文件添加失败的错误反馈到 UI
- ✅ 修复 Shader 模式下源件/成果切换时加载动画不消失
- ✅ 添加 `VideoPlaybackController` 安全超时机制

---

## 技术实现细节

### SupportedFormats 设计

作为唯一真相源（Single Source of Truth），所有模块统一引用：

```cpp
class SupportedFormats {
    static const QStringList& imageExtensions();
    static const QStringList& videoExtensions();
    static const QStringList& allExtensions();
    static QString imageFileDialogFilter();
    static QString videoFileDialogFilter();
    static QString allSupportedFileDialogFilter();
    static bool isImageFile(const QString& filePath);
    static bool isVideoFile(const QString& filePath);
    static bool isFormatSupported(const QString& filePath);
};
```

### Shader 模式切换 Bug 根因

`onShowOriginalChanged` 无条件调用 `prepareSourceResultSwitch()`，但在 Shader 模式下视频源不会改变（效果通过渲染管线切换），导致 `_switchMode` 永远为 2，`isLoading` 永远为 true。

修复：仅当视频源实际改变时才进入切换模式。

---

## 遇到的问题及解决方案

### 问题 1：3.5GB 视频文件添加无反应

**现象**：点击"添加文件"添加大文件后无任何反馈

**原因**：`FileModel::kMaxFileSize = 2GB` 限制 + `errorOccurred` 信号未连接到 UI

**解决方案**：移除大小限制 + 添加 `Connections` 监听错误信号

### 问题 2：Shader 模式切换加载动画不消失

**现象**：点击源件/成果切换后，加载动画持续显示

**原因**：Shader 模式下视频源不变，`MediaPlayer` 不发出 `BufferedMedia` 信号

**解决方案**：仅当源实际改变时才调用 `prepareSourceResultSwitch()`

---

## 测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 添加 3.5GB 视频文件 | 成功添加 | ✅ 通过 |
| 添加 wmv/webm 等新格式 | 成功添加 | ✅ 通过 |
| 添加不支持的格式 | 显示错误提示 | ✅ 通过 |
| Shader 模式源件/成果切换 | 无加载动画 | ✅ 通过 |
| 原有 jpg/png/mp4 格式 | 正常工作 | ✅ 通过 |
