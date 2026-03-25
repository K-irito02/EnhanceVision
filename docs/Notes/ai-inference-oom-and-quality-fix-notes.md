# AI推理OOM和图像质量修复笔记

## 概述

**创建日期**: 2026-03-25  
**相关 Issue**: Real-ESRGAN模型OOM和图像质量问题

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `src/core/AIEngine.cpp` | 修复GPU OOM重试机制、动态padding、进度条倒退、模型切换崩溃 |
| `qml/components/MessageItem.qml` | 修复进度条剩余时间计算逻辑 |

---

## 二、修复的问题

### 1. GPU OOM和崩溃问题
- **问题**: Real-ESRGAN大模型推理时GPU显存不足，导致进程崩溃
- **修复**: 
  - 改进OOM重试机制，使用更小分块（128/96/64）
  - 添加GPU内存清理函数，OOM后重置状态
  - 加载新模型前清理GPU内存，防止状态损坏

### 2. 图像质量问题
- **问题**: 分块处理边缘区域出现"花屏"和失真，颜色范围异常（min: 92 max: 180）
- **修复**:
  - 动态计算padding：超大模型(999层)→64px，大模型(268层)→48px
  - 使用镜像填充扩展原图边界，确保所有分块有完整padding上下文
  - 修改分块提取逻辑，统一处理边界分块

### 3. 进度条倒退问题
- **问题**: OOM重试时进度条从0.1重新开始，出现倒退
- **修复**: setProgress()只允许进度前进（除非重置或完成）

---

## 三、技术实现细节

### 关键代码片段

#### 动态Padding计算
```cpp
// 根据模型复杂度动态计算padding
int padding = model.tilePadding;
const int layerCount = model.isLoaded ? static_cast<int>(m_net.layers().size()) : 0;
if (layerCount > 500) {
    padding = std::max(padding, 64);  // 超大模型（如 realesrgan_x4plus 999层）
} else if (layerCount > 200) {
    padding = std::max(padding, 48);  // 大模型（如 realesrgan_x4plus_anime 268层）
} else if (layerCount > 50) {
    padding = std::max(padding, 24);  // 中等模型
}
```

#### 镜像填充边界
```cpp
// 使用镜像填充扩展原图，确保所有分块有完整padding
QImage paddedInput(w + 2 * padding, h + 2 * padding, input.format());
// ... 镜像填充四个边缘和角落
```

#### 进度条只允许前进
```cpp
void AIEngine::setProgress(double value, bool forceEmit)
{
    // 防止进度条倒退
    const bool isReset = (clampedValue < 0.01);
    const bool isComplete = (clampedValue >= 0.99);
    const bool isForward = (clampedValue > previous);
    
    if (!forceEmit && !isReset && !isComplete && !isForward) {
        return; // 忽略倒退的进度更新
    }
}
```

### 设计决策
- 使用镜像填充而非简单的边界复制，保持更好的连续性
- 根据模型层数动态调整padding，平衡内存使用和质量
- OOM重试时保存当前进度，避免UI体验问题

---

## 四、遇到的问题及解决方案

### 问题1: 边缘分块颜色范围异常
**现象**: `after clamp min: 92.5122 max: 180.417`
**原因**: 边缘分块padding不足，模型感受野超出有效区域
**解决**: 镜像填充提供完整上下文，所有分块padding: 64px

### 问题2: 模型切换时崩溃
**现象**: 加载第二个Real-ESRGAN模型时进程崩溃
**原因**: GPU内存状态损坏，allocator未清理
**解决**: 切换模型前清理所有Vulkan allocator

### 问题3: 进度条倒退
**现象**: OOM重试时进度从19%回到10%
**原因**: 重试时进度重新计算
**解决**: setProgress只允许前进，保存重试前进度

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| realesrgan_x4plus (999层) | 9分块成功，无OOM | ✅ 通过 |
| realesrgan_x4plus_anime (268层) | 处理成功，OOM重试 | ✅ 通过 |
| 边界图像质量 | min: 0 max: 255 | ✅ 通过 |
| 进度条 | 只前进不倒退 | ✅ 通过 |
| 多模型连续处理 | 无崩溃 | ✅ 通过 |

---

## 六、性能影响

- **内存使用**: 镜像填充增加约 (2*padding)² 像素，对于64px padding增加约 8MB (940x940图像)
- **处理时间**: 动态padding可能增加分块数量，但避免了OOM重试，整体更稳定
- **GPU稳定性**: 显著改善，无Device Loss错误

---

## 七、后续工作

- [ ] 考虑自适应padding，根据实际感受野计算
- [ ] 添加GPU内存使用监控
- [ ] 优化镜像填充性能（小图像时可能不需要）
