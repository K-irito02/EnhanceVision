# Vulkan GPU 加速推理集成计划

> **创建日期**: 2026-04-04  
> **状态**: 📋 待审核  
> **目标**: 在现有纯 CPU 推理基础上，增加 NCNN Vulkan GPU 加速方案，实现 CPU/GPU 双模式切换

---

## 一、现状分析

### 1.1 当前架构（双轨并行）

| 组件 | 路径 | 状态 | 说明 |
|------|------|------|------|
| **AIEngine** | `src/core/AIEngine.cpp` | ✅ 生产中使用 | 主推理引擎，CPU-only，被 AIEnginePool/ProcessingController 调用 |
| **IInferenceBackend** | `src/core/inference/IInferenceBackend.h` | ⚠️ 已存在未接入 | 抽象接口 + 工厂模式，有 NCNNCPUBackend 实现 |
| **NCNNCPUBackend** | `src/core/inference/NCNNCPUBackend.cpp` | ⚠️ 已存在未接入 | 工厂模式的 CPU 后端 |
| **InferenceBackendFactory** | `src/core/inference/InferenceBackendFactory.cpp` | ⚠️ 部分完成 | 有 NCNN_Vulkan 枚举但无实际后端注册 |

### 1.2 关键约束（来自崩溃修复笔记）

1. **内存对齐**: 必须使用 32 字节对齐的 stride，逐行拷贝到 QImage
2. **AVFrame 中间缓冲区**: FFmpeg sws_scale 目标必须用 AVFrame 管理
3. **异步清理**: cleanupTask 必须 QTimer::singleShot(0) 延迟，禁止同步 take() 最后引用
4. **取消标志**: cancelProcess() 只设标志，不设 isProcessing=false
5. **单线程推理**: 当前 CPU 模式强制 num_threads=1 保证稳定性
6. **禁用优化项**: use_packing_layout/lightmode/local_pool_allocator/sgemm/winograd 全部关闭

### 1.3 构建系统现状

```cmake
# CMakeLists.txt 第 70-71 行
set(NCNN_VULKAN OFF CACHE BOOL "Disable Vulkan GPU acceleration in NCNN" FORCE)
```
Vulkan 被强制关闭，需改为可配置选项。

---

## 二、设计方案

### 2.1 总体策略：主引擎增强 + 后端工厂补充

采用 **"AIEngine 双模式 + NCNNVulkanBackend 工厂后端"** 的混合策略：

```
┌─────────────────────────────────────────────────────┐
│                   QML UI 层                          │
│  AIParamsPanel.qml (CPU/GPU 切换 ComboBox)          │
└──────────────────────┬──────────────────────────────┘
                       │ useGpu / backendType
                       ▼
┌─────────────────────────────────────────────────────┐
│              ProcessingController                    │
│  将 backendType 传递给 AIEnginePool                   │
└──────────────────────┬──────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────┐
│                AIEnginePool                         │
│  根据 backendType 创建对应类型的引擎                  │
│  CPU → AIEngine(CPU mode)                           │
│  GPU → AIEngine(Vulkan mode)                        │
└──────────────────────┬──────────────────────────────┘
                       │
                       ▼
┌─────────────────────────┬───────────────────────────┐
│      AIEngine (CPU)     │    AIEngine (Vulkan)       │
│  use_vulkan_compute=F   │  use_vulkan_compute=T      │
│  num_threads=1          │  num_threads=0 (GPU管理)   │
│  安全稳定优先           │  性能优先 + GPU安全检查     │
└─────────────────────────┴───────────────────────────┘
```

**设计理由**：
- 不引入新的引擎类到生产管线，最小化改动风险
- 复用已有的 AIEngine 基础设施（进度、取消、参数管理）
- 同时完善工厂模式体系（创建 NCNNVulkanBackend）

### 2.2 文件变更清单

| # | 操作 | 文件路径 | 变更内容 |
|---|------|----------|----------|
| 1 | **修改** | `CMakeLists.txt` | 启用 NCNN_VULKAN 可选，链接 Vulkan SDK |
| 2 | **新建** | `include/EnhanceVision/core/inference/NCNNVulkanBackend.h` | Vulkan 推理后端头文件 |
| 3 | **新建** | `src/core/inference/NCNNVulkanBackend.cpp` | Vulkan 推理后端实现 |
| 4 | **修改** | `include/EnhanceVision/core/AIEngine.h` | 新增 BackendType 属性、Vulkan 初始化方法 |
| 5 | **修改** | `src/core/AIEngine.cpp` | 支持 CPU/Vulkan 双模式切换、GPU 内存安全 |
| 6 | **修改** | `include/EnhanceVision/core/AIEnginePool.h` | 支持按任务指定后端类型 |
| 7 | **修改** | `src/core/AIEnginePool.cpp` | 根据后端类型创建不同配置的引擎 |
| 8 | **修改** | `src/controllers/ProcessingController.cpp` | 传递后端类型到引擎池 |
| 9 | **修改** | `qml/components/AIParamsPanel.qml` | CPU/GPU 模式切换 UI（ComboBox） |
| 10 | **修改** | `src/core/inference/InferenceBackendFactory.cpp` | 注册 NCNNVulkanBackend |

---

## 三、详细实施步骤

### 步骤 1: CMakeLists.txt — 启用 Vulkan 构建

**变更点**:
- 将 `NCNN_VULKAN OFF FORCE` 改为可选选项（默认 ON）
- 条件性链接 Vulkan 库和 Vulkan SDK
- 添加预处理器宏 `NCNN_VULKAN_AVAILABLE` 供代码运行时检测

```cmake
# 修改前:
set(NCNN_VULKAN OFF CACHE BOOL "Disable Vulkan GPU acceleration in NCNN" FORCE)

# 修改后:
option(NCNN_USE_VULKAN "Enable NCNN Vulkan GPU acceleration" ON)
if(NCNN_USE_VULKAN)
    set(NCNN_VULKAN ON CACHE BOOL "" FORCE)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Vulkan SDK found: ${Vulkan_VERSION}")
        add_definitions(-DNCNN_VULKAN_AVAILABLE=1)
    else()
        message(WARNING "Vulkan SDK not found, falling back to CPU only")
        set(NCNN_VULKAN OFF CACHE BOOL "" FORCE)
    endif()
else()
    set(NCNN_VULKAN OFF CACHE BOOL "" FORCE)
endif()
```

同时在 `target_link_libraries` 中条件性添加 Vulkan 库。

### 步骤 2-3: 创建 NCNNVulkanBackend（头文件 + 实现）

**职责**: 实现完整的 Vulkan GPU 推理后端，继承 IInferenceBackend

**关键设计要点**:
```
NCNNVulkanBackend
├── Vulkan 设备管理
│   ├── ncnn::GpuInstance  (全局单例，进程级共享)
│   ├── ncnn::GpuAllocator (显存分配器)
│   └── GPU 设备选择 (gpuDeviceId from BackendConfig)
│
├── NCNN Option 配置
│   ├── use_vulkan_compute = true
│   ├── num_threads = 0 (GPU 不需要 CPU 线程)
│   └── lightmode = true (GPU 上可以启用轻量模式)
│
├── 推理安全机制 (参考 CPU 崩溃修复笔记)
│   ├── 与 CPU 共享相同的 ImagePreprocessor/ImagePostprocessor
│   ├── 相同的 stride 对齐处理 (32字节)
│   └── 相同的取消/进度机制
│
└── GPU 特有处理
    ├── 显存不足检测与降级 (OOM → 自动回退 CPU)
    ├── Vulkan 设备热插拔检测
    └── clearInferenceCache() → 释放 GPU 缓存
```

**核心 API**:
```cpp
class NCNNVulkanBackend : public IInferenceBackend {
    Q_OBJECT
public:
    bool initialize(const BackendConfig& config) override;
    void shutdown() override;
    bool loadModel(const ModelInfo& model) override;
    InferenceResult inference(const InferenceRequest& request) override;
    
    BackendType type() const override { return BackendType::NCNN_Vulkan; }
    QString name() const override { return QStringLiteral("NCNN Vulkan Backend"); }
    BackendCapabilities capabilities() const override; // supportsGPU=true
    
private:
    // Vulkan 资源
    std::unique_ptr<ncnn::GpuInstance> m_gpuInstance;
    std::unique_ptr<ncnn::GpuAllocator> m_gpuAllocator;
    int m_gpuDeviceId = 0;
    
    // GPU 显存监控
    qint64 m_estimatedGpuMemoryMB = 0;
    
    // 推理方法 (复用 CPU 版本的逻辑结构)
    InferenceResult processSingle(const QImage& input);
    InferenceResult processTiled(const QImage& input, int tileSize);
    ncnn::Mat runInference(const ncnn::Mat& input);
};
```

### 步骤 4-5: 修改 AIEngine — 支持 CPU/Vulkan 双模式

**变更策略**: 在现有 AIEngine 基础上增加 Vulkan 能力，而非重写

**4.1 AIEngine.h 新增内容**:
```cpp
// 新增属性
Q_PROPERTY(BackendType backendType READ backendType NOTIFY backendTypeChanged)
Q_PROPERTY(bool gpuAvailable READ gpuAvailable CONSTANT)
Q_PROPERTY(QString gpuDeviceName READ gpuDeviceName NOTIFY gpuDeviceInfoChanged)

// 新增方法
Q_INVOKABLE bool setBackendType(BackendType type);
BackendType backendType() const;
bool gpuAvailable() const;
QString gpuDeviceName() const;

// Vulkan 专用
bool initializeVulkan(int gpuDeviceId = 0);
void shutdownVulkan();
bool isVulkanReady() const;

signals:
    void backendTypeChanged(BackendType type);
    void gpuDeviceInfoChanged();

// 新增成员变量
BackendType m_backendType = BackendType::NCNN_CPU;
std::atomic<bool> m_gpuAvailable{false};
QString m_gpuDeviceName;
#ifdef NCNN_VULKAN_AVAILABLE
std::unique_ptr<ncnn::GpuInstance> m_gpuInstance;
std::unique_ptr<ncnn::GpuAllocator> m_gpuAllocator;
int m_gpuDeviceId = -1;
#endif
```

**4.2 AIEngine.cpp 关键变更**:

**(a) 构造函数分支初始化**:
```cpp
AIEngine::AIEngine(QObject *parent) : QObject(parent), ... {
    // 默认 CPU 模式（保持现有行为不变）
    m_opt.num_threads = 1;
    m_opt.use_vulkan_compute = false;
    // ... 其他 CPU 安全选项保持不变
}
```

**(b) setBackendType() 方法**:
```cpp
bool AIEngine::setBackendType(BackendType type) {
    if (m_isProcessing.load()) {
        emit processError(tr("推理进行中，无法切换后端"));
        return false;
    }
    
    if (type == m_backendType) return true;
    
#ifdef NCNN_VULKAN_AVAILABLE
    if (type == BackendType::NCNN_Vulkan) {
        if (!initializeVulkan()) {
            qWarning() << "[AIEngine] Vulkan init failed, staying on CPU";
            return false;
        }
        m_backendType = BackendType::NCNN_Vulkan;
        m_opt.use_vulkan_compute = true;
        m_opt.num_threads = 0;  // GPU 模式不需要 CPU 多线程
        // GPU 可以启用更多优化
        m_opt.lightmode = true;
        m_opt.use_packing_layout = true;
    } else {
        shutdownVulkan();
        m_backendType = BackendType::NCNN_CPU;
        m_opt.use_vulkan_compute = false;
        m_opt.num_threads = 1;
        // 回到 CPU 安全配置
        m_opt.lightmode = false;
        m_opt.use_packing_layout = false;
    }
#else
    if (type == BackendType::NCNN_Vulkan) {
        emit processError(tr("编译时未启用 Vulkan 支持"));
        return false;
    }
#endif
    
    emit backendTypeChanged(m_backendType);
    return true;
}
```

**(c) initializeVulkan() 方法**:
```cpp
bool AIEngine::initializeVulkan(int gpuDeviceId) {
#ifdef NCNN_VULKAN_AVAILABLE
    try {
        m_gpuInstance = std::make_unique<ncnn::GpuInstance>();
        if (!m_gpuInstance->supportsVulkan()) {
            emit processError(tr("系统不支持 Vulkan"));
            return false;
        }
        
        m_gpuAllocator = std::make_unique<ncnn::GpuAllocator>(m_gpuInstance.get());
        m_gpuDeviceId = gpuDeviceId;
        m_gpuAvailable.store(true);
        
        // 获取设备信息
        m_gpuDeviceName = QString::fromStdString(
            m_gpuInstance->get_device_name(gpuDeviceId));
        
        qInfo() << "[AIEngine] Vulkan initialized successfully"
                << "device:" << m_gpuDeviceName;
                
        emit gpuDeviceInfoChanged();
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[AIEngine] Vulkan init exception:" << e.what();
        shutdownVulkan();
        return false;
    }
#else
    Q_UNUSED(gpuDeviceId);
    return false;
#endif
}
```

**(d) loadModel() 增强** — Vulkan 模式下使用 GPU allocator:
```cpp
// 在 loadModel() 中，加载模型后设置 GPU allocator
if (m_backendType == BackendType::NCNN_Vulkan && m_gpuAllocator) {
#if NCNN_VULKAN_AVAILABLE
    m_net.set_vulkan_device(m_gpuDeviceId);
#endif
}
```

**(e) clearInferenceCache() 增强** — Vulkan 需要释放 GPU 缓存:
```cpp
void AIEngine::clearInferenceCache() {
#ifdef NCNN_VULKAN_AVAILABLE
    if (m_backendType == BackendType::NCNN_Vulkan && m_gpuAllocator) {
        // NCNN Vulkan 会缓存 GPU 计算结果
        // 通过创建一个空推理来刷新缓存
        // 或调用 ncnn 内部的缓存清理接口
        m_gpuAllocator->clear_cache();
    }
#endif
}
```

**(f) runInference() 无需改动** — NCNN 的 extractor 在设置了 `use_vulkan_compute=true` 和 `set_vulkan_device()` 后会自动使用 GPU

### 步骤 6-7: 修改 AIEnginePool — 支持多后端类型

**6.1 AIEnginePool.h 新增**:
```cpp
// 新增方法
AIEngine* acquireWithBackend(const QString& taskId, BackendType backendType);
void setDefaultBackendType(BackendType type);
BackendType defaultBackendType() const;

// EngineSlot 新增字段
struct EngineSlot {
    AIEngine* engine = nullptr;
    EngineState state = EngineState::Uninitialized;
    QString taskId;
    BackendType backendType = BackendType::NCNN_CPU;  // 新增
    bool wasUsed = false;
    QString lastError;
};

// 成员变量
BackendType m_defaultBackendType = BackendType::NCNN_CPU;
```

**6.2 AIEnginePool.cpp 变更**:
- `createEngines()`: 引擎仍默认以 CPU 模式创建
- `acquire()` / `acquireWithBackend()`: 获取引擎后调用 `engine->setBackendType(type)`
- 支持同一池中混合使用 CPU/GPU 引擎（通过 slot 记录每个引擎的后端类型）

### 步骤 8: ProcessingController — 传递后端类型

**变更位置**: `launchAIVideoProcessor()` 和图片处理启动方法中
- 从 `Message.aiParams.useGpu` 或新的 `aiParams.backendType` 字段读取用户选择
- 调用 `m_aiEnginePool->acquireWithBackend(taskId, backendType)`
- 在引擎获取成功后确认后端类型已正确设置

### 步骤 9: AIParamsPanel.qml — CPU/GPU 切换 UI

**当前状态** (第 279-300 行):
```qml
// 当前：硬编码显示 "CPU 模式"
RowLayout {
    Text { text: qsTr("推理模式") }
    Badge { text: qsTr("CPU 模式") }
}
InfoBanner { text: qsTr("当前使用 CPU 模式运行。") }
Component.onCompleted: { root.useGpu = false }
```

**改造为交互式 ComboBox**:
```qml
// ===== 推理设备选择 =====
RowLayout {
    Layout.fillWidth: true; spacing: 8; visible: engine !== null
    
    ColoredIcon { source: Theme.icon("zap"); iconSize: 14;
        color: root.useGpu ? Theme.colors.primary : Theme.colors.mutedForeground }
    
    Text { text: qsTr("推理设备"); color: Theme.colors.foreground; 
           font.pixelSize: 12; Layout.fillWidth: true }
    
    ComboBox {
        id: _backendCombo
        model: ListElement { text: qsTr("CPU (兼容性最佳)"); value: "cpu" }
        implicitWidth: 160
        
        popup.y: height + 4
        
        onActivated: function(index) {
            var val = model.get(index).value
            var oldGpu = root.useGpu
            root.useGpu = (val === "vulkan")
            _updateBackendStatus()
            root.paramsChanged()
        }
        
        Component.onCompleted: {
            // 检测 Vulkan 是否可用
            if (engine && engine.gpuAvailable) {
                model.append({text: qsTr("Vulkan GPU (加速)"), value: "vulkan"})
                // 如果之前选择了 GPU 且可用，恢复选择
                if (root.useGpu) currentIndex = 1
            }
            currentIndex = root.useGpu ? 1 : 0
            _updateBackendStatus()
        }
    }
}

// 动态状态信息栏
InfoBanner {
    id: _backendStatusBanner
    Layout.fillWidth: true
    property bool isGpuMode: root.useGpu && engine && engine.gpuAvailable
    text: isGpuMode 
        ? qsTr("GPU 加速模式 · 设备: %1").arg(engine ? engine.gpuDeviceName : "")
        : qsTr("CPU 模式 · 兼容性最佳，适合所有硬件")
    isWarning: root.useGpu && !isGpuMode
    visible: engine !== null
}

function _updateBackendStatus() {
    _backendStatusBanner.isGpuChanged()
}
```

### 步骤 10: InferenceBackendFactory — 注册 Vulkan 后端

在 `registry()` 静态方法中注册 NCNNVulkanBackend：

```cpp
if (s_registry.isEmpty()) {
    s_registry[BackendType::NCNN_CPU] = []() -> std::unique_ptr<IInferenceBackend> {
        return std::make_unique<NCNNCPUBackend>();
    };
#ifdef NCNN_VULKAN_AVAILABLE
    s_registry[BackendType::NCNN_Vulkan] = []() -> std::unique_ptr<IInferenceBackend> {
        return std::make_unique<NCNNVulkanBackend>();
    };
#endif
}
```

---

## 四、Vulkan 特殊安全措施

### 4.1 GPU 内存安全（参考 CPU 崩溃笔记）

| 问题 | CPU 方案 | Vulkan 对应方案 |
|------|----------|----------------|
| 堆损坏 (0xc0000374) | AVFrame 中间缓冲区 | 同上（预处理相同） |
| stride 不匹配 | 32 字节对齐 + 逐行拷贝 | **相同方案复用** |
| Use-After-Free | 异步 cleanupTask | **相同方案 + GPU 资源等待** |
| 显存不足 (OOM) | N/A | **捕获 OOM 异常 → 自动降级 CPU** |
| GPU 驱动崩溃 | N/A | **try-catch 包裹所有 Vulkan 调用** |
| 多任务 GPU 资源竞争 | 单线程推理 | **限制并发 GPU 任务数 ≤ 2** |

### 4.2 Vulkan OOM 降级策略

```cpp
InferenceResult NCNNVulkanBackend::inference(const InferenceRequest& request) {
    try {
        // 正常推理流程...
        return result;
    } catch (const ncnn::VulkanOutOfMemoryException& e) {
        qWarning() << "[NCNNVulkanBackend] GPU OOM:" << e.what();
        emitError(tr("GPU 显存不足，建议切换到 CPU 模式或减小分块大小"));
        return InferenceResult::makeFailure(
            tr("GPU 显存不足"), 
            static_cast<int>(InferenceError::OutOfMemory));
    } catch (const std::exception& e) {
        qWarning() << "[NCNNVulkanBackend] Vulkan error:" << e.what();
        emitError(tr("GPU 推理错误: %1").arg(e.what()));
        return InferenceResult::makeFailure(
            tr("GPU 推理失败"),
            static_cast<int>(InferenceError::InferenceFailed));
    }
}
```

### 4.3 Vulkan 设备探测与就绪检查

应用启动时或设置页面打开时，执行一次 Vulkan 就绪探测：

```cpp
// AIEngine 或独立工具类中
struct VulkanProbeResult {
    bool available = false;
    QString deviceName;
    qint64 totalMemoryMB = 0;
    QString driverVersion;
    QStringList supportedExtensions;
    QString errorReason;
};

static VulkanProbeResult probeVulkan() {
    VulkanProbeResult result;
#ifdef NCNN_VULKAN_AVAILABLE
    try {
        auto instance = std::make_unique<ncnn::GpuInstance>();
        if (!instance->supportsVulkan()) {
            result.errorReason = QStringLiteral("系统不支持 Vulkan API");
            return result;
        }
        
        int deviceCount = instance->get_device_count();
        if (deviceCount <= 0) {
            result.errorReason = QStringLiteral("未找到支持 Vulkan 的 GPU");
            return result;
        }
        
        result.available = true;
        result.deviceName = QString::fromStdString(instance->get_device_name(0));
        // ... 更多设备信息
    } catch (...) {
        result.errorReason = QStringLiteral("Vulkan 初始化异常");
    }
#else
    result.errorReason = QStringLiteral("程序未编译 Vulkan 支持");
#endif
    return result;
}
```

---

## 五、UI 交互流程

### 5.1 用户操作流程

```
用户打开 AIParamsPanel
    │
    ▼
组件加载完成 → 探测 Vulkan 可用性
    │
    ├─ Vulkan 可用 → ComboBox 显示 [CPU, Vulkan GPU] 两个选项
    │                   默认选中 CPU（安全优先）
    │
    └─ Vulkan 不可用 → ComboBox 仅显示 [CPU]
                        显示提示 "未检测到 Vulkan GPU"
    │
    ▼
用户选择 "Vulkan GPU"
    │
    ▼
paramsChanged() 信号发射
    │
    ▼
ProcessingController 接收新参数
    │
    ▼
下次推理时: AIEnginePool.acquireWithBackend(taskId, NCNN_Vulkan)
    │
    ▼
AIEngine.setBackendType(NCNN_Vulkan) → initializeVulkan()
    │
    ▼
后续所有推理自动走 GPU 路径
    │
    ▼ (如果推理出错)
自动降级提示 → 用户可切回 CPU 重试
```

### 5.2 进度条适配

Vulkan 模式的进度报告与 CPU 完全一致：
- 使用相同的 ProgressReporter
- processSingle/processTiled 进度节点不变
- 唯一区别：Vulkan 推理速度更快，进度更新频率更高

### 5.3 各种比例/尺寸图片视频处理

Vulkan 模式完全复用 CPU 模式的尺寸自适应逻辑：
- `computeAutoTileSizeForModel()` — GPU 显存更大，tileSize 可以更大
- `processTiled()` — 分块逻辑相同，但每块处理更快
- `processSingle()` — 小图直接处理
- 竖向视频 (360x640)、横向视频 (1920x1080)、方形 — 全部支持

**GPU 模式下的 tileSize 调优**:
```cpp
// Vulkan 模式可以使用更大的分块（显存通常 > 4GB）
int NCNNVulkanBackend::computeGpuTileSize(const QSize& imageSize, const ModelInfo& model) {
    // GPU 显存充足时，增大 tile 以减少拼接伪影
    const qint64 kGpuAvailableMB = 4096;  // 假设 4GB 显存
    // ... 类似 CPU 的计算逻辑，但 kFactor 更小（GPU 效率更高）
}
```

---

## 六、数据流变更

### 6.1 参数传递链路

```
QML: AIParamsPanel.getParams()
  → params.backendType = useGpu ? "NCNN_Vulkan" : "NCNN_CPU"
  
ProcessingController.launchAIVideoProcessor()
  → 读取 message.aiParams.backendType
  → m_aiEnginePool->acquireWithBackend(taskId, backendType)

AIEnginePool.acquireWithBackend()
  → engine = acquire(taskId)  // 获取空闲引擎
  → engine->setBackendType(backendType)  // 切换模式
  → 返回引擎

AIEngine (推理时)
  → 检查 m_backendType 决定使用哪种 NCNN Option
  → Vulkan: use_vulkan_compute=true + GpuAllocator
  → CPU: use_vulkan_compute=false + 安全配置
```

### 6.2 DataTypes.h 变更

```cpp
// AIParams 结构体新增:
struct AIParams {
    // ... 现有字段 ...
    bool useGpu = false;       // 保留向后兼容
    QString backendType = "NCNN_CPU";  // 新增: 精确后端类型
};
```

---

## 七、测试验证计划

### 7.1 功能测试

| 测试场景 | 预期结果 |
|----------|----------|
| 启动程序，无 Vulkan GPU | 仅显示 CPU 选项，程序正常运行 |
| 启动程序，有 Vulkan GPU | 显示 CPU + Vulkan GPU 两个选项 |
| 选择 Vulkan GPU → 推理图片 | GPU 加速推理完成，速度明显快于 CPU |
| 选择 Vulkan GPU → 推理竖向视频 (360x640) | 正常处理，不崩溃 |
| 选择 Vulkan GPU → 推理横向视频 (1920x1080) | 正常处理，不崩溃 |
| Vulkan 推理中点击取消 | 正常取消，不崩溃（遵循 crash-fix-notes） |
| 快速连续切换 CPU ↔ GPU | 模式切换正常，无资源泄漏 |
| Vulkan 不可用时选择 GPU | 显示错误提示，自动保持在 CPU 模式 |
| 多任务并发 GPU 推理 | 不超过 2 个并发 GPU 任务，其余排队 |

### 7.2 性能基准对比

| 测试图像 | CPU 耗时 (参考) | Vulkan 预期耗时 | 加速比预期 |
|----------|----------------|-----------------|-----------|
| 360x640 (竖向) | ~7 秒/帧 | ~0.5-1 秒/帧 | 7-14x |
| 640x360 (横向) | ~5 秒/帧 | ~0.3-0.8 秒/帧 | 6-16x |
| 1080x1920 (大图) | ~20 秒/帧 | ~2-4 秒/帧 | 5-10x |

### 7.3 崩溃回归测试（必须全部通过）

基于 cpu-video-inference-crash-fix-notes 中的所有修复点：
- [ ] 视频推理不崩溃（堆损坏 0xc0000374）
- [ ] 取消/删除正在运行的 AI 任务不崩溃（Use-After-Free）
- [ ] 快速连续删除任务不崩溃
- [ ] 多任务场景删除一个不影响其他
- [ ] UI 无卡顿（取消时）

---

## 八、风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 用户系统无 Vulkan 驱动 | 无法使用 GPU 模式 | 自动降级 CPU，UI 明确提示 |
| GPU 显存不足 | 推理失败 | OOM 捕获 + 自动降级 + 减小 tile |
| Vulkan 驱动 bug 导致崩溃 | 程序崩溃 | try-catch 所有 Vulkan 调用 + 崩溃恢复 |
| NCNN Vulkan 编译问题 | 项目无法构建 | CMake option 可关闭 Vulkan |
| GPU/CPU 结果微小差异 | 质量不一致 | 使用相同模型和参数，数值差异应在 ±1 以内 |
| 多 GPU 设备冲突 | 选错设备 | 默认使用 device 0，提供设备选择 UI |

---

## 九、实施顺序与依赖关系

```
步骤 1 (CMakeLists.txt)
    │
    ├──▶ 步骤 2-3 (NCNNVulkanBackend.h/cpp) ← 可与步骤 4-5 并行
    │
    ├──▶ 步骤 4-5 (AIEngine 修改)
    │         │
    │         ├──▶ 步骤 6-7 (AIEnginePool 修改)
    │         │         │
    │         │         └──▶ 步骤 8 (ProcessingController)
    │         │
    │         └──▶ 步骤 10 (Factory 注册)
    │
    └──▶ 步骤 9 (AIParamsPanel.qml UI) ← 可与步骤 4-8 并行
              │
              ▼
        步骤 11 (构建验证 + 测试)
```

**建议实施分组**:
- **第一批 (核心)**: 步骤 1 + 4-5 + 9 — 最小可用版本（AIEngine 双模式 + UI 切换）
- **第二批 (完整)**: 步骤 2-3 + 6-7 + 8 + 10 — 工厂模式 + 引擎池集成
- **第三批 (验证)**: 步骤 11 — 全面测试
