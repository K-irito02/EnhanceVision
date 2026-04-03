# AI 推理模式重构计划

## 一、现状分析

### 1.1 当前代码结构

| 文件 | 类名 | 当前行数 | 职责 |
|------|------|----------|------|
| `AIInferenceService.h/cpp` | AIInferenceService | ~600 | 服务入口、任务调度、状态管理、资源生命周期 |
| `AIInferenceWorker.h/cpp` | AIInferenceWorker | ~560 | 任务执行、图像处理、视频处理、模型加载 |
| `AIInferenceWorker.h` | AIInferenceThread | ~50 | 线程管理 |
| `AITaskQueue.h/cpp` | AITaskQueue | ~400 | 任务队列管理 |
| `AIEngine.h/cpp` | AIEngine | ~1470 | 推理引擎、模型管理、预处理、后处理、参数计算 |
| `AIVideoProcessor.h/cpp` | AIVideoProcessor | ~200 | 视频处理 |
| `AIInferenceTypes.h` | - | ~230 | 类型定义 |

### 1.2 存在的问题

#### 问题1：职责不单一
- **AIInferenceService**：混合了服务管理、任务调度、状态同步、资源管理
- **AIInferenceWorker**：混合了任务执行、图像处理、视频处理、模型加载
- **AIEngine**：混合了推理核心、预处理、后处理、参数计算、进度管理

#### 问题2：扩展性差
- 硬编码 NCNN CPU 推理，无法切换到 GPU（Vulkan/OpenCL/DirectML）
- 无法支持其他推理框架（ONNX Runtime、TensorRT、CoreML）
- 预处理/后处理逻辑耦合在推理引擎中

#### 问题3：可维护性低
- AIEngine.cpp 超过 1400 行，难以维护
- 多种处理模式（单图、分块、TTA）混在一起
- 自动参数计算逻辑复杂且分散

#### 问题4：健壮性不足
- 缺少统一的错误处理机制
- 资源清理逻辑分散
- 取消机制不够优雅

---

## 二、重构目标

### 2.1 设计原则
1. **单一职责**：每个类只负责一件事
2. **开闭原则**：对扩展开放，对修改关闭
3. **依赖倒置**：依赖抽象而非具体实现
4. **接口隔离**：接口要小而专一

### 2.2 目标架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application)                       │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    AIInferenceService                        │ │
│  │  (服务门面 - 统一入口、状态查询、信号转发)                      │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        调度层 (Scheduling)                        │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │   AITaskQueue    │  │ AIInferenceWorker│  │ AIWorkerThread │ │
│  │   (任务队列)      │  │   (任务执行器)    │  │  (线程管理)    │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        处理层 (Processing)                        │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │ AIImageProcessor │  │AIVideoProcessor  │  │ AIModelManager │ │
│  │   (图像处理)      │  │   (视频处理)      │  │  (模型管理)    │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        推理层 (Inference)                         │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                   IInferenceBackend (接口)                   │ │
│  └─────────────────────────────────────────────────────────────┘ │
│          │                │                │                     │
│          ▼                ▼                ▼                     │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐             │
│  │NCNNCPUBackend│ │NCNNVulkanBkd │ │ ONNXBackend  │             │
│  │  (CPU推理)    │ │ (GPU推理)    │ │ (ONNX推理)   │             │
│  └──────────────┘ └──────────────┘ └──────────────┘             │
└─────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│                        基础层 (Foundation)                        │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐             │
│  │ Preprocessor │ │ Postprocessor│ │ParamCalculator│             │
│  │  (预处理)     │ │  (后处理)    │ │ (参数计算)    │             │
│  └──────────────┘ └──────────────┘ └──────────────┘             │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐             │
│  │ ImageUtils   │ │ProgressRpt   │ │ ErrorHandling│             │
│  │  (图像工具)   │ │  (进度报告)   │ │  (错误处理)   │             │
│  └──────────────┘ └──────────────┘ └──────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 三、详细设计

### 3.1 推理层重构

#### 3.1.1 抽象推理后端接口

**文件**：`include/EnhanceVision/core/inference/IInferenceBackend.h`

```cpp
/**
 * @brief 推理后端接口
 * 
 * 支持多种推理框架和硬件加速的统一抽象。
 */
class IInferenceBackend : public QObject {
    Q_OBJECT
public:
    virtual ~IInferenceBackend() = default;
    
    // 生命周期
    virtual bool initialize(const BackendConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;
    
    // 模型管理
    virtual bool loadModel(const ModelInfo& model) = 0;
    virtual void unloadModel() = 0;
    virtual bool isModelLoaded() const = 0;
    virtual QString currentModelId() const = 0;
    
    // 推理执行
    virtual InferenceResult inference(const InferenceRequest& request) = 0;
    virtual void cancel() = 0;
    
    // 能力查询
    virtual BackendType type() const = 0;
    virtual BackendCapabilities capabilities() const = 0;
    virtual QString name() const = 0;
    
signals:
    void progressChanged(double progress);
    void errorOccurred(const QString& error);
};
```

#### 3.1.2 NCNN CPU 后端实现

**文件**：`include/EnhanceVision/core/inference/NCNNCPUBackend.h`
**文件**：`src/core/inference/NCNNCPUBackend.cpp`

从 AIEngine 中提取纯 CPU 推理逻辑，职责：
- NCNN 网络初始化和配置
- CPU 线程数设置
- 单图推理执行
- 内存管理

#### 3.1.3 NCNN Vulkan 后端实现（预留接口）

**文件**：`include/EnhanceVision/core/inference/NCNNVulkanBackend.h`

为未来 GPU 加速预留接口：
- Vulkan 设备检测和选择
- GPU 内存管理
- GPU 推理执行

#### 3.1.4 后端工厂

**文件**：`include/EnhanceVision/core/inference/InferenceBackendFactory.h`

```cpp
class InferenceBackendFactory {
public:
    static std::unique_ptr<IInferenceBackend> create(BackendType type);
    static QList<BackendType> availableBackends();
    static BackendType recommendedBackend();
};
```

### 3.2 基础层重构

#### 3.2.1 预处理器

**文件**：`include/EnhanceVision/core/inference/ImagePreprocessor.h`

从 AIEngine 中提取预处理逻辑：
- `qimageToMat()` - 图像格式转换
- `normalize()` - 归一化处理
- `tileImage()` - 图像分块
- `padImage()` - 图像填充

#### 3.2.2 后处理器

**文件**：`include/EnhanceVision/core/inference/ImagePostprocessor.h`

从 AIEngine 中提取后处理逻辑：
- `matToQimage()` - 输出转换
- `denormalize()` - 反归一化
- `mergeTiles()` - 分块合并
- `applyOutscale()` - 缩放输出

#### 3.2.3 参数计算器

**文件**：`include/EnhanceVision/core/inference/AutoParamCalculator.h`

从 AIEngine 中提取自动参数计算逻辑：
- `computeTileSize()` - 自动分块大小
- `computeModelParams()` - 模型参数计算
- 按模型类别的参数策略

#### 3.2.4 错误处理器

**文件**：`include/EnhanceVision/core/inference/InferenceErrorHandler.h`

统一错误处理：
- 错误分类和转换
- 错误恢复策略
- 错误日志记录

### 3.3 处理层重构

#### 3.3.1 图像处理器

**文件**：`include/EnhanceVision/core/processing/AIImageProcessor.h`

从 AIInferenceWorker 中提取图像处理逻辑：
- 单图处理流程
- 分块处理流程
- TTA 处理流程
- 进度报告

#### 3.3.2 视频处理器（优化）

**文件**：`include/EnhanceVision/core/processing/AIVideoProcessor.h`

优化现有视频处理器：
- 解耦与 AIEngine 的直接依赖
- 使用 IInferenceBackend 接口
- 改进帧处理流程

#### 3.3.3 模型管理器

**文件**：`include/EnhanceVision/core/processing/AIModelManager.h`

统一模型管理：
- 模型加载/卸载
- 模型缓存
- 模型预加载

### 3.4 调度层重构

#### 3.4.1 任务队列（保持）

**文件**：`include/EnhanceVision/core/scheduling/AITaskQueue.h`

当前设计良好，保持不变，仅移动到 scheduling 目录。

#### 3.4.2 任务执行器（重构）

**文件**：`include/EnhanceVision/core/scheduling/AITaskExecutor.h`

从 AIInferenceWorker 重构：
- 专注于任务执行调度
- 调用处理层完成实际工作
- 管理任务生命周期

#### 3.4.3 工作线程（简化）

**文件**：`include/EnhanceVision/core/scheduling/AIWorkerThread.h`

简化线程管理：
- 纯粹的线程生命周期管理
- 不包含业务逻辑

### 3.5 服务层重构

#### 3.5.1 服务门面（精简）

**文件**：`include/EnhanceVision/core/service/AIInferenceService.h`

精简为服务门面：
- 统一入口点
- 状态查询
- 信号转发
- 配置管理

---

## 四、文件目录结构

```
include/EnhanceVision/core/
├── inference/                    # 推理层
│   ├── IInferenceBackend.h       # 推理后端接口
│   ├── InferenceTypes.h          # 推理类型定义
│   ├── InferenceBackendFactory.h # 后端工厂
│   ├── NCNNCPUBackend.h          # NCNN CPU 后端
│   ├── NCNNVulkanBackend.h       # NCNN Vulkan 后端（预留）
│   ├── ImagePreprocessor.h       # 图像预处理器
│   ├── ImagePostprocessor.h      # 图像后处理器
│   ├── AutoParamCalculator.h     # 自动参数计算器
│   └── InferenceErrorHandler.h   # 错误处理器
├── processing/                   # 处理层
│   ├── AIImageProcessor.h        # 图像处理器
│   ├── AIVideoProcessor.h        # 视频处理器
│   └── AIModelManager.h          # 模型管理器
├── scheduling/                   # 调度层
│   ├── AITaskQueue.h             # 任务队列
│   ├── AITaskExecutor.h          # 任务执行器
│   ├── AIWorkerThread.h          # 工作线程
│   └── AITaskTypes.h             # 任务类型定义
├── service/                      # 服务层
│   ├── AIInferenceService.h      # 服务门面
│   └── AIServiceConfig.h         # 服务配置
└── types/                        # 公共类型
    └── AICommonTypes.h           # 公共类型定义

src/core/
├── inference/                    # 推理层实现
│   ├── NCNNCPUBackend.cpp
│   ├── NCNNVulkanBackend.cpp
│   ├── InferenceBackendFactory.cpp
│   ├── ImagePreprocessor.cpp
│   ├── ImagePostprocessor.cpp
│   ├── AutoParamCalculator.cpp
│   └── InferenceErrorHandler.cpp
├── processing/                   # 处理层实现
│   ├── AIImageProcessor.cpp
│   ├── AIVideoProcessor.cpp
│   └── AIModelManager.cpp
├── scheduling/                   # 调度层实现
│   ├── AITaskQueue.cpp
│   ├── AITaskExecutor.cpp
│   └── AIWorkerThread.cpp
└── service/                      # 服务层实现
    └── AIInferenceService.cpp
```

---

## 五、实施步骤

### 阶段一：基础层建设（预计 2-3 天）

| 步骤 | 任务 | 输入 | 输出 | 依赖 |
|------|------|------|------|------|
| 1.1 | 创建推理类型定义 | AIInferenceTypes.h | inference/InferenceTypes.h | 无 |
| 1.2 | 创建推理后端接口 | 设计文档 | IInferenceBackend.h | 1.1 |
| 1.3 | 提取图像预处理器 | AIEngine::qimageToMat等 | ImagePreprocessor.h/cpp | 1.1 |
| 1.4 | 提取图像后处理器 | AIEngine::matToQimage等 | ImagePostprocessor.h/cpp | 1.1 |
| 1.5 | 提取参数计算器 | AIEngine::computeAuto* | AutoParamCalculator.h/cpp | 1.1 |
| 1.6 | 创建错误处理器 | 设计文档 | InferenceErrorHandler.h/cpp | 1.1 |

### 阶段二：推理层实现（预计 2-3 天）

| 步骤 | 任务 | 输入 | 输出 | 依赖 |
|------|------|------|------|------|
| 2.1 | 实现 NCNN CPU 后端 | AIEngine 核心逻辑 | NCNNCPUBackend.h/cpp | 阶段一 |
| 2.2 | 实现后端工厂 | 设计文档 | InferenceBackendFactory.h/cpp | 2.1 |
| 2.3 | 创建 Vulkan 后端骨架 | 设计文档 | NCNNVulkanBackend.h（空实现） | 2.1 |
| 2.4 | 单元测试 | 后端实现 | 测试用例 | 2.2 |

### 阶段三：处理层重构（预计 2 天）

| 步骤 | 任务 | 输入 | 输出 | 依赖 |
|------|------|------|------|------|
| 3.1 | 创建图像处理器 | AIInferenceWorker::processImage | AIImageProcessor.h/cpp | 阶段二 |
| 3.2 | 重构视频处理器 | AIVideoProcessor | AIVideoProcessor（优化） | 阶段二 |
| 3.3 | 创建模型管理器 | AIEngine 模型管理部分 | AIModelManager.h/cpp | 阶段二 |

### 阶段四：调度层重构（预计 1-2 天）

| 步骤 | 任务 | 输入 | 输出 | 依赖 |
|------|------|------|------|------|
| 4.1 | 移动任务队列 | AITaskQueue | scheduling/AITaskQueue | 无 |
| 4.2 | 创建任务执行器 | AIInferenceWorker | AITaskExecutor.h/cpp | 阶段三 |
| 4.3 | 简化工作线程 | AIInferenceThread | AIWorkerThread.h/cpp | 4.2 |

### 阶段五：服务层精简（预计 1 天）

| 步骤 | 任务 | 输入 | 输出 | 依赖 |
|------|------|------|------|------|
| 5.1 | 重构服务门面 | AIInferenceService | service/AIInferenceService | 阶段四 |
| 5.2 | 更新服务配置 | AIServiceConfig | AIServiceConfig.h | 5.1 |

### 阶段六：集成测试（预计 1 天）

| 步骤 | 任务 | 输入 | 输出 | 依赖 |
|------|------|------|------|------|
| 6.1 | 更新 CMakeLists | 新文件结构 | 构建配置 | 阶段五 |
| 6.2 | 集成测试 | 完整系统 | 测试报告 | 6.1 |
| 6.3 | 性能对比 | 新旧实现 | 性能报告 | 6.2 |
| 6.4 | 文档更新 | 新架构 | API 文档 | 6.2 |

---

## 六、向后兼容

### 6.1 保留的公共 API

以下 API 保持不变，确保现有代码无需修改：

```cpp
// AIInferenceService 主要接口
AIInferenceService::instance()
AIInferenceService::initialize()
AIInferenceService::submitTask()
AIInferenceService::cancelTask()
AIInferenceService::isProcessing()
AIInferenceService::queueSize()
// ... 其他公共方法
```

### 6.2 废弃的 API

以下内部 API 将被废弃：

```cpp
// AIEngine 将被重构，直接使用将被废弃
// 建议使用 IInferenceBackend 接口
```

### 6.3 迁移指南

提供详细的迁移指南文档，帮助开发者适应新架构。

---

## 七、风险评估

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| 重构引入新 Bug | 中 | 高 | 分阶段实施，每阶段充分测试 |
| 性能下降 | 低 | 中 | 性能对比测试，优化关键路径 |
| API 不兼容 | 低 | 高 | 保持公共 API 稳定，提供迁移指南 |
| 开发周期延长 | 中 | 中 | 预留缓冲时间，优先核心功能 |

---

## 八、验收标准

### 8.1 功能验收
- [ ] 所有现有功能正常工作
- [ ] 图像处理（单图、分块、TTA）正常
- [ ] 视频处理正常
- [ ] 任务队列管理正常
- [ ] 取消机制正常

### 8.2 质量验收
- [ ] 单元测试覆盖率 > 80%
- [ ] 无内存泄漏
- [ ] 无编译警告
- [ ] 代码符合项目规范

### 8.3 性能验收
- [ ] 推理性能不低于重构前
- [ ] 内存使用不增加
- [ ] UI 响应流畅

### 8.4 架构验收
- [ ] 每个类职责单一
- [ ] 依赖关系清晰
- [ ] 易于扩展新后端
- [ ] 代码可读性提高

---

## 九、后续扩展

### 9.1 GPU 加速支持
完成重构后，可以轻松添加：
- NCNN Vulkan 后端
- ONNX Runtime GPU 后端
- TensorRT 后端

### 9.2 新模型支持
基于新架构，可以快速支持：
- 新的模型格式
- 新的模型类别
- 自定义模型

### 9.3 分布式推理
架构支持未来扩展：
- 多 GPU 并行
- 多机分布式
- 云端推理

---

## 十、总结

本次重构将 AI 推理模式从单体架构转变为分层架构，核心改进：

1. **职责单一**：每个类只负责一件事
2. **结构清晰**：四层架构，依赖关系明确
3. **易于维护**：文件大小合理，逻辑集中
4. **可扩展**：支持多种推理后端和硬件加速
5. **健壮性**：统一错误处理，资源管理规范

预计总工期：**8-12 天**
