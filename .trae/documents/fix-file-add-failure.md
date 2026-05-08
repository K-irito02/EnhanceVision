# 修复文件添加失败问题 - 实施计划

## 问题摘要

用户点击"添加文件"按钮添加 3.5GB 视频文件时，应用程序无任何反应，文件打开失败。

## 当前状态分析

### 根本原因

1. **文件大小限制过严**：`FileModel::kMaxFileSize = 2GB`，3.5GB 文件被拒绝
2. **错误信息未展示给用户**：`FileModel::errorOccurred` 信号发出后，QML 层未连接该信号，导致用户看不到任何提示

### 次要问题

3. **支持格式列表不一致**：6 处定义了不同的格式列表
   - `FileController` 构造函数：视频含 `wmv`
   - `FileModel::isFormatSupported()`：视频不含 `wmv`
   - `ImageUtils::isImageFile()`：与 FileController 一致
   - `ImageUtils::isVideoFile()`：视频不含 `wmv`、`webm`、`m4v`、`mpg`、`mpeg`、`ts`、`mts`、`m2ts`
   - `VideoProcessorFactory::isVideoFile()`：最完整（13 种视频格式）
   - QML `FileDialog.nameFilters`：视频不含 `wmv`、`webm` 等
4. **错误消息语言不一致**：`isSizeValid` 错误消息为英文，其余为中文
5. **格式列表维护困难**：分散在 6+ 处，改一处容易遗漏其他

## 提议变更

### 变更 1：创建统一格式注册表 `SupportedFormats`

**文件**：新建 `include/EnhanceVision/utils/SupportedFormats.h` + `src/utils/SupportedFormats.cpp`

**设计**：
- 提供静态方法获取支持的图片/视频格式列表
- 作为唯一真相源（Single Source of Truth）
- 所有其他模块统一引用此类

```cpp
// SupportedFormats.h
namespace EnhanceVision {

class SupportedFormats {
public:
    static const QStringList& imageExtensions();
    static const QStringList& videoExtensions();
    static const QStringList& allExtensions();

    static QString imageFileDialogFilter();
    static QString videoFileDialogFilter();
    static QString allFileDialogFilter();
    static QString qmlNameFilters();

    static bool isImageFile(const QString& filePath);
    static bool isVideoFile(const QString& filePath);
    static bool isFormatSupported(const QString& filePath);
};

} // namespace EnhanceVision
```

**格式列表**（与 VideoProcessorFactory 对齐并扩展）：
- 图片：`jpg`, `jpeg`, `png`, `bmp`, `webp`, `tiff`, `tif`, `gif`, `ico`, `svg`
- 视频：`mp4`, `avi`, `mkv`, `mov`, `flv`, `wmv`, `webm`, `m4v`, `mpg`, `mpeg`, `ts`, `mts`, `m2ts`, `3gp`

### 变更 2：移除文件大小限制

**文件**：
- `include/EnhanceVision/models/FileModel.h`：移除 `kMaxFileSize` 声明和 `isSizeValid()` 方法
- `src/models/FileModel.cpp`：移除 `kMaxFileSize` 定义、`isSizeValid()` 实现、`addFile()` 中的大小检查

**理由**：
- FFmpeg 采用流式处理，不会一次性将整个文件加载到内存
- 缩略图生成也只读取少量帧，与文件总大小无关
- 用户明确要求支持任意大小的文件

### 变更 3：添加错误反馈到 QML

**文件**：`qml/pages/MainPage.qml`

在 MainPage 中添加 Connections 监听 `fileController.errorOccurred`，调用 `NotificationManager.showError()` 显示错误提示：

```qml
Connections {
    target: typeof fileController !== "undefined" ? fileController : null
    enabled: typeof fileController !== "undefined"
    function onErrorOccurred(message) {
        NotificationManager.showError(message)
    }
}
```

### 变更 4：重构现有模块引用统一格式注册表

**文件及变更**：

| 文件 | 变更内容 |
|------|----------|
| `src/controllers/FileController.cpp` | 构造函数中格式列表改为引用 `SupportedFormats`；`isFileSupported()` 委托给 `SupportedFormats::isFormatSupported()`；`getFileDialogFilter()` 委托给 `SupportedFormats` |
| `src/models/FileModel.cpp` | `isFormatSupported()` 委托给 `SupportedFormats::isFormatSupported()`；`getMediaType()` 委托给 `SupportedFormats::isImageFile()` |
| `src/utils/ImageUtils.cpp` | `isImageFile()` 和 `isVideoFile()` 委托给 `SupportedFormats` |
| `qml/pages/MainPage.qml` | `FileDialog.nameFilters` 改为动态获取或与 `SupportedFormats` 保持一致；`_addDemoFiles()` 中的视频格式判断与 `SupportedFormats` 对齐 |

### 变更 5：修复错误消息语言一致性

**文件**：`src/models/FileModel.cpp`

- 移除 `isSizeValid` 相关代码（变更 2 已涵盖）
- 确保所有 `errorOccurred` 消息使用中文

### 变更 6：更新 QML FileDialog 过滤器

**文件**：`qml/pages/MainPage.qml`

更新 `FileDialog.nameFilters` 以包含所有支持的格式，与 `SupportedFormats` 保持一致。

## 假设与决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 文件大小限制 | 完全移除 | 用户明确要求；FFmpeg 流式处理不依赖文件总大小 |
| 格式管理方式 | 创建统一注册表类 | 一改全改，消除不一致 |
| 新增图片格式 | gif, ico, svg | Qt 原生支持，用户可能需要 |
| 新增视频格式 | wmv, webm, m4v, mpg, mpeg, ts, mts, m2ts, 3gp | FFmpeg 支持这些格式，与 VideoProcessorFactory 对齐 |

## 验证步骤

1. **构建验证**：编译通过，无警告
2. **功能验证**：
   - 添加 3.5GB+ 视频文件 → 成功添加，显示缩略图
   - 添加 wmv/webm/m4v 等格式 → 成功添加
   - 添加不支持的格式 → 显示错误提示（如 .txt 文件）
   - 拖拽添加文件 → 正常工作
   - FileDialog 中显示所有支持的格式过滤器
3. **回归验证**：
   - 原有 jpg/png/mp4 等格式仍可正常添加
   - 处理流程（Shader/AI）不受影响
   - 缩略图生成正常
