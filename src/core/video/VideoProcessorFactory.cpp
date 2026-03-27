/**
 * @file VideoProcessorFactory.cpp
 * @brief 视频处理器工厂实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/VideoProcessorFactory.h"
#include "EnhanceVision/core/video/AIVideoProcessor.h"
#include "EnhanceVision/core/AIEngine.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QFileInfo>
#include <QSet>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace EnhanceVision {

std::unique_ptr<AIVideoProcessor> VideoProcessorFactory::createAIProcessor(
    AIEngine* engine,
    const ModelInfo& model,
    const VideoProcessingConfig& config)
{
    auto processor = std::make_unique<AIVideoProcessor>();
    processor->setAIEngine(engine);
    processor->setModelInfo(model);
    processor->setConfig(config);
    return processor;
}

bool VideoProcessorFactory::isVideoFile(const QString& path)
{
    static const QSet<QString> videoExtensions = {
        "mp4", "avi", "mov", "mkv", "webm", "flv", "wmv", "m4v", "mpg", "mpeg", "ts", "mts", "m2ts"
    };
    
    QFileInfo info(path);
    return videoExtensions.contains(info.suffix().toLower());
}

VideoMetadata VideoProcessorFactory::probeVideoMetadata(const QString& path)
{
    VideoMetadata meta;
    
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path.toUtf8().constData(), nullptr, nullptr) < 0) {
        return meta;
    }
    
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return meta;
    }
    
    int videoIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoIdx < 0) {
            videoIdx = static_cast<int>(i);
        } else if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !meta.hasAudio) {
            meta.hasAudio = true;
        }
    }
    
    if (videoIdx >= 0) {
        auto* videoStream = fmtCtx->streams[videoIdx];
        meta.width = videoStream->codecpar->width;
        meta.height = videoStream->codecpar->height;
        meta.totalFrames = videoStream->nb_frames;
        meta.durationMs = fmtCtx->duration / 1000;
        
        if (videoStream->avg_frame_rate.num > 0 && videoStream->avg_frame_rate.den > 0) {
            meta.frameRate = av_q2d(videoStream->avg_frame_rate);
        }
        
        meta.aspectRatio = detectAspectRatio(meta.width, meta.height);
        
        const AVCodecDescriptor* codecDesc = avcodec_descriptor_get(videoStream->codecpar->codec_id);
        if (codecDesc) {
            meta.codecName = QString::fromUtf8(codecDesc->name);
        }
    }
    
    QFileInfo info(path);
    meta.format = detectVideoFormat(info.suffix());
    
    avformat_close_input(&fmtCtx);
    return meta;
}

QString VideoProcessorFactory::suggestOutputPath(const QString& inputPath, VideoFormat format)
{
    QFileInfo info(inputPath);
    QString baseName = info.completeBaseName();
    QString dir = info.absolutePath();
    QString ext = videoFormatExtension(format);
    
    return QString("%1/%2_enhanced.%3").arg(dir, baseName, ext);
}

} // namespace EnhanceVision
