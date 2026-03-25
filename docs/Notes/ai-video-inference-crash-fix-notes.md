# AI 视频推理闪退修复笔记

## 概述

**创建日期**: 2026-03-26  
**问题**: AI 推理模式处理视频时崩溃（0xc0000374，堆损坏）

---

## 变更文件


| 文件路径                    | 修改内容                                                                       |
| ----------------------- | -------------------------------------------------------------------------- |
| `src/core/AIEngine.cpp` | 修复 `qimageToMat()` 中 RGB888 行对齐处理，改为使用带 stride 的 `from_pixels`，避免越界读取导致堆损坏 |
| `src/core/AIEngine.cpp` | 修复视频管线像素格式不匹配：解码输出改为 `RGB24 -> QImage::Format_RGB888`，编码输入同步使用 `RGB24` |
| `src/app/Application.cpp` / `include/EnhanceVision/app/Application.h` | 新增生命周期守卫（window handle 丢失 / 窗口隐藏 / 主窗口销毁）后强制关闭应用进程 |
| `src/controllers/ProcessingController.cpp` | 重构任务取消链路：`cancelAllTasks/cancelTask` 统一走 `gracefulCancel + cleanupTask`，确保 AI 任务可终止 |


---

## 根因

`QImage::Format_RGB888` 的 `bytesPerLine` 可能大于 `width * 3`（Qt 行对齐）。
原实现调用 `ncnn::Mat::from_pixels(..., w, h)` 未传 stride，ncnn 按紧密行读取，视频逐帧/分块场景下会发生读错位，最终触发堆损坏闪退。

另外，视频解码阶段此前使用 `sws_scale` 输出 `AV_PIX_FMT_BGRA`，但目标 `QImage` 与后续编码链路混用了 `ARGB32/RGB32/RGB888`，导致帧内存布局在高频转换中不稳定，增加了访问违规概率。

---

## 修复要点

- `qimageToMat()` 先确保输入为有效 `RGB888` 图像。
- 读取 `stride = img.bytesPerLine()`。
- 改用 `ncnn::Mat::from_pixels(img.constBits(), ncnn::Mat::PIXEL_RGB, w, h, stride)`。
- 增加转换失败与空图保护日志。
- 视频解码 `sws_getContext` 统一输出 `AV_PIX_FMT_RGB24`，并使用 `QImage::Format_RGB888` 承接，避免 `BGRA/ARGB32` 混用。
- 视频编码前不再额外转 `RGB32`，直接以 `RGB888` 作为编码输入，`encSwsCtx` 同步改为 `AV_PIX_FMT_RGB24 -> YUV420P`。
- `Application` 增加生命周期守卫与看门狗：
  - 主窗口 `destroyed`、`windowHandleChanged(nullptr)`、`winId()==0`、异常隐藏时触发 `scheduleHardExit`。
  - 统一关闭序列：`ProcessingController::cancelAllTasks()` → `QApplication::quit()`。
  - 若 3.5 秒内未退出，执行 `std::_Exit(EXIT_FAILURE)` 强制结束进程，避免“句柄为 0 但后台仍运行”。
- `ProcessingController` 取消逻辑重构：
  - 析构时主动 `cancelAllTasks()` 与 `AIEngine::cancelProcess()`。
  - `cancelTask/cancelAllTasks` 不再只改状态，统一走 `gracefulCancel` 和 `cleanupTask`，确保 AI 推理线程收到取消信号。

---

## 验证

- 关键崩溃路径（视频帧转 ncnn 输入）已修复为 stride 安全读取路径。
- 生命周期与任务取消链路已补齐：当主窗口句柄丢失、窗口销毁或异常隐藏时，会触发统一退出流程并在超时后强制终止进程。
- Windows 崩溃事件中历史异常以 `0xc0000005 / 0xc0000374` 为主，本次修复针对视频帧像素链路的高风险段做了统一格式收敛。
- 目前本机终端环境缺少 `cmake` 命令，无法在当前会话完成编译回归；需在你的开发机执行一次完整构建与运行验证。

