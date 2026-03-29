/**
 * @file VideoFrameBuffer.cpp
 * @brief 视频帧缓冲池实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/VideoFrameBuffer.h"
#include <QMutexLocker>

namespace EnhanceVision {

VideoFrameBuffer::VideoFrameBuffer(int maxSlots, QObject* parent)
    : QObject(parent)
    , m_maxSlots(maxSlots)
    , m_stopped(false)
{
}

VideoFrameBuffer::~VideoFrameBuffer()
{
    clear();
}

bool VideoFrameBuffer::pushFrame(const QImage& frame, int64_t pts, int frameIndex)
{
    QMutexLocker locker(&m_mutex);
    
    while (m_buffer.size() >= m_maxSlots && !m_stopped) {
        m_notFull.wait(&m_mutex);
    }
    
    if (m_stopped) {
        return false;
    }
    
    FrameBufferSlot slot;
    slot.frame = frame;
    slot.pts = pts;
    slot.frameIndex = frameIndex;
    slot.isProcessed = false;
    
    m_buffer.enqueue(slot);
    m_notEmpty.wakeOne();
    
    emit frameAvailable();
    
    return true;
}

bool VideoFrameBuffer::popFrame(FrameBufferSlot& slot)
{
    QMutexLocker locker(&m_mutex);
    
    while (m_buffer.isEmpty() && !m_stopped) {
        m_notEmpty.wait(&m_mutex);
    }
    
    if (m_buffer.isEmpty()) {
        return false;
    }
    
    slot = m_buffer.dequeue();
    m_notFull.wakeOne();
    
    return true;
}

bool VideoFrameBuffer::tryPushFrame(const QImage& frame, int64_t pts, int frameIndex)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_buffer.size() >= m_maxSlots) {
        return false;
    }
    
    FrameBufferSlot slot;
    slot.frame = frame;
    slot.pts = pts;
    slot.frameIndex = frameIndex;
    slot.isProcessed = false;
    
    m_buffer.enqueue(slot);
    m_notEmpty.wakeOne();
    
    emit frameAvailable();
    
    return true;
}

bool VideoFrameBuffer::tryPopFrame(FrameBufferSlot& slot)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_buffer.isEmpty()) {
        return false;
    }
    
    slot = m_buffer.dequeue();
    m_notFull.wakeOne();
    
    return true;
}

void VideoFrameBuffer::setMaxSlots(int maxSlots)
{
    QMutexLocker locker(&m_mutex);
    m_maxSlots = maxSlots;
    m_notFull.wakeAll();
}

int VideoFrameBuffer::maxSlots() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxSlots;
}

int VideoFrameBuffer::usedSlots() const
{
    QMutexLocker locker(&m_mutex);
    return m_buffer.size();
}

void VideoFrameBuffer::clear()
{
    QMutexLocker locker(&m_mutex);
    m_stopped = true;
    m_buffer.clear();
    m_notFull.wakeAll();
    m_notEmpty.wakeAll();
    emit bufferCleared();
}

bool VideoFrameBuffer::isEmpty() const
{
    QMutexLocker locker(&m_mutex);
    return m_buffer.isEmpty();
}

bool VideoFrameBuffer::isFull() const
{
    QMutexLocker locker(&m_mutex);
    return m_buffer.size() >= m_maxSlots;
}

} // namespace EnhanceVision
