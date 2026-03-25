# AI 视频处理崩溃修复

## 概述

**创建日期**: 2026-03-26  
**相关 Issue**: 无

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `src/core/AIEngine.cpp` | 修复视频处理崩溃、优化编码器选择、改进分块处理策略 |

### 新增文件
| 文件路径 | 功能描述 |
|----------|----------|
| `third_party/x264/` | libx264 编码库（MSVC17 预构建版本） |
| `third_party/x264/README.md` | x264 库使用说明 |

---

## 二、实现的功能特性

- ✅ **修复堆损坏崩溃**：解决 `sws_scale` 输出缓冲区与 `QImage::bytesPerLine` 不匹配导致的内存越界
- ✅ **优化编码器选择**：排除不稳定的 `h264_mf` 编码器，优先使用 `libx264`/`h264_nvenc`，回退到 MPEG-4
- ✅ **改进分块处理策略**：视频帧使用单帧处理（更稳定），仅大图像使用分块处理
- ✅ **添加输入验证**：在 `processFrame` 中创建深拷贝确保内存安全
- ✅ **安装 libx264**：添加预构建的 x264 库到 `third_party` 目录

---

## 三、技术实现细节

### 1. 堆损坏问题根因

`sws_scale` 期望输出缓冲区为 `width * 3` 字节/行，但 `QImage::Format_RGB888` 的 `bytesPerLine` 可能因内存对齐而不同，导致写入越界。

**解决方案**：使用独立的 `std::vector<uint8_t>` 缓冲区接收 `sws_scale` 输出，再逐行复制到 `QImage`。

```cpp
std::vector<uint8_t> rgb24Buffer(rgb24LineSize * frameH);
// sws_scale 写入 rgb24Buffer
// 逐行复制到 QImage
for (int y = 0; y < frameH; ++y) {
    std::memcpy(frameImg.scanLine(y), rgb24Buffer.data() + y * rgb24LineSize, rgb24LineSize);
}
```

### 2. 编码器选择策略

| 优先级 | 编码器 | 说明 |
|--------|--------|------|
| 1 | libx264 | GPL 软件编码器（需 GPL FFmpeg） |
| 2 | h264_nvenc | NVIDIA 硬件编码器 |
| 3 | h264_qsv | Intel Quick Sync |
| 4 | h264_amf | AMD 硬件编码器 |
| 5 | libopenh264 | Cisco OpenH264 |
| 6 | mpeg4 | MPEG-4 回退（广泛支持） |

**注意**：`h264_mf`（Windows Media Foundation）在工作线程中不稳定，已排除。

### 3. 分块处理策略

```cpp
const qint64 pixelCount = width * height;
constexpr qint64 kTileThreshold = 1024LL * 1024;  // 1M 像素

if (pixelCount > kTileThreshold) {
    // 大图像：分块处理
} else {
    // 视频帧/小图像：单帧处理（更稳定）
}
```

---

## 四、遇到的问题及解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 0xc0000374 堆损坏 | `sws_scale` 输出缓冲区对齐问题 | 使用独立缓冲区 + 逐行复制 |
| h264_mf 崩溃 | MF 编码器在工作线程不稳定 | 排除 h264_mf，回退到 MPEG-4 |
| 分块处理崩溃 | padding/mirroring 边界条件问题 | 视频帧使用单帧处理 |

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 视频 AI 增强处理 | 完成处理无崩溃 | ✅ 通过（150+ 帧） |
| 进度条更新 | UI 实时显示进度 | ✅ 通过 |
| 编码器选择 | 使用稳定编码器 | ✅ 通过（MPEG-4） |

---

## 六、后续更新 (2026-03-26)

### 已完成

- ✅ **启用 h264_nvenc 硬件编码**：NVIDIA GPU 加速，编码速度大幅提升
- ✅ **集成 FFmpeg GPL 构建**：替换为 BtbN 的 `ffmpeg-master-latest-win64-gpl-shared`，支持 libx264 和 nvenc
- ✅ **修复分块处理角落镜像**：添加四个角落的镜像填充，避免黑色伪影导致失真
- ✅ **添加诊断日志**：帧质量检测、编码错误详情、分块处理状态

### 编码器配置

| 编码器 | 参数 | 说明 |
|--------|------|------|
| h264_nvenc | preset=p4, rc=vbr, cq=23 | NVIDIA 硬件编码 |
| libx264 | preset=medium, crf=18 | GPL 软件编码 |
| h264_qsv | preset=medium, global_quality=23 | Intel Quick Sync |
| h264_amf | quality=balanced | AMD 硬件编码 |
| mpeg4 | q:v=5 | 回退编码器 |

### FFmpeg 版本

- **版本**: ffmpeg-master-latest-win64-gpl-shared (BtbN)
- **位置**: `third_party/ffmpeg/`
- **特性**: `--enable-libx264 --enable-nvenc --enable-gpl`

### 测试结果

- 视频处理：210 帧，0 失败，h264_nvenc 编码
- 图像处理：分块处理正常，无角落伪影
