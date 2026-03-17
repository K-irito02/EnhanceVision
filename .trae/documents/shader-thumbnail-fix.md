# Shader 处理后缩略图显示问题修复计划

## 问题分析

### 问题现象
发送后缩略图还是显示的原图，没有生成应用了效果的缩略图。

### 根本原因
存在**两个数据获取路径不一致**的问题：

1. **`MessageModel::data()` 方法**（第 44-60 行）：
   - 这是 QML 通过 `model.mediaFiles` 访问数据的地方
   - 它直接返回 `message.mediaFiles` 的数据
   - **没有**添加 `processedThumbnailId` 字段

2. **`MessageModel::getMediaFiles()` 方法**（第 451-481 行）：
   - 这是一个单独的 Q_INVOKABLE 方法
   - 它**确实**添加了 `processedThumbnailId` 字段
   - 但这个方法只在 `_openViewer` 函数中被调用，用于打开查看器窗口

3. **MessageList.qml** 中的 `_buildMediaForDelegate` 函数使用的是 `model.mediaFiles`，这直接调用 `MessageModel::data()` 方法，而不是 `getMediaFiles()` 方法。

### 数据流分析

```
QML: model.mediaFiles
    ↓
MessageModel::data() [MediaFilesRole]
    ↓
返回 message.mediaFiles（没有 processedThumbnailId）
    ↓
MessageList.qml: _buildMediaForDelegate()
    ↓
f.processedThumbnailId 为 undefined
    ↓
使用原图缩略图
```

---

## 解决方案

### 方案 A：修改 MessageModel::data() 方法（推荐）

**优点：**
- 修改范围小，只需修改一处
- 保持现有架构不变
- 性能影响最小

**缺点：**
- `data()` 方法会被频繁调用，需要确保效率

**实现步骤：**
1. 在 `MessageModel::data()` 的 `MediaFilesRole` 分支中添加 `processedThumbnailId` 字段
2. 复用 `getMediaFiles()` 中的逻辑

### 方案 B：修改 QML 使用 getMediaFiles() 方法

**优点：**
- 复用已有代码
- 逻辑集中

**缺点：**
- 需要修改 QML 绑定方式
- 需要在消息创建时调用额外方法
- 可能影响性能（每次都需要调用 Q_INVOKABLE）

### 方案 C：在消息创建时预计算 processedThumbnailId

**优点：**
- 只计算一次，后续访问快速

**缺点：**
- 需要修改 MediaFile 结构体
- 需要在处理完成时更新
- 修改范围较大

---

## 推荐方案：方案 A

修改 `MessageModel::data()` 方法，在 `MediaFilesRole` 分支中添加 `processedThumbnailId` 字段。

### 具体修改

**文件：** `src/models/MessageModel.cpp`

**修改前（第 44-60 行）：**
```cpp
case MediaFilesRole: {
    QVariantList filesList;
    for (const MediaFile &file : message.mediaFiles) {
        QVariantMap fileMap;
        fileMap["id"] = file.id;
        fileMap["filePath"] = file.filePath;
        fileMap["fileName"] = file.fileName;
        fileMap["fileSize"] = file.fileSize;
        fileMap["mediaType"] = static_cast<int>(file.type);
        fileMap["thumbnail"] = "";
        fileMap["status"] = static_cast<int>(file.status);
        fileMap["resultPath"] = file.resultPath;
        fileMap["duration"] = file.duration;
        filesList.append(fileMap);
    }
    return filesList;
}
```

**修改后：**
```cpp
case MediaFilesRole: {
    QVariantList filesList;
    
    bool hasShaderModifications = 
        qAbs(message.shaderParams.brightness) > 0.001f ||
        qAbs(message.shaderParams.contrast - 1.0f) > 0.001f ||
        qAbs(message.shaderParams.saturation - 1.0f) > 0.001f;
    
    for (const MediaFile &file : message.mediaFiles) {
        QVariantMap fileMap;
        fileMap["id"] = file.id;
        fileMap["filePath"] = file.filePath;
        fileMap["fileName"] = file.fileName;
        fileMap["fileSize"] = file.fileSize;
        fileMap["mediaType"] = static_cast<int>(file.type);
        fileMap["thumbnail"] = "";
        fileMap["status"] = static_cast<int>(file.status);
        fileMap["resultPath"] = file.resultPath;
        fileMap["duration"] = file.duration;
        
        if (message.mode == ProcessingMode::Shader && hasShaderModifications && file.status == ProcessingStatus::Completed) {
            fileMap["processedThumbnailId"] = "processed_" + file.id;
        }
        
        filesList.append(fileMap);
    }
    return filesList;
}
```

---

## 验证步骤

1. 构建项目
2. 上传图片
3. 调整 Shader 参数（亮度、对比度、饱和度）
4. 发送消息
5. 检查消息列表中的缩略图是否显示处理后的效果

---

## 注意事项

1. **性能考虑**：`data()` 方法会被 QML 频繁调用，`hasShaderModifications` 的计算应该保持简单高效。

2. **线程安全**：`ThumbnailProvider::setThumbnail()` 需要在主线程调用，确保缩略图生成在正确的线程。

3. **缓存失效**：当消息状态变化时，QML 会重新请求 `model.mediaFiles`，此时 `processedThumbnailId` 会正确返回。

4. **代码复用**：可以考虑将 `hasShaderModifications` 的判断逻辑提取为私有方法，避免重复代码。
