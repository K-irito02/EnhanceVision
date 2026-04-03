/**
 * @file IInferenceBackend.h
 * @brief AI 推理后端抽象接口（纯虚接口，无 Q_OBJECT）
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_IINFERENCEBACKEND_H
#define ENHANCEVISION_IINFERENCEBACKEND_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QVariantMap>

namespace EnhanceVision {

struct ModelInfo;
struct BackendConfig;
struct InferenceRequest;
struct InferenceResult;
struct BackendCapabilities;
enum class InferenceError;
enum class BackendType;

class IInferenceBackend : public QObject
{
public:
    explicit IInferenceBackend(QObject* parent = nullptr) : QObject(parent) {}
    ~IInferenceBackend() override = default;

    virtual bool initialize(const BackendConfig& config) = 0;
    virtual void shutdown() = 0;

    virtual bool loadModel(const ModelInfo& model) = 0;
    virtual void unloadModel() = 0;
    virtual bool isModelLoaded() const = 0;
    virtual QString currentModelId() const = 0;
    virtual ModelInfo currentModel() const = 0;

    virtual InferenceResult inference(const InferenceRequest& request) = 0;

    virtual BackendType type() const = 0;
    virtual QString name() const = 0;
    virtual BackendCapabilities capabilities() const = 0;
    virtual bool isInferenceRunning() const = 0;
};

} // namespace EnhanceVision

Q_DECLARE_METATYPE(EnhanceVision::IInferenceBackend*)

#endif // ENHANCEVISION_IINFERENCEBACKEND_H
