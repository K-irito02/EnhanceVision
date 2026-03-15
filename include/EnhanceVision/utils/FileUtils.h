/**
 * @file FileUtils.h
 * @brief 文件工具类
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_FILEUTILS_H
#define ENHANCEVISION_FILEUTILS_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace EnhanceVision {

class FileUtils : public QObject
{
    Q_OBJECT

public:
    explicit FileUtils(QObject *parent = nullptr);
    ~FileUtils() override;

    /**
     * @brief 复制文件到目标目录
     * @param sourcePath 源文件路径
     * @param targetDir 目标目录
     * @param overwrite 是否覆盖已有文件
     * @return 目标文件完整路径，失败返回空字符串
     */
    static QString copyFileTo(const QString &sourcePath, const QString &targetDir, bool overwrite = false);

    /**
     * @brief 批量复制文件到目标目录
     * @param sourcePaths 源文件路径列表
     * @param targetDir 目标目录
     * @return 成功复制的文件数
     */
    static int copyFilesTo(const QStringList &sourcePaths, const QString &targetDir);

    /**
     * @brief 确保目录存在（不存在则创建）
     * @param dirPath 目录路径
     * @return 是否成功
     */
    static bool ensureDirectory(const QString &dirPath);

    /**
     * @brief 生成不重复的文件名（自动追加 _1, _2 等后缀）
     * @param dirPath 目标目录
     * @param fileName 原始文件名
     * @return 不重复的文件名
     */
    static QString generateUniqueFileName(const QString &dirPath, const QString &fileName);

    /**
     * @brief 获取默认保存路径（用户文档/EnhanceVision/output）
     * @return 默认路径
     */
    static QString getDefaultSavePath();

    /**
     * @brief 在系统文件管理器中打开并选中文件
     * @param filePath 文件路径
     */
    static void openInExplorer(const QString &filePath);

    /**
     * @brief 获取人类可读的文件大小
     * @param bytes 字节数
     * @return 格式化字符串 (如 "12.5 MB")
     */
    static QString humanReadableSize(qint64 bytes);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_FILEUTILS_H
