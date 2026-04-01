# 缩略图显示修复 - 完整解决方案

## 概述

**创建日期**: 2025-04-02  
**问题描述**: 消息卡片上的缩略图在多种场景下显示为灰色占位图

---

## 一、变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/providers/ThumbnailProvider.h` | 新增 `normalizeKey()`、`invalidateThumbnail()`、`hasThumbnail()` 方法，添加 `m_failedKeys` 成员 |
| `src/providers/ThumbnailProvider.cpp` | 实现归一化缓存键、失败追踪、主动失效功能 |
| `qml/components/MessageList.qml` | 移除 `_buildMediaForDelegate` 和 `_patchCachedMediaFile` 中的 URL 构建逻辑 |
| `qml/components/MediaThumbnailStrip.qml` | 统一 URL 构建、简化 delegate、添加重试机制和初始化检查 |

---

## 二、问题根因分析

### 原始问题
1. **URL 构建分散** - 缩略图 URL 在多处构建（`MessageList`、`MediaThumbnailStrip`），逻辑不一致
2. **缓存键不归一化** - 相同文件可能产生不同的缓存键（编码差异、路径格式差异）
3. **信号丢失** - 会话切换后 delegate 重建，错过了之前发出的 `thumbnailReady` 信号
4. **无重试机制** - 缩略图加载失败后没有自动重试

---

## 三、实现的功能特性

### C++ 端 (ThumbnailProvider)

- ✅ **归一化缓存键** (`normalizeKey`)：统一处理查询参数、`processed_` 前缀、文件路径格式
- ✅ **失败追踪** (`m_failedKeys`)：记录生成失败的键，防止无限重试
- ✅ **主动失效** (`invalidateThumbnail`)：允许 QML 强制刷新特定缩略图
- ✅ **缓存查询** (`hasThumbnail`)：允许 QML 检查缩略图是否已缓存
- ✅ **始终返回占位图**：避免 QML Image 进入 Error 状态

### QML 端

- ✅ **单一 URL 构建入口** (`_thumbnailSourceForItem`)：统一在 `MediaThumbnailStrip` 中构建
- ✅ **简化缓存键计算** (`_cacheKeyForRow`)：与 C++ `normalizeKey` 逻辑对齐
- ✅ **初始版本号为 1**：确保 delegate 创建时立即尝试加载
- ✅ **延迟初始化检查** (`thumbInitTimer`)：100ms 后检查图像是否正确加载
- ✅ **错误重试机制** (`thumbRetryTimer`)：最多 3 次重试，间隔递增

---

## 四、技术实现细节

### 缓存键归一化流程

```cpp
QString ThumbnailProvider::normalizeKey(const QString &rawId)
{
    QString key = rawId;
    
    // 1. 去掉查询参数 (?v=N)
    int queryPos = key.indexOf('?');
    if (queryPos > 0) {
        key = key.left(queryPos);
    }
    
    // 2. processed_ 前缀保留原样（逻辑 ID）
    if (key.startsWith(QStringLiteral("processed_"))) {
        return key;
    }
    
    // 3. 对文件路径进行归一化
    return normalizeFilePath(key);
}
```

### QML 缩略图 URL 构建

```javascript
function _thumbnailSourceForItem(itemData, isSuccess) {
    if (!itemData) return ""

    var resourceId = ""

    // 已完成的文件：优先使用处理后的缩略图 ID
    if (root.messageMode && isSuccess) {
        if (itemData.processedThumbnailId && itemData.processedThumbnailId !== "") {
            resourceId = itemData.processedThumbnailId
        } else if (itemData.resultPath && itemData.resultPath !== "") {
            resourceId = itemData.resultPath
        }
    }

    // 回退：使用原始文件路径
    if (resourceId === "") {
        resourceId = itemData.filePath || ""
    }

    if (resourceId === "") return ""

    var version = itemData.thumbVersion || 0
    return "image://thumbnail/" + resourceId + "?v=" + version
}
```

### 重试机制

```javascript
Component.onCompleted: {
    thumbInitTimer.start()  // 100ms 后检查
}

Timer {
    id: thumbInitTimer
    interval: 100
    onTriggered: {
        if (thumbImage.status !== Image.Ready && thumbImage.source !== "") {
            // 递增 thumbVersion 触发重新加载
            filteredModel.setProperty(idx, "thumbVersion", (row.thumbVersion || 0) + 1)
        }
    }
}
```

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 正在处理的文件 | 显示原始文件缩略图 | ✅ 通过 |
| 等待处理的文件 | 显示原始文件缩略图 | ✅ 通过 |
| 处理完成的文件 | 显示处理后的缩略图 | ✅ 通过 |
| 会话切换后返回 | 缩略图正常显示 | ✅ 通过 |
| 缩略图生成失败 | 显示占位图，不无限重试 | ✅ 通过 |

---

## 六、架构改进

### 数据流简化

**修改前**：
```
MessageList._buildMediaForDelegate → 构建 thumbSource URL
MessageList._patchCachedMediaFile → 构建 thumbSource URL
MediaThumbnailStrip._thumbnailSourceForItem → 再次构建/修改 URL
```

**修改后**：
```
MessageList → 只存储原始数据 (filePath, resultPath, processedThumbnailId)
MediaThumbnailStrip._thumbnailSourceForItem → 唯一的 URL 构建入口
```

### 单一数据源原则

- `thumbnail` 字段不再存储预构建的 URL
- URL 构建完全由 `_thumbnailSourceForItem` 负责
- `thumbVersion` 驱动 Image 刷新

---

## 七、后续工作

- [ ] 监控长期稳定性
- [ ] 考虑添加缩略图预加载机制
- [ ] 优化大量文件时的缩略图生成性能
