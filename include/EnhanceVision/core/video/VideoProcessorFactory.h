/**
 * @file VideoProcessorFactory.h
 * @brief 视频处理器工厂
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEOPROCESSORFACTORY_H
#define ENHANCEVISION_VIDEOPROCESSORFACTORY_H

#include "VideoProcessingTypes.h"
#include <memory>
#include <QString>

namespace EnhanceVision {

class AIVideoProcessor;
class AIEngine;
struct ModelInfo;

class VideoProcessorFactory {
public:
    static std::unique_ptr<AIVideoProcessor> createAIProcessor(
        AIEngine* engine,
        const ModelInfo& model,
        const VideoProcessingConfig& config);
    
    static bool isVideoFile(const QString& path);
    static VideoMetadata probeVideoMetadata(const QString& path);
    static QString suggestOutputPath(const QString& inputPath, VideoFormat format = VideoFormat::MP4);
    
private:
    VideoProcessorFactory() = delete;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOPROCESSORFACTORY_H
