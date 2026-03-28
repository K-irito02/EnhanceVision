# AI 推理进度显示优化笔记

## 概述

优化 AI 推理模式进度显示系统，解决进度条跳跃、停滞和时间估算不准确的问题。

**创建日期**: 2026-03-29
**作者**: AI Assistant

---

## 一、变更概述

### 1. 新增文件

| 文件路径 | 功能描述 |
|----------|----------|
| `include/EnhanceVision/core/ProgressReporter.h` | 进度报告器头文件 |
| `src/core/ProgressReporter.cpp` | 进度报告器实现 |

### 2. 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `CMakeLists.txt` | 添加 ProgressReporter 源文件 |
| `include/EnhanceVision/core/AIEngine.h` | 集成 ProgressReporter |
| `src/core/AIEngine.cpp` | 重构进度报告逻辑 |
| `include/EnhanceVision/core/video/AIVideoProcessor.h` | 集成 ProgressReporter |
| `src/core/video/AIVideoProcessor.cpp` | 优化视频进度计算 |
| `src/controllers/ProcessingController.cpp` | 移除进度 99 上限限制 |
| `qml/components/MessageItem.qml` | 优化剩余时间估算和平滑显示 |

---

## 二、实现的功能特性

- ✅ 统一进度状态管理：通过 ProgressReporter 类统一管理进度状态
- ✅ 进度平滑过渡算法：限制最大跳跃幅度为 10%，避免进度条突变
- ✅ 异常检测与修正：检测进度倒退超过 20% 的异常情况并忽略
- ✅ 剩余时间动态估算：基于历史进度数据动态计算剩余时间
- ✅ 处理阶段标识：支持设置当前处理阶段描述
- ✅ UI 层时间估算平滑：使用滑动平均算法平滑剩余时间显示

---

## 三、技术实现细节

### ProgressReporter 核心设计

```cpp
class ProgressReporter : public QObject {
    // 进度值 (0.0 - 1.0)
    double progress() const;
    
    // 当前处理阶段
    QString stage() const;
    
    // 预计剩余时间（秒）
    int estimatedRemainingSec() const;
    
    // 设置进度（带平滑处理）
    void setProgress(double value, const QString& stage = QString());
    
    // 重置进度
    void reset();
    
    // 强制更新（跳过平滑处理）
    void forceUpdate(double value, const QString& stage = QString());

private:
    // 平滑参数
    static constexpr double kMaxJumpDelta = 0.10;  // 最大跳跃幅度 10%
    static constexpr qint64 kMinEmitIntervalMs = 66;  // 最小发射间隔
    static constexpr double kAnomalyThreshold = 0.20;  // 异常跳跃阈值 20%
};
```

### 进度计算优化

1. **AIEngine**: 使用 ProgressReporter 管理进度，统一进度发射逻辑
2. **AIVideoProcessor**: 添加 `estimateProgressByTime()` 方法处理总帧数不确定情况
3. **ProcessingController**: 移除 99 上限限制，允许进度自然增长到 100
4. **MessageItem.qml**: 添加滑动平均平滑时间估算，检测进度跳跃并重置

### 数据流

```
AIEngine/AIVideoProcessor 
    → ProgressReporter.setProgress() 
    → 平滑处理 + 异常检测 
    → emit progressChanged 
    → ProcessingController 
    → MessageItem.qml 
    → UI 显示
```

---

## 四、解决的问题

### 问题 1：进度条跳跃

**现象**：进度条在初始阶段快速来回跳跃，甚至短暂显示 100%

**原因**：多处代码同时设置进度值，缺乏统一管理和协调

**解决方案**：
- 创建统一的 ProgressReporter 类管理进度
- 添加进度平滑算法，限制最大跳跃幅度
- 添加异常检测，忽略异常的进度倒退

### 问题 2：进度停滞在 99%

**现象**：进度推进到 99% 后停滞，完成后才跳到 100%

**原因**：ProcessingController 中将进度归一化到 0-99 范围

**解决方案**：移除 99 上限限制，允许进度自然增长到 100

### 问题 3：剩余时间估算不准确

**现象**：剩余时间估算剧烈波动，不准确

**原因**：进度跳跃导致时间计算不稳定

**解决方案**：
- 使用滑动平均算法平滑时间估算
- 检测进度跳跃时重置时间采样
- 基于历史进度数据动态计算

---

## 五、测试验证

### 测试场景

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 图片 AI 推理 | 进度平滑增长 | ✅ 通过 |
| 视频 AI 推理 | 进度平滑增长，时间估算合理 | ✅ 通过 |
| 多文件处理 | 整体进度准确 | ✅ 通过 |
| 进度完成 | 显示 100% | ✅ 通过 |

---

## 六、性能影响

| 指标 | 变更前 | 变更后 | 影响 |
|------|--------|--------|------|
| 进度更新频率 | 66ms | 66ms | 无变化 |
| 内存占用 | 基准 | +少量历史数据 | 可忽略 |
| CPU 开销 | 基准 | +平滑计算 | 可忽略 |

---

## 七、后续工作

- [ ] 可考虑添加更多处理阶段描述
- [ ] 可考虑添加进度预测算法

---

## 八、文件修改清单

| 文件 | 修改类型 |
|------|----------|
| `include/EnhanceVision/core/ProgressReporter.h` | 新增 |
| `src/core/ProgressReporter.cpp` | 新增 |
| `include/EnhanceVision/core/AIEngine.h` | 修改 |
| `src/core/AIEngine.cpp` | 修改 |
| `include/EnhanceVision/core/video/AIVideoProcessor.h` | 修改 |
| `src/core/video/AIVideoProcessor.cpp` | 修改 |
| `src/controllers/ProcessingController.cpp` | 修改 |
| `qml/components/MessageItem.qml` | 修改 |
| `CMakeLists.txt` | 修改 |
