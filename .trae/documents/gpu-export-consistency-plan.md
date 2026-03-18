# 预览与导出一致性修复计划

## 问题分析

### 当前架构

| 场景 | 渲染方式 | 代码位置 |
|------|----------|----------|
| 消息展示区域缩略图 | CPU (ImageUtils::generateThumbnail) | ThumbnailProvider.cpp |
| 放大查看窗口 | GPU Shader (FullShaderEffect) | MediaViewerWindow.qml |
| 处理完成后保存 | CPU (ImageUtils::applyShaderEffects) | ProcessingController.cpp |

### 不一致原因

1. **GPU vs CPU 实现差异**：
   - GPU Shader 使用 GLSL 并行处理，浮点精度和插值方式与 CPU 不同
   - CPU 实现虽然算法逻辑相似，但细节处理存在差异

2. **处理顺序差异**：
   - Shader 处理顺序：Exposure → Brightness → Contrast → Saturation → Hue → Gamma → Temperature → Tint → Highlights → Shadows → Vignette → Denoise → Blur → Sharpness
   - CPU 处理顺序：基本一致，但降噪/模糊/锐化的实现细节不同

3. **视觉效果差异**：
   - GPU 渲染更细腻、清晰（用户反馈）
   - CPU 处理可能存在精度损失

---

## 解决方案：GPU 渲染 + grabToImage 导出

### 核心思路

使用 Qt Quick 的 `grabToImage()` 将 GPU 渲染结果直接导出为图像，确保预览和导出使用完全相同的渲染管线。

### 架构变更

```
┌─────────────────────────────────────────────────────────────┐
│                     修改前                                   │
│                                                              │
│  预览区域 ──→ GPU Shader ──→ 实时显示                        │
│  导出保存 ──→ CPU 算法 ──→ 文件保存  ❌ 不一致               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                     修改后                                   │
│                                                              │
│  预览区域 ──→ GPU Shader ──→ 实时显示                        │
│  导出保存 ──→ GPU Shader ──→ grabToImage ──→ 文件保存 ✅ 一致│
└─────────────────────────────────────────────────────────────┘
```

---

## 实施步骤

### 步骤 1：创建 ImageExportService 服务类

**文件**：`src/services/ImageExportService.cpp` + `include/EnhanceVision/services/ImageExportService.h`

**功能**：
- 提供 Q_INVOKABLE 方法供 QML 调用
- 管理 GPU 渲染导出的异步操作
- 支持导出进度回调

**关键接口**：
```cpp
class ImageExportService : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void exportFromItem(QQuickItem* item, const QString& outputPath);
    Q_INVOKABLE void exportWithShader(
        const QString& imagePath,
        const QVariantMap& shaderParams,
        const QString& outputPath
    );
signals:
    void exportCompleted(const QString& outputPath, bool success, const QString& error);
    void exportProgress(const QString& outputPath, int progress);
};
```

### 步骤 2：创建离屏渲染组件

**文件**：`qml/components/OffscreenShaderRenderer.qml`

**功能**：
- 创建不可见的渲染组件
- 加载图片并应用 Shader 效果
- 使用 grabToImage 导出

**关键实现**：
```qml
Item {
    id: renderer
    visible: false  // 不可见
    
    property var shaderParams: ({})
    property var currentSource: null
    
    Image {
        id: sourceImage
        source: currentSource
        visible: false
    }
    
    FullShaderEffect {
        source: sourceImage
        // 绑定所有 shader 参数
    }
    
    function exportImage(outputPath) {
        grabToImage(function(result) {
            result.saveToFile(outputPath)
            // 发送完成信号
        }, Qt.size(sourceImage.width, sourceImage.height))
    }
}
```

### 步骤 3：修改 ProcessingController

**文件**：`src/controllers/ProcessingController.cpp`

**修改内容**：
- 移除 CPU 算法处理逻辑（`ImageUtils::applyShaderEffects`）
- 改为调用 GPU 渲染导出服务
- 处理异步导出完成回调

**修改前**：
```cpp
// 使用 CPU 算法处理
QImage processedImage = ImageUtils::applyShaderEffects(
    originalImage, brightness, contrast, ...);
processedImage.save(processedPath);
```

**修改后**：
```cpp
// 发送信号到 QML 进行 GPU 渲染导出
emit requestShaderExport(fileId, mf.filePath, shaderParams, processedPath);
// 等待导出完成回调
```

### 步骤 4：在 App.qml 中集成导出服务

**文件**：`qml/App.qml`

**修改内容**：
- 添加离屏渲染组件实例
- 连接 ProcessingController 的导出请求信号
- 处理导出完成回调

### 步骤 5：修改 MediaViewerWindow 支持导出

**文件**：`qml/components/MediaViewerWindow.qml`

**修改内容**：
- 添加 `exportCurrentImage()` 方法
- 使用 grabToImage 导出当前显示的图片（含 Shader 效果）

### 步骤 6：更新缩略图生成逻辑

**文件**：`src/providers/ThumbnailProvider.cpp`

**修改内容**：
- 对于已处理的图片，从 GPU 渲染结果生成缩略图
- 或在导出完成后自动生成缩略图

### 步骤 7：清理旧代码

**需要删除/修改的代码**：
- `ImageUtils::applyShaderEffects` 方法（保留声明但标记废弃，或完全移除）
- ProcessingController 中调用 CPU 算法的代码

---

## 详细实现计划

### 任务 1：创建 ImageExportService

1. 创建头文件 `include/EnhanceVision/services/ImageExportService.h`
2. 创建实现文件 `src/services/ImageExportService.cpp`
3. 在 CMakeLists.txt 中添加新文件
4. 在 main.cpp 中注册服务

### 任务 2：创建离屏渲染组件

1. 创建 `qml/components/OffscreenShaderRenderer.qml`
2. 实现图片加载和 Shader 应用
3. 实现 grabToImage 导出功能

### 任务 3：修改 ProcessingController

1. 添加 `requestShaderExport` 信号
2. 添加 `onShaderExportCompleted` 槽函数
3. 修改 `completeTask` 方法，移除 CPU 处理逻辑
4. 添加导出状态管理（防止并发冲突）

### 任务 4：集成到 App.qml

1. 添加 OffscreenShaderRenderer 实例
2. 连接 ProcessingController 信号
3. 实现导出请求处理

### 任务 5：修改 MediaViewerWindow

1. 添加导出功能
2. 确保导出时使用当前 Shader 参数

### 任务 6：更新缩略图逻辑

1. 在导出完成后自动生成缩略图
2. 更新 ThumbnailProvider 缓存

### 任务 7：清理代码

1. 移除 `ImageUtils::applyShaderEffects` 实现
2. 清理相关调用代码
3. 更新注释和文档

---

## 技术细节

### grabToImage 使用注意事项

1. **异步操作**：grabToImage 是异步的，需要通过回调处理结果
2. **分辨率**：可以指定导出分辨率，应与原图一致
3. **内存管理**：大图片导出时注意 GPU 内存
4. **线程安全**：确保在 GUI 线程调用

### 导出流程

```
用户点击发送
    ↓
ProcessingController 接收任务
    ↓
任务进入队列，开始处理
    ↓
completeTask 触发
    ↓
发送 requestShaderExport 信号
    ↓
QML OffscreenShaderRenderer 接收
    ↓
加载原图 → 应用 Shader → grabToImage
    ↓
保存到缓存目录
    ↓
发送 exportCompleted 信号
    ↓
更新 MessageModel 文件状态
    ↓
生成缩略图
```

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| 大图片导出内存不足 | 分块渲染或降级到 CPU 处理 |
| grabToImage 失败 | 添加错误处理和重试机制 |
| 异步导出顺序问题 | 使用队列管理导出任务 |
| GPU 不支持某些格式 | 检测并回退到兼容格式 |

---

## 测试计划

1. **功能测试**：
   - 单张图片导出
   - 批量图片导出
   - 不同 Shader 参数组合

2. **一致性测试**：
   - 对比预览效果和导出文件
   - 像素级对比（允许微小误差）

3. **性能测试**：
   - 导出速度
   - 内存占用
   - GPU 利用率

4. **边界测试**：
   - 超大图片（>4096×4096）
   - 特殊格式图片
   - 并发导出

---

## 预期效果

1. **100% 一致性**：预览和导出使用完全相同的 GPU 渲染管线
2. **更好的画质**：GPU 渲染效果更细腻、清晰
3. **代码简化**：无需维护两套算法实现
4. **维护成本降低**：修改 Shader 即可同时影响预览和导出
