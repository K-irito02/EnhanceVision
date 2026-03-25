# AI模式视频处理崩溃问题调查与修复计划

## 问题描述

在使用AI模式进行视频类型的多媒体文件处理时，应用程序崩溃。图片处理正常，只有视频处理会崩溃。

## 日志分析

### 最后的日志输出
```
[2026-03-25 23:55:24.371] [INFO] [AIEngine][processFrame] start inputSize: QSize(360, 514) tileSize: 200 model: "realesr_animevideov3_x2" scale: 2
[2026-03-25 23:55:24.371] [INFO] [AIEngine][processFrame] inference needTile: true effectiveTileSize: 200
[2026-03-25 23:55:24.371] [INFO] [AIEngine][TiledNoProgress] entry inputSize: 360 x 514 inputFormat: QImage::Format_RGB32 tileSize: 200 scale: 2 layerCount: 41
```

### 关键发现
1. 程序在 `processTiledNoProgress` 函数的 entry 日志后崩溃
2. **视频帧的输入格式是 `QImage::Format_RGB32`**（每像素4字节）
3. 没有打印 "start processing" 日志，说明崩溃发生在 entry 和 "start processing" 之间
4. 图片处理正常，视频处理崩溃

## 根本原因分析

### 崩溃位置定位

崩溃发生在 [AIEngine.cpp:1404-1498](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/AIEngine.cpp#L1404) 之间的代码：

```cpp
// 1404-1407: tileSize 检查
if (tileSize <= 0) {
    return processSingleNoProgress(input, model);
}

// 1409-1419: padding 计算
int padding = model.tilePadding;
// ...

// 1424-1430: 格式转换 - 关键点！
QImage normalizedInput = input.format() == QImage::Format_RGB888 
                        ? input 
                        : input.convertToFormat(QImage::Format_RGB888);

// 1432-1437: paddedInput 创建
QImage paddedInput(w + 2 * padding, h + 2 * padding, QImage::Format_RGB888);

// 1441-1480: 像素复制操作
for (int y = 0; y < h; ++y) {
    const uchar* srcLine = normalizedInput.constScanLine(y);
    uchar* dstLine = paddedInput.scanLine(y + padding);
    std::memcpy(dstLine + padding * 3, srcLine, w * 3);  // 问题所在！
}
```

### 根本原因

**像素格式不匹配导致的内存访问错误**：

1. 视频帧来自 FFmpeg 解码，格式为 `Format_RGB32`（每像素4字节，BGRA或ARGB）
2. `convertToFormat(QImage::Format_RGB888)` 将其转换为 `Format_RGB888`（每像素3字节，RGB）
3. 但在像素复制循环中：
   - `srcLine` 指向 `normalizedInput` 的扫描线
   - 如果转换失败或部分成功，`srcLine` 的每行字节数可能不是 `w * 3`
   - `std::memcpy(dstLine + padding * 3, srcLine, w * 3)` 可能读取越界

**更深层的问题**：

查看 `processTiled`（图片处理使用）和 `processTiledNoProgress`（视频处理使用）的代码，它们几乎相同，但：

1. 图片处理路径：`process()` -> `processTiled()` - 工作正常
2. 视频处理路径：`processFrame()` -> `processTiledNoProgress()` - 崩溃

**关键差异**：视频帧的 `Format_RGB32` 格式可能包含特殊的字节对齐或填充，导致 `bytesPerLine()` 与 `width() * 4` 不同。

### 验证假设

在 [AIEngine.cpp:1444](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/AIEngine.cpp#L1444)：
```cpp
std::memcpy(dstLine + padding * 3, srcLine, w * 3);
```

如果 `normalizedInput` 的 `bytesPerLine()` 不等于 `w * 3`（由于对齐填充），这个拷贝可能：
- 读取到无效内存
- 或者读取到下一行的数据

## 修复方案

### 方案：使用正确的字节宽度进行拷贝

修改像素复制逻辑，使用实际的 `bytesPerLine()` 而不是假定的 `w * 3`：

```cpp
// 修复前
std::memcpy(dstLine + padding * 3, srcLine, w * 3);

// 修复后
const int srcBytesPerLine = normalizedInput.bytesPerLine();
const int copyBytes = std::min(w * 3, srcBytesPerLine);
std::memcpy(dstLine + padding * 3, srcLine, copyBytes);
```

### 完整修复步骤

1. **增加详细日志**：在关键位置添加日志，确认崩溃点
2. **修复格式转换后的检查**：确保转换成功且格式正确
3. **修复像素复制逻辑**：使用正确的字节宽度
4. **添加防御性检查**：在所有内存操作前检查边界

## 实施步骤

### 步骤1: 增加诊断日志
在 `processTiledNoProgress` 中增加详细日志：
- 格式转换前后的状态
- `bytesPerLine()` 值
- paddedInput 创建状态

### 步骤2: 修复像素复制逻辑
- 使用 `bytesPerLine()` 获取实际行宽
- 添加边界检查

### 步骤3: 增强错误处理
- 检查 `convertToFormat` 返回值
- 检查 `paddedInput` 创建是否成功
- 添加 try-catch 保护

### 步骤4: 验证修复
- 构建并运行项目
- 测试视频处理功能
- 检查日志确认无崩溃

## 预期修改文件

| 文件 | 修改内容 |
|------|----------|
| `src/core/AIEngine.cpp` | 修复 `processTiledNoProgress()` 函数 |

## 具体代码修改

### 修改位置: [AIEngine.cpp:1424-1445](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/core/AIEngine.cpp#L1424)

```cpp
// 修复后的代码
QImage normalizedInput;
if (input.format() == QImage::Format_RGB888) {
    normalizedInput = input;
} else {
    normalizedInput = input.convertToFormat(QImage::Format_RGB888);
    qInfo() << "[AIEngine][TiledNoProgress] format converted"
            << "from:" << input.format()
            << "to:" << normalizedInput.format()
            << "bytesPerLine:" << normalizedInput.bytesPerLine()
            << "expectedBytesPerLine:" << (w * 3);
}

if (normalizedInput.isNull() || normalizedInput.format() != QImage::Format_RGB888) {
    qWarning() << "[AIEngine][TiledNoProgress] failed to normalize input format"
               << "isNull:" << normalizedInput.isNull()
               << "format:" << normalizedInput.format();
    return processSingleNoProgress(input, model);
}

// 使用正确的字节宽度进行复制
const int srcBytesPerLine = normalizedInput.bytesPerLine();
const int expectedBytesPerLine = w * 3;

for (int y = 0; y < h; ++y) {
    const uchar* srcLine = normalizedInput.constScanLine(y);
    uchar* dstLine = paddedInput.scanLine(y + padding);
    // 使用实际的字节宽度，而不是假定的 w * 3
    if (srcBytesPerLine >= expectedBytesPerLine) {
        std::memcpy(dstLine + padding * 3, srcLine, expectedBytesPerLine);
    } else {
        // 字节宽度不足，填充剩余部分
        std::memcpy(dstLine + padding * 3, srcLine, srcBytesPerLine);
        std::memset(dstLine + padding * 3 + srcBytesPerLine, 0, expectedBytesPerLine - srcBytesPerLine);
    }
}
```

## 风险评估

- **低风险**: 增加日志和防御性检查
- **低风险**: 使用 `bytesPerLine()` 替代假定值
- **测试范围**: 图片处理、视频处理、不同分辨率和格式

## 后续验证

1. 构建项目
2. 运行应用程序
3. 测试视频AI处理功能
4. 检查日志文件确认无崩溃
5. 等待60秒进行手动测试
6. 再次检查日志确认无警告或错误
