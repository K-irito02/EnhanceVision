/**
 * @file VideoResourceGuard.cpp
 * @brief FFmpeg 资源 RAII 守护类实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/VideoResourceGuard.h"
#include <QDebug>
#include <QFileInfo>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#ifndef AVFMT_GLOBAL_HEADER
#define AVFMT_GLOBAL_HEADER AVFMT_GLOBALHEADER
#endif

namespace EnhanceVision {

struct VideoResourceGuard::Impl {
    AVFormatContext* inFmtCtx = nullptr;
    AVFormatContext* outFmtCtx = nullptr;
    AVCodecContext* decCtx = nullptr;
    AVCodecContext* encCtx = nullptr;
    SwsContext* decSwsCtx = nullptr;
    SwsContext* encSwsCtx = nullptr;
    
    int videoIdx = -1;
    int audioIdx = -1;
    int outVideoIdx = -1;
    int outAudioIdx = -1;
    
    bool headerWritten = false;
    bool encoderOpened = false;
    QString lastError;
    std::atomic<bool> cancelled{false};
    
    int64_t totalFrames = 0;
    double frameRate = 0.0;
    
    int decSwsW = 0;
    int decSwsH = 0;
    int decSwsFmt = -1;
    
    void cleanup() {
        if (decSwsCtx) {
            sws_freeContext(decSwsCtx);
            decSwsCtx = nullptr;
        }
        if (encSwsCtx) {
            sws_freeContext(encSwsCtx);
            encSwsCtx = nullptr;
        }
        if (encCtx) {
            avcodec_free_context(&encCtx);
            encCtx = nullptr;
        }
        if (decCtx) {
            avcodec_free_context(&decCtx);
            decCtx = nullptr;
        }
        if (outFmtCtx) {
            if (headerWritten && !(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&outFmtCtx->pb);
            }
            avformat_free_context(outFmtCtx);
            outFmtCtx = nullptr;
        }
        if (inFmtCtx) {
            avformat_close_input(&inFmtCtx);
            inFmtCtx = nullptr;
        }
        headerWritten = false;
        encoderOpened = false;
        videoIdx = -1;
        audioIdx = -1;
        outVideoIdx = -1;
        outAudioIdx = -1;
    }
};

VideoResourceGuard::VideoResourceGuard()
    : m_impl(std::make_unique<Impl>())
{
}

VideoResourceGuard::~VideoResourceGuard()
{
    m_impl->cleanup();
}

VideoResourceGuard::VideoResourceGuard(VideoResourceGuard&& other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

VideoResourceGuard& VideoResourceGuard::operator=(VideoResourceGuard&& other) noexcept
{
    if (this != &other) {
        m_impl->cleanup();
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

bool VideoResourceGuard::initializeInput(const QString& path)
{
    if (!openInputFile(path)) return false;
    if (!findStreamInfo()) return false;
    if (!openDecoder()) return false;
    return true;
}

bool VideoResourceGuard::openInputFile(const QString& path)
{
    if (avformat_open_input(&m_impl->inFmtCtx, path.toUtf8().constData(), nullptr, nullptr) < 0) {
        m_impl->lastError = QObject::tr("无法打开视频文件: %1").arg(path);
        return false;
    }
    return true;
}

bool VideoResourceGuard::findStreamInfo()
{
    if (avformat_find_stream_info(m_impl->inFmtCtx, nullptr) < 0) {
        m_impl->lastError = QObject::tr("无法获取视频流信息");
        return false;
    }
    
    for (unsigned int i = 0; i < m_impl->inFmtCtx->nb_streams; ++i) {
        if (m_impl->inFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_impl->videoIdx < 0) {
            m_impl->videoIdx = static_cast<int>(i);
        } else if (m_impl->inFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_impl->audioIdx < 0) {
            m_impl->audioIdx = static_cast<int>(i);
        }
    }
    
    if (m_impl->videoIdx < 0) {
        m_impl->lastError = QObject::tr("未找到视频流");
        return false;
    }
    
    auto* videoStream = m_impl->inFmtCtx->streams[m_impl->videoIdx];
    m_impl->totalFrames = videoStream->nb_frames;
    if (m_impl->totalFrames <= 0 && m_impl->decCtx && m_impl->decCtx->framerate.num > 0 && m_impl->inFmtCtx->duration > 0) {
        m_impl->totalFrames = m_impl->inFmtCtx->duration * m_impl->decCtx->framerate.num
                            / (static_cast<int64_t>(AV_TIME_BASE) * std::max(1, m_impl->decCtx->framerate.den));
    }
    if (m_impl->totalFrames <= 0) m_impl->totalFrames = 1000;
    
    if (m_impl->decCtx) {
        m_impl->frameRate = av_q2d(m_impl->decCtx->framerate);
    }
    
    return true;
}

bool VideoResourceGuard::openDecoder()
{
    if (m_impl->videoIdx < 0) return false;
    
    const AVCodec* dec = avcodec_find_decoder(m_impl->inFmtCtx->streams[m_impl->videoIdx]->codecpar->codec_id);
    if (!dec) {
        m_impl->lastError = QObject::tr("未找到合适的视频解码器");
        return false;
    }
    
    m_impl->decCtx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(m_impl->decCtx, m_impl->inFmtCtx->streams[m_impl->videoIdx]->codecpar);
    
    if (avcodec_open2(m_impl->decCtx, dec, nullptr) < 0) {
        m_impl->lastError = QObject::tr("无法打开视频解码器");
        return false;
    }
    
    m_impl->frameRate = av_q2d(m_impl->decCtx->framerate);
    
    qInfo() << "[VideoResourceGuard] decoder opened:" << dec->name
            << "size:" << m_impl->decCtx->width << "x" << m_impl->decCtx->height
            << "fps:" << m_impl->frameRate;
    
    return true;
}

bool VideoResourceGuard::initializeOutput(const QString& path, const VideoMetadata& meta)
{
    return setupOutputFile(path);
}

bool VideoResourceGuard::setupOutputFile(const QString& path)
{
    int ret = avformat_alloc_output_context2(&m_impl->outFmtCtx, nullptr, "mp4", path.toUtf8().constData());
    if (ret < 0 || !m_impl->outFmtCtx) {
        m_impl->lastError = QObject::tr("无法创建输出格式上下文");
        return false;
    }
    return true;
}

bool VideoResourceGuard::initializeEncoder(const VideoMetadata& meta, int outputWidth, int outputHeight)
{
    return openEncoder(outputWidth, outputHeight);
}

bool VideoResourceGuard::openEncoder(int width, int height)
{
    if (!m_impl->outFmtCtx) return false;
    
    const AVCodec* enc = nullptr;
    QString selectedEncoderName;
    
    const char* encoderPriority[] = {
        "h264_nvenc",
        "libx264",
        "h264_qsv",
        "h264_amf",
        "libopenh264",
        nullptr
    };
    
    for (int i = 0; encoderPriority[i] && !enc; ++i) {
        const AVCodec* candidate = avcodec_find_encoder_by_name(encoderPriority[i]);
        if (candidate) {
            enc = candidate;
            selectedEncoderName = QString(enc->name);
            qInfo() << "[VideoResourceGuard] selected H.264 encoder:" << enc->name;
        }
    }
    
    if (!enc) {
        enc = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
        if (enc) {
            selectedEncoderName = QString(enc->name);
            qInfo() << "[VideoResourceGuard] using MPEG-4 fallback encoder:" << enc->name;
        }
    }
    
    if (!enc) {
        m_impl->lastError = QObject::tr("未找到可用的视频编码器");
        return false;
    }
    
    AVStream* outVStream = avformat_new_stream(m_impl->outFmtCtx, nullptr);
    m_impl->outVideoIdx = outVStream->index;
    
    m_impl->encCtx = avcodec_alloc_context3(enc);
    m_impl->encCtx->width = width & ~1;
    m_impl->encCtx->height = height & ~1;
    m_impl->encCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_impl->encCtx->time_base = m_impl->inFmtCtx->streams[m_impl->videoIdx]->time_base;
    m_impl->encCtx->framerate = m_impl->decCtx->framerate;
    m_impl->encCtx->bit_rate = static_cast<int64_t>(width) * height * 4;
    
    if (m_impl->outFmtCtx->oformat->flags & AVFMT_GLOBAL_HEADER) {
        m_impl->encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    AVDictionary* encOpts = nullptr;
    if (selectedEncoderName.contains("nvenc")) {
        av_dict_set(&encOpts, "preset", "p4", 0);
        av_dict_set(&encOpts, "rc", "vbr", 0);
        av_dict_set(&encOpts, "cq", "23", 0);
    } else if (selectedEncoderName.contains("libx264")) {
        av_dict_set(&encOpts, "preset", "medium", 0);
        av_dict_set(&encOpts, "crf", "18", 0);
    } else {
        av_dict_set(&encOpts, "q:v", "5", 0);
    }
    
    int ret = avcodec_open2(m_impl->encCtx, enc, &encOpts);
    av_dict_free(&encOpts);
    
    if (ret < 0) {
        m_impl->lastError = QObject::tr("无法打开视频编码器");
        return false;
    }
    
    m_impl->encoderOpened = true;
    avcodec_parameters_from_context(outVStream->codecpar, m_impl->encCtx);
    outVStream->time_base = m_impl->encCtx->time_base;
    
    if (m_impl->audioIdx >= 0) {
        AVStream* outAStream = avformat_new_stream(m_impl->outFmtCtx, nullptr);
        m_impl->outAudioIdx = outAStream->index;
        avcodec_parameters_copy(outAStream->codecpar, m_impl->inFmtCtx->streams[m_impl->audioIdx]->codecpar);
        outAStream->time_base = m_impl->inFmtCtx->streams[m_impl->audioIdx]->time_base;
    }
    
    if (!(m_impl->outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        QString outputPath = QString::fromUtf8(m_impl->outFmtCtx->url);
        ret = avio_open(&m_impl->outFmtCtx->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            m_impl->lastError = QObject::tr("无法打开输出文件: %1").arg(outputPath);
            return false;
        }
    }
    
    ret = avformat_write_header(m_impl->outFmtCtx, nullptr);
    if (ret < 0) {
        m_impl->lastError = QObject::tr("无法写入视频文件头");
        return false;
    }
    m_impl->headerWritten = true;
    
    m_impl->encSwsCtx = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    qInfo() << "[VideoResourceGuard] encoder initialized"
            << "outputSize:" << m_impl->encCtx->width << "x" << m_impl->encCtx->height
            << "codec:" << enc->name;
    
    return true;
}

AVFormatContext* VideoResourceGuard::inputFormatContext() const
{
    return m_impl->inFmtCtx;
}

AVFormatContext* VideoResourceGuard::outputFormatContext() const
{
    return m_impl->outFmtCtx;
}

AVCodecContext* VideoResourceGuard::decoderContext() const
{
    return m_impl->decCtx;
}

AVCodecContext* VideoResourceGuard::encoderContext() const
{
    return m_impl->encCtx;
}

SwsContext* VideoResourceGuard::decoderSwsContext() const
{
    return m_impl->decSwsCtx;
}

SwsContext* VideoResourceGuard::encoderSwsContext() const
{
    return m_impl->encSwsCtx;
}

int VideoResourceGuard::videoStreamIndex() const
{
    return m_impl->videoIdx;
}

int VideoResourceGuard::audioStreamIndex() const
{
    return m_impl->audioIdx;
}

int VideoResourceGuard::outputVideoStreamIndex() const
{
    return m_impl->outVideoIdx;
}

int VideoResourceGuard::outputAudioStreamIndex() const
{
    return m_impl->outAudioIdx;
}

bool VideoResourceGuard::isValid() const
{
    return m_impl->inFmtCtx != nullptr && m_impl->decCtx != nullptr;
}

QString VideoResourceGuard::lastError() const
{
    return m_impl->lastError;
}

void VideoResourceGuard::setCancelled(bool cancelled)
{
    m_impl->cancelled.store(cancelled);
}

bool VideoResourceGuard::isCancelled() const
{
    return m_impl->cancelled.load();
}

bool VideoResourceGuard::readFrame(AVPacket* packet)
{
    return av_read_frame(m_impl->inFmtCtx, packet) >= 0;
}

bool VideoResourceGuard::decodeFrame(AVFrame* frame)
{
    return avcodec_receive_frame(m_impl->decCtx, frame) == 0;
}

bool VideoResourceGuard::encodeFrame(AVFrame* frame, AVPacket* packet)
{
    int ret = avcodec_send_frame(m_impl->encCtx, frame);
    if (ret < 0) return false;
    
    ret = avcodec_receive_packet(m_impl->encCtx, packet);
    return ret == 0;
}

bool VideoResourceGuard::writeFrame(AVPacket* packet)
{
    av_packet_rescale_ts(packet, m_impl->encCtx->time_base,
        m_impl->outFmtCtx->streams[m_impl->outVideoIdx]->time_base);
    packet->stream_index = m_impl->outVideoIdx;
    return av_interleaved_write_frame(m_impl->outFmtCtx, packet) >= 0;
}

bool VideoResourceGuard::writeTrailer()
{
    if (m_impl->headerWritten && m_impl->outFmtCtx) {
        return av_write_trailer(m_impl->outFmtCtx) >= 0;
    }
    return true;
}

void VideoResourceGuard::updateDecoderSwsContext(int width, int height, int pixelFormat)
{
    if (!m_impl->decSwsCtx || m_impl->decSwsW != width || m_impl->decSwsH != height || m_impl->decSwsFmt != pixelFormat) {
        if (m_impl->decSwsCtx) {
            sws_freeContext(m_impl->decSwsCtx);
        }
        m_impl->decSwsCtx = sws_getContext(
            width, height, static_cast<AVPixelFormat>(pixelFormat),
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_impl->decSwsW = width;
        m_impl->decSwsH = height;
        m_impl->decSwsFmt = pixelFormat;
    }
}

void VideoResourceGuard::updateEncoderSwsContext(int width, int height)
{
    if (m_impl->encSwsCtx) {
        sws_freeContext(m_impl->encSwsCtx);
    }
    m_impl->encSwsCtx = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
}

int64_t VideoResourceGuard::totalFrames() const
{
    return m_impl->totalFrames;
}

double VideoResourceGuard::frameRate() const
{
    return m_impl->frameRate;
}

void VideoResourceGuard::cleanup()
{
    m_impl->cleanup();
}

} // namespace EnhanceVision
