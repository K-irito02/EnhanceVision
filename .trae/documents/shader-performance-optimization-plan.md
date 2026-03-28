# Shader模式图片处理性能优化计划（完整实施版）

## 问题诊断总结

### 1. GLSL Shader性能瓶颈
- **过多纹理采样**: 每像素执行约34次纹理采样
- **无条件执行**: 纹理采样在效果条件判断之前执行
- **低效排序算法**: 降噪使用冒泡排序 O(n²)

### 2. QML层面性能问题
- **实时参数绑定无防抖**: 滑块拖动时频繁触发重渲染
- **视频Shader额外开销**: ShaderEffectSource每帧都有额外开销
- **layer.enabled过度使用**: 每个layer都会创建额外的FBO

### 3. 架构设计问题
- 单Pass渲染所有效果
- 缺少脏区域检测和效果缓存机制

---

## 阶段一：GLSL Shader优化

### 1.1 条件化纹理采样
**文件**: `resources/shaders/fullshader.frag`
**改动内容**:
- 将邻居采样移入条件块内
- 添加 `needNeighbors` 判断变量
- 仅在需要降噪或锐化时执行邻居采样

### 1.2 优化降噪算法
**文件**: `resources/shaders/fullshader.frag`
**改动内容**:
- 使用快速中值滤波替代冒泡排序
- 减少比较次数

### 1.3 优化Blur效果
**文件**: `resources/shaders/fullshader.frag`
**改动内容**:
- 减少Blur采样半径
- 使用预计算权重

### 1.4 预计算优化
**文件**: `resources/shaders/fullshader.frag`
**改动内容**:
- 将常量计算移到条件块外
- 减少重复计算

---

## 阶段二：QML层面优化

### 2.1 参数变化防抖
**文件**: `qml/components/FullShaderEffect.qml`
**改动内容**:
- 添加内部参数缓存
- 添加防抖Timer（50ms）
- 延迟更新实际Shader参数

### 2.2 视频Shader优化
**文件**: `qml/components/EmbeddedMediaViewer.qml`
**改动内容**:
- 添加分辨率限制参数
- 优化ShaderEffectSource配置
- 添加textureSize限制

### 2.3 减少layer.effect使用
**文件**: 多个QML文件
**改动内容**:
- 审查每个layer.enabled必要性
- 移除不必要的layer效果
- 合并相邻的layer效果

---

## 阶段三：架构重构

### 3.1 多Pass渲染管线
**新文件**: `qml/components/MultiPassShaderEffect.qml`
**改动内容**:
- 创建多Pass渲染组件
- Pass 1: 基础颜色调整
- Pass 2: 色彩调整
- Pass 3: 空间滤波（按需）
- Pass 4: 效果叠加

### 3.2 效果缓存机制
**文件**: `qml/components/FullShaderEffect.qml`
**改动内容**:
- 添加参数变化检测
- 仅在参数变化时触发重渲染
- 添加缓存标志

### 3.3 渐进式渲染
**文件**: `qml/components/FullShaderEffect.qml`
**改动内容**:
- 大图先渲染低分辨率预览
- 用户停止操作后渲染完整分辨率

---

## 阶段四：高级优化

### 4.1 创建可分离模糊Shader
**新文件**: `resources/shaders/blur_horizontal.frag`
**新文件**: `resources/shaders/blur_vertical.frag`
**改动内容**:
- 水平模糊Pass
- 垂直模糊Pass
- 减少采样次数从25次到10次

### 4.2 帧缓冲对象优化
**文件**: `qml/components/MultiPassShaderEffect.qml`
**改动内容**:
- 复用FBO避免频繁分配
- 添加FBO池管理

### 4.3 异步渲染支持
**文件**: `qml/components/FullShaderEffect.qml`
**改动内容**:
- 使用Qt.callLater延迟渲染
- 添加渲染队列管理

---

## 实施文件清单

### 需要修改的文件
1. `resources/shaders/fullshader.frag` - Shader优化
2. `qml/components/FullShaderEffect.qml` - 防抖和缓存
3. `qml/components/EmbeddedMediaViewer.qml` - 视频优化
4. `qml/components/MediaViewerWindow.qml` - 视频优化
5. `qml/components/FileList.qml` - layer优化
6. `qml/components/MediaThumbnailStrip.qml` - layer优化
7. `qml/controls/ColoredIcon.qml` - layer优化
8. `qml/controls/ComboBox.qml` - layer优化

### 需要创建的文件
1. `resources/shaders/blur_horizontal.frag` - 水平模糊
2. `resources/shaders/blur_vertical.frag` - 垂直模糊
3. `qml/components/MultiPassShaderEffect.qml` - 多Pass渲染

---

## 预期效果

| 指标 | 优化前 | 优化后（预期） |
|------|--------|----------------|
| 纹理采样次数 | ~34次/像素 | ~10次/像素（平均） |
| 帧率（1080p） | 15-20 FPS | 50-60 FPS |
| 内存占用 | 较高 | 降低30% |
| 响应延迟 | 明显卡顿 | 流畅 |

---

## 详细实施步骤

### Step 1: 优化fullshader.frag
1. 添加needNeighbors条件判断
2. 将邻居采样移入条件块
3. 优化降噪排序算法
4. 减少Blur采样半径

### Step 2: 添加参数防抖
1. 在FullShaderEffect.qml中添加防抖Timer
2. 添加参数缓存变量
3. 实现延迟更新机制

### Step 3: 优化视频Shader
1. 在EmbeddedMediaViewer.qml中添加分辨率限制
2. 优化ShaderEffectSource配置
3. 添加textureSize限制

### Step 4: 减少layer.effect
1. 审查FileList.qml中的layer使用
2. 审查MediaThumbnailStrip.qml中的layer使用
3. 审查ColoredIcon.qml中的layer使用
4. 审查ComboBox.qml中的layer使用

### Step 5: 创建多Pass渲染
1. 创建MultiPassShaderEffect.qml
2. 实现Pass分离逻辑
3. 添加按需渲染支持

### Step 6: 创建可分离模糊Shader
1. 创建blur_horizontal.frag
2. 创建blur_vertical.frag
3. 集成到多Pass渲染管线

### Step 7: 添加效果缓存
1. 在FullShaderEffect.qml中添加缓存机制
2. 实现参数变化检测
3. 添加脏区域标记

### Step 8: 添加渐进式渲染
1. 实现低分辨率预览
2. 添加完整分辨率延迟渲染
3. 集成到用户交互流程
