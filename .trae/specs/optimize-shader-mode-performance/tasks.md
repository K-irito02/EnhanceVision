# Tasks

- [x] Task 1: 优化 FFmpeg 编码器配置（软件编码器）
  - [x] SubTask 1.1: 将 H.264 preset 从 "medium" 改为 "fast"
  - [x] SubTask 1.2: 将 CRF 从 18 改为 23
  - [x] SubTask 1.3: 移除 thread_count 的 8 上限，使用 QThread::idealThreadCount()
  - [x] SubTask 1.4: 将 max_b_frames 从 2 改为 0（减少编码延迟）
- [x] Task 2: 添加硬件编码器自动检测与回退
  - [x] SubTask 2.1: 在 VideoProcessor 中添加 findAvailableEncoder() 方法，按优先级检测 h264_nvenc → h264_qsv → h264_amf → libx264
  - [x] SubTask 2.2: 为硬件编码器配置合适的参数（preset=p4/hp, rc=vbr 等）
  - [x] SubTask 2.3: 确保硬件编码器不可用时自动回退到优化的软件编码器
- [x] Task 3: 消除帧处理中的不必要开销
  - [x] SubTask 3.1: 移除 applyShader 返回后的 convertToFormat(RGB32) 调用（输入已是 RGB32）
  - [x] SubTask 3.2: 将 sws_getContext 的 flags 从 SWS_BILINEAR 改为 SWS_FAST_BILINEAR
- [x] Task 4: 预分配和复用帧处理缓冲区
  - [x] SubTask 4.1: 在 ImageProcessor 中添加 m_neighborPixels/m_originalPixels 成员变量和 ensureNeighborBuffers() 方法
  - [x] SubTask 4.2: 在 applyShader 中使用成员变量替代局部变量
  - [x] SubTask 4.3: 缓冲区跨帧复用，仅在尺寸变化时重新分配
- [x] Task 5: 实现 Gamma LUT 优化
  - [x] SubTask 5.1: 在 ImageProcessor 中添加 m_gammaLUT 成员（std::array<uint8_t, 256>）
  - [x] SubTask 5.2: 添加 updateGammaLUT(float gamma) 方法
  - [x] SubTask 5.3: 在 applyShader 和 processImageAsyncWithReporter 中使用 LUT 替代 std::pow()
- [x] Task 6: 优化像素处理内循环
  - [x] SubTask 6.1: 预计算 kInv255 = 1.0f/255.0f 常量，乘法替代除法
  - [x] SubTask 6.2: 在无效果参数时跳过处理（全默认值直接拷贝）
- [x] Task 7: 图片并行处理（跳过 - 已在后台线程运行，行级并行收益有限且复杂度高）
- [x] Task 8: 构建验证与回归测试

# Task Dependencies
- [Task 2] depends on [Task 1] (硬件编码器回退需要先有优化的软件编码器基线)
- [Task 4] depends on [Task 5] (缓冲区复用和 LUT 可以一起实现)
- [Task 7] depends on [Task 5, Task 6] (并行处理需要先优化内循环)
- [Task 8] depends on [Task 1-7]
