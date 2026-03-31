/**
 * @file AutoSaveService.h
 * @brief 自动保存服务 - 处理完成后自动保存结果文件到默认导出路径
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_AUTOSAVESERVICE_H
#define ENHANCEVISION_AUTOSAVESERVICE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QMutex>

namespace EnhanceVision {

/**
 * @brief 自动保存服务
 * 
 * 当任务处理完成后，检查设置中的 autoSaveResult 选项，
 * 如果开启则自动将结果文件复制到 defaultSavePath 目录。
 */
class AutoSaveService : public QObject
{
    Q_OBJECT

public:
    static AutoSaveService* instance();
    
    /**
     * @brief 初始化服务
     */
    void initialize();

    /**
     * @brief 自动保存结果文件
     * @param taskId 任务ID
     * @param resultPath 结果文件路径
     */
    Q_INVOKABLE void autoSaveResult(const QString& taskId, const QString& resultPath);
    
    /**
     * @brief 检查自动保存是否启用
     * @return 是否启用自动保存
     */
    Q_INVOKABLE bool isAutoSaveEnabled() const;
    
    /**
     * @brief 获取默认保存路径
     * @return 默认保存路径
     */
    Q_INVOKABLE QString getDefaultSavePath() const;

signals:
    /**
     * @brief 自动保存完成信号
     * @param taskId 任务ID
     * @param success 是否成功
     * @param savedPath 保存后的文件路径（成功时有效）
     * @param error 错误信息（失败时有效）
     */
    void autoSaveCompleted(const QString& taskId, bool success, const QString& savedPath, const QString& error);

private:
    explicit AutoSaveService(QObject* parent = nullptr);
    ~AutoSaveService() override = default;
    
    /**
     * @brief 生成唯一的文件路径（处理文件名冲突）
     * @param targetDir 目标目录
     * @param originalPath 原始文件路径
     * @return 唯一的目标文件路径
     */
    QString generateUniquePath(const QString& targetDir, const QString& originalPath);
    
    /**
     * @brief 确保目录存在
     * @param path 目录路径
     * @return 是否成功创建或目录已存在
     */
    bool ensureDirectoryExists(const QString& path);

    static AutoSaveService* s_instance;
    QMutex m_mutex;
    QHash<QString, bool> m_pendingSaves;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AUTOSAVESERVICE_H
