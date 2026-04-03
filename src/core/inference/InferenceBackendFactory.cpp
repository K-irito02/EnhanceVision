/**
 * @file InferenceBackendFactory.cpp
 * @brief 推理后端工厂实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/inference/InferenceBackendFactory.h"
#include "EnhanceVision/core/inference/NCNNCPUBackend.h"
#ifdef NCNN_VULKAN_AVAILABLE
#include "EnhanceVision/core/inference/NCNNVulkanBackend.h"
#endif
#include <QDebug>
#include <QMap>

namespace EnhanceVision {

QMap<BackendType, InferenceBackendFactory::BackendCreator>& InferenceBackendFactory::registry()
{
    static QMap<BackendType, BackendCreator> s_registry;
    
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
    
    return s_registry;
}

std::unique_ptr<IInferenceBackend> InferenceBackendFactory::create(BackendType type)
{
    BackendType actualType = type;
    
    if (type == BackendType::Auto) {
        actualType = recommendedBackend();
    }
    
    auto& reg = registry();
    auto it = reg.find(actualType);
    
    if (it != reg.end()) {
        auto backend = it.value()();
        if (backend) {
            qInfo() << "[InferenceBackendFactory] Created backend:" << backendTypeName(actualType);
            return backend;
        }
    }
    
    qWarning() << "[InferenceBackendFactory] Failed to create backend:" << backendTypeName(actualType);
    
    if (type == BackendType::Auto && actualType != BackendType::NCNN_CPU) {
        qInfo() << "[InferenceBackendFactory] Falling back to NCNN_CPU";
        return create(BackendType::NCNN_CPU);
    }
    
    return nullptr;
}

QList<BackendType> InferenceBackendFactory::availableBackends()
{
    QList<BackendType> available;
    
    auto& reg = registry();
    for (auto it = reg.begin(); it != reg.end(); ++it) {
        available.append(it.key());
    }
    
    return available;
}

BackendType InferenceBackendFactory::recommendedBackend()
{
    auto available = availableBackends();
    
    if (available.contains(BackendType::NCNN_Vulkan)) {
        return BackendType::NCNN_Vulkan;
    }
    
    if (available.contains(BackendType::ONNX_GPU)) {
        return BackendType::ONNX_GPU;
    }
    
    if (available.contains(BackendType::NCNN_CPU)) {
        return BackendType::NCNN_CPU;
    }
    
    if (!available.isEmpty()) {
        return available.first();
    }
    
    return BackendType::NCNN_CPU;
}

bool InferenceBackendFactory::isBackendAvailable(BackendType type)
{
    auto& reg = registry();
    return reg.contains(type);
}

QString InferenceBackendFactory::backendInfo(BackendType type)
{
    switch (type) {
        case BackendType::Auto:
            return QStringLiteral("自动选择最佳后端");
        case BackendType::NCNN_CPU:
            return QStringLiteral("NCNN CPU 后端 - 纯 CPU 推理，兼容性最好");
        case BackendType::NCNN_Vulkan:
            return QStringLiteral("NCNN Vulkan 后端 - GPU 加速推理（需要 Vulkan 支持）");
        case BackendType::ONNX_CPU:
            return QStringLiteral("ONNX Runtime CPU 后端 - ONNX 模型 CPU 推理");
        case BackendType::ONNX_GPU:
            return QStringLiteral("ONNX Runtime GPU 后端 - ONNX 模型 GPU 加速推理");
        case BackendType::TensorRT:
            return QStringLiteral("NVIDIA TensorRT 后端 - 高性能 GPU 推理（需要 NVIDIA GPU）");
        case BackendType::CoreML:
            return QStringLiteral("Apple CoreML 后端 - Apple 设备专用加速");
        case BackendType::Custom:
            return QStringLiteral("自定义后端");
        default:
            return QStringLiteral("未知后端");
    }
}

bool InferenceBackendFactory::registerBackend(BackendType type, BackendCreator creator)
{
    if (!creator) {
        qWarning() << "[InferenceBackendFactory] Invalid creator for backend:" << backendTypeName(type);
        return false;
    }
    
    auto& reg = registry();
    reg[type] = std::move(creator);
    
    qInfo() << "[InferenceBackendFactory] Registered backend:" << backendTypeName(type);
    return true;
}

} // namespace EnhanceVision
