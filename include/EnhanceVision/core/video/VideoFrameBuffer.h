/**
 * @file VideoFrameBuffer.h
 * @brief 视频帧缓冲池
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEOFRAMEBUFFER_H
#define ENHANCEVISION_VIDEOFRAMEBUFFER_H

#include <QObject>
#include <QImage>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

namespace EnhanceVision {

struct FrameBufferSlot {
    QImage frame;
    int64_t pts = 0;
    int frameIndex = 0;
    bool isProcessed = false;
};

class VideoFrameBuffer : public QObject {
    Q_OBJECT
    
public:
    explicit VideoFrameBuffer(int maxSlots = 4, QObject* parent = nullptr);
    ~VideoFrameBuffer() override;
    
    bool pushFrame(const QImage& frame, int64_t pts, int frameIndex);
    bool popFrame(FrameBufferSlot& slot);
    
    bool tryPushFrame(const QImage& frame, int64_t pts, int frameIndex);
    bool tryPopFrame(FrameBufferSlot& slot);
    
    void setMaxSlots(int maxSlots);
    int maxSlots() const;
    int usedSlots() const;
    
    void clear();
    bool isEmpty() const;
    bool isFull() const;
    
signals:
    void frameAvailable();
    void bufferCleared();
    void bufferFull();
    
private:
    mutable QMutex m_mutex;
    QWaitCondition m_notFull;
    QWaitCondition m_notEmpty;
    QQueue<FrameBufferSlot> m_buffer;
    int m_maxSlots;
    bool m_stopped;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOFRAMEBUFFER_H
