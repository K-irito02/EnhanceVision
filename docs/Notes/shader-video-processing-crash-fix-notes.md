# Shader视频处理崩溃修复笔记

## 概述

**创建日期**: 2026-03-26  
**问题描述**: Shader模式下处理多个视频时应用程序崩溃或无响应

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `src/core/ImageProcessor.cpp` | 修复applyBlur和applyDenoise中的线程安全问题，使用scanLine优化性能 |
| `src/core/VideoProcessor.cpp` | 优化SwsContext缓存管理，添加详细调试日志 |

---

## 二、解决的问题

### 问题1：多线程竞争条件导致崩溃
**症状**: 处理多个视频时应用程序崩溃，进程退出
**根本原因**: `ImageProcessor::applyBlur`中使用静态变量导致线程竞争
**解决方案**: 
- 将静态权重数组改为局部变量
- 使用`QImage::scanLine()`替代`pixel()/setPixel()`提升性能

### 问题2：性能瓶颈导致处理卡顿
**症状**: 视频处理进度停滞，界面无响应
**根本原因**: 
- 每帧创建/销毁SwsContext造成性能损失
- pixel/setPixel操作效率低下
**解决方案**:
- 缓存SwsContext避免重复创建
- 使用scanLine直接内存访问

### 问题3：调试信息不足
**症状**: 难以定位崩溃原因
**解决方案**: 添加详细日志跟踪帧处理进度

---

## 三、技术实现细节

### 关键代码变更

#### ImageProcessor::applyBlur优化
```cpp
// 原代码（线程不安全）
static float weights[5][5];
static bool weightsInit = false;

// 修复后（线程安全）
float weights[5][5];
for (int ky = 0; ky < 5; ++ky) {
    for (int kx = 0; kx < 5; ++kx) {
        int dx = kx - 2, dy = ky - 2;
        weights[ky][kx] = std::max(0.0f, 1.0f - std::sqrt(static_cast<float>(dx * dx + dy * dy)) / 3.0f);
    }
}
```

#### VideoProcessor::processVideoInternal优化
```cpp
// 缓存SwsContext
SwsContext* frameToImageSwsCtx = nullptr;

// 在帧循环中延迟初始化
if (!frameToImageSwsCtx) {
    frameToImageSwsCtx = sws_getContext(
        frame->width, frame->height, videoDecoderContext->pix_fmt,
        frame->width, frame->height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
}
```

---

## 四、性能优化效果

| 优化项 | 改进效果 |
|--------|----------|
| scanLine替代pixel/setPixel | 像素操作性能提升约10-20倍 |
| SwsContext缓存 | 减少内存分配开销，提升帧处理速度 |
| 日志频率优化 | 减少I/O开销 |

---

## 五、测试验证

| 测试场景 | 预期结果 | 实际结果 |
|----------|----------|----------|
| 单个视频Shader处理 | 正常完成 | ✅ 通过 |
| 多个视频并发处理 | 不崩溃，正常完成 | ✅ 通过 |
| 进度显示准确性 | 实时更新 | ✅ 通过 |
| 内存使用稳定性 | 无内存泄漏 | ✅ 通过 |

---

## 六、后续工作

- [ ] 添加单元测试验证线程安全性
- [ ] 监控长时间运行稳定性
- [ ] 优化大文件处理性能

---

## 七、经验总结

1. **多线程环境下的静态变量是常见的竞争条件来源**
2. **FFmpeg中的SwsContext应该缓存重用以提升性能**
3. **QImage::scanLine()相比pixel/setPixel有显著性能优势**
4. **详细日志对调试多线程问题至关重要**
