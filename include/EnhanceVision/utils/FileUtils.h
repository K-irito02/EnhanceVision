/**
 * @file FileUtils.h
 * @brief 文件工具类
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_FILEUTILS_H
#define ENHANCEVISION_FILEUTILS_H

#include <QObject>

namespace EnhanceVision {

class FileUtils : public QObject
{
    Q_OBJECT

public:
    explicit FileUtils(QObject *parent = nullptr);
    ~FileUtils() override;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_FILEUTILS_H
