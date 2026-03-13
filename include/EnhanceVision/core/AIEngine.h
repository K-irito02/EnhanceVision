/**
 * @file AIEngine.h
 * @brief NCNN AI 推理引擎
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_AIENGINE_H
#define ENHANCEVISION_AIENGINE_H

#include <QObject>

namespace EnhanceVision {

class AIEngine : public QObject
{
    Q_OBJECT

public:
    explicit AIEngine(QObject *parent = nullptr);
    ~AIEngine() override;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIENGINE_H
