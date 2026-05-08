# Shader 模式视频/图片处理性能优化 Spec

## Why
Shader 模式处理大视频（如 3.5GB）时速度极慢，用户体验不可接受。当前实现存在多个性能瓶颈：CPU 单线程像素处理、编码器配置保守、每帧重复内存分配、不必要的格式转换等。

## What Changes
- 优化 FFmpeg 编码器配置：使用 faster preset、合理 CRF、硬件编码器自动检测
- 消除每帧不必要的内存分配和格式转换
- 预计算查找表（LUT）替代逐像素 `std::pow()` 调用
- 使用 `SWS_FAST_BILINEAR` 替代 `SWS_BILINEAR` 加速格式转换
- 优化 `ImageProcessor::applyShader()` 内部循环，减少浮点运算开销
- 为图片处理添加并行行处理能力

## Impact
- Affected specs: 视频处理管线、图像处理管线
- Affected code:
  - `src/core/VideoProcessor.cpp` - 编码器配置、帧处理循环
  - `src/core/ImageProcessor.cpp` - 像素处理优化、LUT、并行处理
  - `include/EnhanceVision/core/VideoProcessor.h` - 可能新增硬件编码器检测方法
  - `include/EnhanceVision/core/ImageProcessor.h` - 可能新增 LUT 和缓冲区成员

## ADDED Requirements

### Requirement: 硬件编码器自动检测与回退
系统 SHALL 自动检测可用的硬件视频编码器（h264_nvenc → h264_qsv → h264_amf），并在不可用时回退到 libx264 软件编码器。

#### Scenario: 检测到 NVIDIA GPU
- **WHEN** 系统检测到 h264_nvenc 可用
- **THEN** 使用 h264_nvenc 编码器，配置 preset=p4, rc=vbr, quality=balanced

#### Scenario: 无硬件编码器
- **WHEN** 所有硬件编码器不可用
- **THEN** 回退到 libx264，配置 preset=fast, crf=23

### Requirement: 编码器性能配置优化
系统 SHALL 使用更高效的编码器参数：
- 软件编码器：preset 从 "medium" 改为 "fast"，CRF 从 18 改为 23
- 硬件编码器：使用硬件加速预设
- 编码线程数上限从 8 提升到 CPU 核心数

### Requirement: 帧处理缓冲区复用
系统 SHALL 在视频帧处理循环中复用预分配的缓冲区，避免每帧重复分配和释放内存。

#### Scenario: 处理连续视频帧
- **WHEN** 处理视频的每一帧
- **THEN** neighborPixels 缓冲区只分配一次，跨帧复用

### Requirement: Gamma 查找表（LUT）
系统 SHALL 预计算 gamma 校正的 256 项查找表，避免逐像素调用 `std::pow()`。

#### Scenario: 应用 gamma 校正
- **WHEN** gamma 参数非 1.0 时
- **THEN** 使用预计算 LUT 查表替代 `std::pow()` 计算

### Requirement: 消除不必要的格式转换
系统 SHALL 在视频帧处理中跳过已为 RGB32 格式的 QImage 的 `convertToFormat()` 调用，并使用 `SWS_FAST_BILINEAR` 替代 `SWS_BILINEAR`。

### Requirement: 图片并行处理
系统 SHALL 对大图片（像素数超过阈值）使用 `QtConcurrent::mapped` 并行处理行数据。

#### Scenario: 处理大图片
- **WHEN** 图片像素数超过 200 万（约 1920x1080）
- **THEN** 使用多线程并行处理行数据

## MODIFIED Requirements

### Requirement: VideoProcessor 编码器初始化
编码器初始化 SHALL 优先尝试硬件编码器，回退到优化的软件编码器配置。编码线程数不再限制为 8，而是使用 `QThread::idealThreadCount()`。

### Requirement: ImageProcessor::applyShader 性能
`applyShader()` SHALL 使用预分配缓冲区和 LUT 优化，避免每帧重复分配 neighborPixels 向量，避免逐像素 pow() 计算。
