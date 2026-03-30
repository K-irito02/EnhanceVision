# AI推理进度显示系统优化计划

## 问题分析

### 现象描述
在使用AI推理模式处理视频时，进度条出现以下异常行为：
1. 初始阶段快速来回跳跃（甚至短暂显示100%）
2. 随后迅速从0%推进至99%位置停滞

### 根本原因分析

#### 1. AIEngine 进度报告机制问题

**位置**: `src/core/AIEngine.cpp`

**问题点**:
- `processAsync()` 启动时调用 `setProgress(0.0)`
- `process()` 内部又调用多个 `setProgress()` 值（0.02, 0.05, 0.15...）
- `processTiled()` 进度范围是 0.01~0.95，但初始阶段密集调用
- `processSingle()` 进度范围是 0.02~0.95
- 进度发射有频率限制（66ms间隔），但初始阶段可能短时间内触发多次

**代码证据**:
```cpp
// AIEngine.cpp:478 - processAsync 启动时
setProgress(0.0);

// AIEngine.cpp:973-1001 - processSingle 内部
setProgress(0.02);
setProgress(0.05);
setProgress(0.15);
setProgress(0.85);
setProgress(0.95);

// AIEngine.cpp:1006-1342 - processTiled 内部
setProgress(0.01);
setProgress(0.02);
setProgress(0.03);
setProgress(0.10);
setProgress(0.15);
// ... 分块处理循环中持续更新
```

#### 2. AIVideoProcessor 进度计算问题

**位置**: `src/core/video/AIVideoProcessor.cpp`

**问题点**:
- 进度从 5% 开始计算：`prog = 0.05 + 0.90 * frameCount / totalFrames`
- 结束于 96%，没有平滑过渡到 100%
- `totalFrames` 可能不准确（某些视频格式无法精确获取总帧数）
- 进度更新间隔 100ms，但帧处理速度可能不均匀

**代码证据**:
```cpp
// AIVideoProcessor.cpp:285-288
if (totalFrames > 0) {
    const double prog = 0.05 + 0.90 * static_cast<double>(frameCount) / totalFrames;
    updateProgress(prog);
}
```

#### 3. ProcessingController 进度同步问题

**位置**: `src/controllers/ProcessingController.cpp`

**问题点**:
- `updateTaskProgress()` 有频率限制（100ms间隔，2%步长）
- `syncMessageProgress()` 计算整体进度时，Pending状态任务不计入
- 多文件任务时，进度计算逻辑复杂，可能导致跳跃
- 进度归一化到 0-99 范围，完成时才设为 100

**代码证据**:
```cpp
// ProcessingController.cpp:813
const int normalizedProgress = qBound(0, progress, 99);

// ProcessingController.cpp:1771-1798 - syncMessageProgress
// Pending 状态的任务不计入进度
```

#### 4. MessageItem.qml 剩余时间计算问题

**位置**: `qml/components/MessageItem.qml`

**问题点**:
- 剩余时间基于 `elapsed / progress * (100 - progress)` 计算
- 进度跳跃时，时间估算会剧烈波动
- 进度 <= 1% 时使用固定估计（30秒），不够智能

---

## 优化方案

### 阶段一：AIEngine 进度报告优化

#### 1.1 统一进度报告入口点
- 创建 `ProgressReporter` 内部类，统一管理进度状态
- 实现进度平滑过渡机制，避免跳跃

#### 1.2 优化进度计算逻辑
- 图片处理：基于实际处理阶段（输入准备、推理、输出）计算进度
- 视频处理：基于帧数和单帧处理时间动态计算
- 分块处理：基于已处理分块数和总分块数计算

#### 1.3 添加进度异常检测
- 检测进度跳跃（新进度 < 当前进度 - 阈值）
- 自动校准异常进度值

### 阶段二：AIVideoProcessor 进度优化

#### 2.1 改进帧进度计算
- 实现基于时间的进度平滑算法
- 添加帧处理速度采样，动态调整进度预期

#### 2.2 处理总帧数不确定情况
- 当 `totalFrames <= 0` 时，使用基于时间的进度估算
- 根据已处理帧数和处理速度推算总帧数

#### 2.3 优化进度范围
- 调整进度范围为 0%-100%，移除固定的 5%-96% 限制
- 添加视频编码完成阶段的进度报告

### 阶段三：ProcessingController 进度同步优化

#### 3.1 改进进度更新频率控制
- 使用滑动窗口算法平滑进度更新
- 实现进度变化率限制，防止突变

#### 3.2 优化多任务进度计算
- 改进 `syncMessageProgress()` 算法
- 为每个任务类型设置合理的进度权重

#### 3.3 添加进度一致性检查
- 定期验证进度值合理性
- 自动修正不一致的进度状态

### 阶段四：UI层进度显示优化

#### 4.1 改进剩余时间估算
- 实现基于处理速度的动态估算
- 添加时间估算平滑算法
- 显示置信区间（如"约2-3分钟"）

#### 4.2 添加进度异常处理
- 检测进度跳跃并显示加载状态
- 进度停滞时显示处理中动画

---

## 实施步骤

### Step 1: 创建进度报告基础设施
**文件**: `include/EnhanceVision/core/ProgressReporter.h`, `src/core/ProgressReporter.cpp`

1. 创建 `ProgressReporter` 类
2. 实现进度平滑算法
3. 实现异常检测机制

### Step 2: 重构 AIEngine 进度报告
**文件**: `src/core/AIEngine.cpp`

1. 集成 `ProgressReporter`
2. 统一进度计算逻辑
3. 添加阶段标识

### Step 3: 优化 AIVideoProcessor 进度
**文件**: `src/core/video/AIVideoProcessor.cpp`

1. 改进帧进度计算
2. 添加时间估算
3. 处理边界情况

### Step 4: 优化 ProcessingController 同步
**文件**: `src/controllers/ProcessingController.cpp`

1. 改进进度同步算法
2. 添加一致性检查
3. 优化多任务进度计算

### Step 5: 优化 UI 显示
**文件**: `qml/components/MessageItem.qml`

1. 改进剩余时间计算
2. 添加进度异常处理
3. 优化动画效果

### Step 6: 测试验证
1. 单元测试：进度计算逻辑
2. 集成测试：图片/视频处理进度
3. 边界测试：不同分辨率、时长文件

---

## 预期效果

1. **进度条显示准确性**: 进度条平滑增长，无跳跃或停滞
2. **剩余时间精确性**: 时间估算误差 < 20%，随处理进行逐渐精确
3. **数据一致性**: UI显示与后端实际进度实时同步
4. **异常处理**: 自动检测并修正进度异常
5. **兼容性**: 支持不同分辨率、时长的图片和视频文件

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 进度平滑导致延迟 | 低 | 使用预测算法提前显示 |
| 多线程竞态条件 | 中 | 使用原子操作和互斥锁 |
| 视频格式兼容性 | 中 | 添加格式检测和回退策略 |
| 性能影响 | 低 | 进度计算使用轻量级算法 |

---

## 文件修改清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| `include/EnhanceVision/core/ProgressReporter.h` | 新增 | 进度报告器头文件 |
| `src/core/ProgressReporter.cpp` | 新增 | 进度报告器实现 |
| `include/EnhanceVision/core/AIEngine.h` | 修改 | 集成 ProgressReporter |
| `src/core/AIEngine.cpp` | 修改 | 重构进度报告逻辑 |
| `src/core/video/AIVideoProcessor.cpp` | 修改 | 优化视频进度计算 |
| `src/controllers/ProcessingController.cpp` | 修改 | 优化进度同步 |
| `qml/components/MessageItem.qml` | 修改 | 优化UI显示 |
| `CMakeLists.txt` | 修改 | 添加新源文件 |
