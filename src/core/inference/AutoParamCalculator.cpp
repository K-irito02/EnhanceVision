/**
 * @file AutoParamCalculator.cpp
 * @brief 自动参数计算器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/inference/AutoParamCalculator.h"
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace EnhanceVision {

int AutoParamCalculator::computeTileSize(const QSize& inputSize, const ModelInfo& model)
{
    return computeTileSizeWithMemoryLimit(inputSize, model, 256);
}

int AutoParamCalculator::computeTileSizeWithMemoryLimit(const QSize& inputSize, 
                                                         const ModelInfo& model,
                                                         qint64 maxMemoryMB)
{
    if (!model.isLoaded && model.paramPath.isEmpty()) {
        return 0;
    }
    
    if (model.tileSize == 0) {
        return 0;
    }
    
    const int w = inputSize.width();
    const int h = inputSize.height();
    const int scale = std::max(1, model.scaleFactor);
    const int channels = model.outputChannels > 0 ? model.outputChannels : 3;
    
    double kFactor = 8.0;
    const int layerCount = model.layerCount;
    
    if (layerCount > 500) {
        kFactor = 48.0;
    } else if (layerCount > 200) {
        kFactor = 32.0;
    } else if (layerCount > 50) {
        kFactor = 16.0;
    } else if (layerCount > 0) {
        kFactor = 10.0;
    } else if (model.sizeBytes > 50LL * 1024 * 1024) {
        kFactor = 48.0;
    } else if (model.sizeBytes > 20LL * 1024 * 1024) {
        kFactor = 32.0;
    } else if (model.sizeBytes > 5LL * 1024 * 1024) {
        kFactor = 16.0;
    }
    
    constexpr qint64 kBytesPerMB = 1024LL * 1024;
    
    auto memForTile = [&](int tile) -> double {
        const qint64 px = static_cast<qint64>(tile) * tile;
        const qint64 bytes = px * channels
                             * static_cast<qint64>(sizeof(float))
                             * static_cast<qint64>(scale * scale);
        return static_cast<double>(bytes) * kFactor / kBytesPerMB;
    };
    
    const int maxPossibleTile = std::max(w, h);
    const int minTile = 64;
    const int maxTile = std::min(512, maxPossibleTile);
    const int step = 32;
    
    int bestTile = minTile;
    for (int t = maxTile; t >= minTile; t -= step) {
        if (memForTile(t) <= static_cast<double>(maxMemoryMB)) {
            bestTile = t;
            break;
        }
    }
    
    if (w <= bestTile && h <= bestTile) {
        return 0;
    }
    
    return bestTile;
}

QVariantMap AutoParamCalculator::computeAutoParams(const QSize& mediaSize, 
                                                    bool isVideo,
                                                    const ModelInfo& model)
{
    switch (model.category) {
        case ModelCategory::SuperResolution:
            return computeSuperResolutionParams(mediaSize, isVideo, model);
        case ModelCategory::Denoising:
            return computeDenoisingParams(mediaSize, isVideo, model);
        case ModelCategory::Deblurring:
            return computeDeblurringParams(mediaSize, isVideo, model);
        case ModelCategory::Dehazing:
            return computeDehazingParams(mediaSize, isVideo, model);
        case ModelCategory::Colorization:
            return computeColorizationParams(mediaSize, isVideo, model);
        case ModelCategory::LowLight:
            return computeLowLightParams(mediaSize, isVideo, model);
        case ModelCategory::FrameInterpolation:
            return computeFrameInterpolationParams(mediaSize, isVideo, model);
        case ModelCategory::Inpainting:
            return computeInpaintingParams(mediaSize, isVideo, model);
        default:
            return QVariantMap();
    }
}

QVariantMap AutoParamCalculator::mergeWithDefaultParams(const QVariantMap& userParams,
                                                         const ModelInfo& model)
{
    QVariantMap merged = userParams;
    
    if (!model.id.isEmpty() && !model.supportedParams.isEmpty()) {
        for (auto it = model.supportedParams.begin(); it != model.supportedParams.end(); ++it) {
            const QString& paramKey = it.key();
            const QVariantMap& paramMeta = it.value().toMap();
            
            if (!merged.contains(paramKey)) {
                if (paramMeta.contains("default")) {
                    merged[paramKey] = paramMeta["default"];
                }
            } else {
                QVariant& val = merged[paramKey];
                QString type = paramMeta["type"].toString();
                
                if (type == "int" || type == "float") {
                    double minVal = paramMeta["min"].toDouble();
                    double maxVal = paramMeta["max"].toDouble();
                    val = std::clamp(val.toDouble(), minVal, maxVal);
                } else if (type == "bool") {
                    val = val.toBool();
                }
            }
        }
    }
    
    return merged;
}

QVariantMap AutoParamCalculator::computeSuperResolutionParams(const QSize& mediaSize,
                                                               bool isVideo,
                                                               const ModelInfo& model)
{
    QVariantMap result;
    
    const int w = mediaSize.width();
    const int h = mediaSize.height();
    const qint64 pixels = static_cast<qint64>(w) * h;
    const double modelScale = static_cast<double>(model.scaleFactor);
    
    if (model.supportedParams.contains("outscale")) {
        double s = modelScale;
        if (pixels > 2073600) {
            s = std::max(1.0, modelScale * 0.5);
        } else if (pixels > 921600) {
            s = std::max(1.0, modelScale * 0.75);
        }
        s = std::round(s * 2.0) / 2.0;
        result["outscale"] = clampParam(model.supportedParams, "outscale", s);
    }
    
    if (model.supportedParams.contains("tta_mode")) {
        result["tta_mode"] = (!isVideo && pixels <= 262144);
    }
    
    if (model.supportedParams.contains("face_enhance")) {
        result["face_enhance"] = !isVideo;
    }
    
    if (model.supportedParams.contains("uhd_mode")) {
        result["uhd_mode"] = (pixels >= 8294400);
    }
    
    if (model.supportedParams.contains("fp32")) {
        result["fp32"] = (pixels <= 262144 && !isVideo);
    }
    
    if (model.supportedParams.contains("denoise")) {
        float dn = isVideo ? 0.3f : (pixels < 65536 ? 0.5f : 0.2f);
        result["denoise"] = static_cast<double>(clampParam(model.supportedParams, "denoise", dn));
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeDenoisingParams(const QSize& mediaSize,
                                                         bool isVideo,
                                                         const ModelInfo& model)
{
    QVariantMap result;
    
    const int w = mediaSize.width();
    const int h = mediaSize.height();
    const qint64 pixels = static_cast<qint64>(w) * h;
    
    if (model.supportedParams.contains("noise_threshold")) {
        float t = 50.0f;
        if (pixels < 65536) {
            t = 70.0f;
        } else if (pixels < 262144) {
            t = 60.0f;
        } else if (pixels > 2073600) {
            t = 35.0f;
        }
        if (isVideo) {
            t = std::min(t + 10.0f, 80.0f);
        }
        result["noise_threshold"] = static_cast<double>(clampParam(model.supportedParams, "noise_threshold", t));
    }
    
    if (model.supportedParams.contains("noise_level")) {
        int lv = isVideo ? 2 : 3;
        if (pixels < 65536) {
            lv = isVideo ? 3 : 4;
        } else if (pixels > 2073600) {
            lv = isVideo ? 1 : 2;
        }
        result["noise_level"] = static_cast<int>(clampParam(model.supportedParams, "noise_level", lv));
    }
    
    if (model.supportedParams.contains("color_denoise")) {
        result["color_denoise"] = !isVideo;
    }
    
    if (model.supportedParams.contains("sharpness_preserve")) {
        result["sharpness_preserve"] = (pixels >= 409600);
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeDeblurringParams(const QSize& mediaSize,
                                                          bool isVideo,
                                                          const ModelInfo& model)
{
    QVariantMap result;
    
    const qint64 pixels = static_cast<qint64>(mediaSize.width()) * mediaSize.height();
    
    if (model.supportedParams.contains("deblur_strength")) {
        float s = 1.0f;
        if (pixels < 65536) {
            s = isVideo ? 1.0f : 1.3f;
        } else if (pixels > 2073600) {
            s = isVideo ? 0.6f : 0.8f;
        } else if (isVideo) {
            s = 0.8f;
        }
        result["deblur_strength"] = static_cast<double>(clampParam(model.supportedParams, "deblur_strength", s));
    }
    
    if (model.supportedParams.contains("iterations")) {
        result["iterations"] = static_cast<int>(clampParam(model.supportedParams, "iterations", isVideo ? 1.0 : 2.0));
    }
    
    if (model.supportedParams.contains("motion_blur")) {
        result["motion_blur"] = isVideo;
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeDehazingParams(const QSize& mediaSize,
                                                        bool isVideo,
                                                        const ModelInfo& model)
{
    QVariantMap result;
    
    const int w = mediaSize.width();
    const int h = mediaSize.height();
    const qint64 pixels = static_cast<qint64>(w) * h;
    const double aspectRatio = (h > 0) ? static_cast<double>(w) / h : 1.0;
    const bool isUltraWide = (aspectRatio >= 2.0);
    const bool isPortrait = (aspectRatio <= 0.7);
    
    if (model.supportedParams.contains("dehaze_strength")) {
        float s = 1.0f;
        if (isVideo && (isUltraWide || isPortrait)) {
            s = 1.1f;
        }
        result["dehaze_strength"] = static_cast<double>(clampParam(model.supportedParams, "dehaze_strength", s));
    }
    
    if (model.supportedParams.contains("sky_protect")) {
        result["sky_protect"] = (pixels >= 409600);
    }
    
    if (model.supportedParams.contains("color_correct")) {
        result["color_correct"] = true;
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeColorizationParams(const QSize& mediaSize,
                                                            bool isVideo,
                                                            const ModelInfo& model)
{
    QVariantMap result;
    
    const int w = mediaSize.width();
    const int h = mediaSize.height();
    const int maxDim = std::max(w, h);
    const qint64 pixels = static_cast<qint64>(w) * h;
    
    if (model.supportedParams.contains("render_factor")) {
        int rf = 35;
        if (maxDim >= 1920) {
            rf = isVideo ? 38 : 45;
        } else if (maxDim >= 1280) {
            rf = isVideo ? 35 : 40;
        } else if (maxDim <= 256) {
            rf = 20;
        }
        result["render_factor"] = static_cast<int>(clampParam(model.supportedParams, "render_factor", rf));
    }
    
    if (model.supportedParams.contains("artistic_mode")) {
        result["artistic_mode"] = !isVideo;
    }
    
    if (model.supportedParams.contains("temporal_consistency")) {
        result["temporal_consistency"] = isVideo;
    }
    
    if (model.supportedParams.contains("saturation_boost")) {
        float sat = (pixels > 921600) ? 0.8f : 1.0f;
        result["saturation_boost"] = static_cast<double>(clampParam(model.supportedParams, "saturation_boost", sat));
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeLowLightParams(const QSize& mediaSize,
                                                        bool isVideo,
                                                        const ModelInfo& model)
{
    QVariantMap result;
    
    const qint64 pixels = static_cast<qint64>(mediaSize.width()) * mediaSize.height();
    
    if (model.supportedParams.contains("enhancement_strength")) {
        float es = 1.0f;
        if (pixels < 65536) {
            es = isVideo ? 1.1f : 1.3f;
        } else if (pixels > 2073600) {
            es = isVideo ? 0.8f : 0.9f;
        } else if (isVideo) {
            es = 0.9f;
        }
        result["enhancement_strength"] = static_cast<double>(clampParam(model.supportedParams, "enhancement_strength", es));
    }
    
    if (model.supportedParams.contains("exposure_correction")) {
        result["exposure_correction"] = true;
    }
    
    if (model.supportedParams.contains("noise_suppression")) {
        result["noise_suppression"] = isVideo || (pixels < 262144);
    }
    
    if (model.supportedParams.contains("gamma_correction")) {
        result["gamma_correction"] = true;
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeFrameInterpolationParams(const QSize& mediaSize,
                                                                   bool isVideo,
                                                                   const ModelInfo& model)
{
    Q_UNUSED(isVideo)
    
    QVariantMap result;
    const qint64 pixels = static_cast<qint64>(mediaSize.width()) * mediaSize.height();
    
    if (model.supportedParams.contains("time_step")) {
        result["time_step"] = static_cast<double>(clampParam(model.supportedParams, "time_step", 0.5));
    }
    
    if (model.supportedParams.contains("uhd_mode")) {
        result["uhd_mode"] = (pixels >= 8294400);
    }
    
    if (model.supportedParams.contains("tta_spatial")) {
        result["tta_spatial"] = false;
    }
    
    if (model.supportedParams.contains("tta_temporal")) {
        result["tta_temporal"] = false;
    }
    
    if (model.supportedParams.contains("scale")) {
        result["scale"] = static_cast<int>(clampParam(model.supportedParams, "scale", 1.0));
    }
    
    if (model.supportedParams.contains("scene_detection")) {
        result["scene_detection"] = true;
    }
    
    return result;
}

QVariantMap AutoParamCalculator::computeInpaintingParams(const QSize& mediaSize,
                                                          bool isVideo,
                                                          const ModelInfo& model)
{
    QVariantMap result;
    
    const int maxDim = std::max(mediaSize.width(), mediaSize.height());
    const qint64 pixels = static_cast<qint64>(mediaSize.width()) * mediaSize.height();
    
    if (model.supportedParams.contains("inpaint_radius")) {
        int r = 3;
        if (maxDim >= 1920) {
            r = 5;
        } else if (maxDim >= 1280) {
            r = 4;
        } else if (maxDim <= 256) {
            r = 2;
        }
        result["inpaint_radius"] = static_cast<int>(clampParam(model.supportedParams, "inpaint_radius", r));
    }
    
    if (model.supportedParams.contains("inpaint_method")) {
        int method = (!isVideo && pixels >= 262144) ? 1 : 0;
        result["inpaint_method"] = static_cast<int>(clampParam(model.supportedParams, "inpaint_method", method));
    }
    
    if (model.supportedParams.contains("feather_edge")) {
        result["feather_edge"] = (pixels >= 262144);
    }
    
    return result;
}

double AutoParamCalculator::clampParam(const QVariantMap& params,
                                        const QString& key,
                                        double rawValue)
{
    if (!params.contains(key)) {
        return rawValue;
    }
    
    const QVariantMap& meta = params[key].toMap();
    double lo = meta.contains("min") ? meta["min"].toDouble() : -1e9;
    double hi = meta.contains("max") ? meta["max"].toDouble() : 1e9;
    
    return std::clamp(rawValue, lo, hi);
}

int AutoParamCalculator::computeDynamicPadding(const ModelInfo& model)
{
    int padding = model.tilePadding;
    const int layerCount = model.layerCount;
    
    if (layerCount > 500) {
        padding = std::max(padding, 64);
    } else if (layerCount > 200) {
        padding = std::max(padding, 48);
    } else if (layerCount > 50) {
        padding = std::max(padding, 24);
    }
    
    return padding;
}

} // namespace EnhanceVision
