/**
 * @file VideoResourceGuard.h
 * @brief FFmpeg 资源 RAII 守护类
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEORESOURCEGUARD_H
#define ENHANCEVISION_VIDEORESOURCEGUARD_H

#include "VideoProcessingTypes.h"
#include <memory>
#include <atomic>

struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct AVFrame;
struct AVPacket;

namespace EnhanceVision {

class VideoResourceGuard {
public:
    VideoResourceGuard();
    ~VideoResourceGuard();
    
    VideoResourceGuard(const VideoResourceGuard&) = delete;
    VideoResourceGuard& operator=(const VideoResourceGuard&) = delete;
    
    VideoResourceGuard(VideoResourceGuard&& other) noexcept;
    VideoResourceGuard& operator=(VideoResourceGuard&& other) noexcept;
    
    bool initializeInput(const QString& path);
    bool initializeOutput(const QString& path, const VideoMetadata& meta);
    bool initializeEncoder(const VideoMetadata& meta, int outputWidth, int outputHeight);
    
    AVFormatContext* inputFormatContext() const;
    AVFormatContext* outputFormatContext() const;
    AVCodecContext* decoderContext() const;
    AVCodecContext* encoderContext() const;
    SwsContext* decoderSwsContext() const;
    SwsContext* encoderSwsContext() const;
    
    int videoStreamIndex() const;
    int audioStreamIndex() const;
    int outputVideoStreamIndex() const;
    int outputAudioStreamIndex() const;
    
    bool isValid() const;
    QString lastError() const;
    
    void setCancelled(bool cancelled);
    bool isCancelled() const;
    
    bool readFrame(AVPacket* packet);
    bool decodeFrame(AVFrame* frame);
    bool encodeFrame(AVFrame* frame, AVPacket* packet);
    bool writeFrame(AVPacket* packet);
    bool writeTrailer();
    
    void updateDecoderSwsContext(int width, int height, int pixelFormat);
    void updateEncoderSwsContext(int width, int height);
    
    int64_t totalFrames() const;
    double frameRate() const;
    
private:
    void cleanup();
    bool openInputFile(const QString& path);
    bool findStreamInfo();
    bool openDecoder();
    bool setupOutputFile(const QString& path);
    bool openEncoder(int width, int height);
    bool copyAudioStream();
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEORESOURCEGUARD_H
