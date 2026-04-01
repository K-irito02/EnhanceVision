# 缓存管理清理功能问题修复计划

## 问题概述

用户报告了设置中"缓存管理"清理功能的三个问题：
1. 清理视频类型文件时，需要多次重复清理才能成功
2. 清理后，会话标签中还存在相关消息，缩略图还是正常显示
3. 混合消息中删除视频后，图片类型的文件缩略图显示灰色

## 问题分析

### 问题1：视频文件需要多次清理

**根本原因**：
- `SettingsController::clearDirectory()` 方法在删除文件时，如果文件被占用（视频播放器、缩略图生成器等），删除会失败
- 删除失败后只打印警告，没有重试或等待机制
- 即使文件删除失败，仍然调用 `clearMediaFilesByModeAndType()` 更新会话数据，导致数据不一致

**代码位置**：`src/controllers/SettingsController.cpp` 第483-515行

### 问题2：清理后消息和缩略图未同步

**根本原因**：
- `SessionController::clearMediaFilesByModeAndType()` 正确更新了会话数据
- 但没有清除 `ThumbnailProvider` 中的缩略图缓存
- QML 界面使用缓存的缩略图数据，显示已删除文件的缩略图

**代码位置**：
- `src/controllers/SessionController.cpp` 第1528-1604行
- `src/providers/ThumbnailProvider.cpp` 缺少批量清除接口

### 问题3：混合消息中图片缩略图显示灰色

**根本原因**：
- 删除部分文件后，消息的 `mediaFiles` 列表被更新
- `MediaThumbnailStrip.qml` 的 `filteredModel` 更新逻辑可能存在问题
- 缩略图缓存未刷新，导致显示异常

**代码位置**：
- `qml/components/MediaThumbnailStrip.qml` 更新逻辑
- `src/models/MessageModel.cpp` 的 `setMessages` 方法

## 修复方案

### 修复1：增强文件删除可靠性

**文件**：`src/controllers/SettingsController.cpp`

**修改内容**：
1. 在 `clearDirectory()` 中添加重试机制
2. 对于删除失败的文件，等待短暂时间后重试
3. 确保只有在文件真正删除成功后才更新会话数据

```cpp
// 伪代码示例
bool SettingsController::clearDirectory(const QString& path) {
    // ... 收集要删除的文件列表
    
    for (const QString& item : itemsToDelete) {
        bool deleted = false;
        for (int retry = 0; retry < 3 && !deleted; ++retry) {
            if (QFile::remove(item)) {
                deleted = true;
            } else {
                QThread::msleep(100); // 等待100ms后重试
            }
        }
        if (!deleted) {
            qWarning() << "Failed to remove file after retries:" << item;
            success = false;
        }
    }
    return success;
}
```

### 修复2：清理缩略图缓存

**文件**：
- `src/providers/ThumbnailProvider.h`
- `src/providers/ThumbnailProvider.cpp`
- `src/controllers/SessionController.cpp`

**修改内容**：
1. 在 `ThumbnailProvider` 中添加按路径前缀清除缓存的方法
2. 在 `clearMediaFilesByModeAndType()` 中调用清除缩略图缓存

```cpp
// ThumbnailProvider.h 新增方法
void clearThumbnailsByPathPrefix(const QString& pathPrefix);

// SessionController.cpp 在 clearMediaFilesByModeAndType 中调用
if (hasChanges) {
    // 清除相关缩略图缓存
    ThumbnailProvider* provider = ThumbnailProvider::instance();
    if (provider) {
        QString cachePath = getCachePathForModeAndType(targetMode, targetMediaType);
        provider->clearThumbnailsByPathPrefix(cachePath);
    }
    // ... 其他更新逻辑
}
```

### 修复3：修复消息模型更新和缩略图刷新

**文件**：
- `src/models/MessageModel.cpp`
- `qml/components/MediaThumbnailStrip.qml`

**修改内容**：
1. 在 `MessageModel::setMessages()` 中确保正确发出 `dataChanged` 信号
2. 在 QML 中监听 `mediaFiles` 变化并刷新缩略图版本号

```cpp
// MessageModel.cpp - 确保在文件列表变化时发出正确的信号
if (mediaFilesChanged) {
    emit messageMediaFilesReloaded(m_messages[i].id);
}
```

### 修复4：统一清理流程

**修改 `SettingsController` 的清理方法**：

```cpp
bool SettingsController::clearAIVideoData() {
    QString path = getAIVideoPath();
    
    // 1. 先清除缩略图缓存
    if (m_thumbnailProvider) {
        m_thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    // 2. 删除文件（带重试）
    bool success = clearDirectory(path);
    
    // 3. 更新会话数据
    if (success && m_sessionController) {
        m_sessionController->clearMediaFilesByModeAndType(1, 1);
    }
    
    refreshDataSize();
    return success;
}
```

## 实施步骤

### 步骤1：修改 ThumbnailProvider
1. 添加 `clearThumbnailsByPathPrefix()` 方法
2. 添加 `clearThumbnailsByModeAndType()` 方法（根据模式类型清除）

### 步骤2：修改 SettingsController
1. 增强 `clearDirectory()` 的重试机制
2. 在清理方法中先清除缩略图缓存再删除文件
3. 添加 `setThumbnailProvider()` 方法

### 步骤3：修改 SessionController
1. 在 `clearMediaFilesByModeAndType()` 中清除缩略图缓存
2. 确保消息模型正确更新

### 步骤4：修改 MessageModel
1. 确保 `setMessages()` 正确处理文件列表变化
2. 发出正确的信号通知 QML 刷新

### 步骤5：修改 MediaThumbnailStrip.qml
1. 监听 `messageMediaFilesReloaded` 信号
2. 刷新缩略图版本号

## 测试验证

1. **测试场景1**：清理 AI 视频缓存
   - 验证文件被正确删除
   - 验证会话标签中相关消息被移除
   - 验证缩略图不再显示

2. **测试场景2**：清理 Shader 图片缓存
   - 验证文件被正确删除
   - 验证消息列表正确更新

3. **测试场景3**：混合消息删除视频
   - 验证视频文件被删除
   - 验证图片文件缩略图正常显示
   - 验证消息状态正确更新

## 风险评估

- **低风险**：缩略图缓存清除逻辑独立，不影响其他功能
- **中风险**：文件删除重试可能增加清理时间，需要添加进度提示
- **注意**：确保在主线程外执行文件操作，避免 UI 卡顿

## 预计工作量

- ThumbnailProvider 修改：约 30 行代码
- SettingsController 修改：约 50 行代码
- SessionController 修改：约 20 行代码
- MessageModel 修改：约 10 行代码
- QML 修改：约 20 行代码
- 测试验证：约 1 小时
