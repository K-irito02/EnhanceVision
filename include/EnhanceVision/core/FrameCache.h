/**
 * @file FrameCache.h
 * @brief 帧缓存管理器
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_FRAMECACHE_H
#define ENHANCEVISION_FRAMECACHE_H

#include <QObject>

namespace EnhanceVision {

class FrameCache : public QObject
{
    Q_OBJECT

public:
    explicit FrameCache(QObject *parent = nullptr);
    ~FrameCache() override;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_FRAMECACHE_H
