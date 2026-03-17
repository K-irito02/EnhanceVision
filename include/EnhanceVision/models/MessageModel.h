/**
 * @file MessageModel.h
 * @brief 消息列表模型
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_MESSAGEMODEL_H
#define ENHANCEVISION_MESSAGEMODEL_H

#include <QAbstractListModel>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class MessageModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    /**
     * @brief 数据角色枚举
     */
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TimestampRole,
        ModeRole,
        MediaFilesRole,
        ParametersRole,
        ShaderParamsRole,
        StatusRole,
        ErrorMessageRole,
        IsSelectedRole,
        ProgressRole,
        QueuePositionRole,
        ModeTextRole,
        StatusTextRole,
        StatusColorRole
    };

    explicit MessageModel(QObject *parent = nullptr);
    ~MessageModel() override;

    // QAbstractListModel 接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 添加消息
     * @param message 消息对象
     * @return 消息ID
     */
    Q_INVOKABLE QString addMessage(const Message &message);

    /**
     * @brief 添加消息（简化版，用于QML）
     */
    Q_INVOKABLE QString addMessage(int mode, const QVariantList &mediaFiles, const QVariantMap &parameters);

    /**
     * @brief 更新消息状态
     * @param messageId 消息ID
     * @param status 新状态
     */
    Q_INVOKABLE void updateStatus(const QString &messageId, int status);

    /**
     * @brief 更新消息进度
     * @param messageId 消息ID
     * @param progress 新进度 (0-100)
     */
    Q_INVOKABLE void updateProgress(const QString &messageId, int progress);

    /**
     * @brief 更新消息错误信息
     * @param messageId 消息ID
     * @param errorMessage 错误信息
     */
    Q_INVOKABLE void updateErrorMessage(const QString &messageId, const QString &errorMessage);

    /**
     * @brief 删除消息
     * @param messageId 消息ID
     * @return 是否成功
     */
    Q_INVOKABLE bool removeMessage(const QString &messageId);

    /**
     * @brief 选择消息（批量操作）
     * @param messageId 消息ID
     * @param selected 是否选中
     */
    Q_INVOKABLE void selectMessage(const QString &messageId, bool selected);

    /**
     * @brief 取消所有选择
     */
    Q_INVOKABLE void clearSelection();

    /**
     * @brief 批量删除选中的消息
     * @return 删除的消息数量
     */
    Q_INVOKABLE int removeSelectedMessages();

    /**
     * @brief 清空所有消息
     */
    Q_INVOKABLE void clear();

    /**
     * @brief 将当前消息同步到会话
     * @param session 目标会话
     */
    void syncToSession(Session& session);

    /**
     * @brief 从会话加载消息
     * @param session 源会话
     */
    void loadFromSession(const Session& session);

    /**
     * @brief 更新消息中某个文件的处理状态
     * @param messageId 消息ID
     * @param fileId 文件ID
     * @param status 新状态
     * @param resultPath 处理结果路径
     */
    Q_INVOKABLE void updateFileStatus(const QString &messageId, const QString &fileId,
                                       int status, const QString &resultPath = QString());

    /**
     * @brief 更新消息队列位置
     * @param messageId 消息ID
     * @param position 队列位置
     */
    Q_INVOKABLE void updateQueuePosition(const QString &messageId, int position);

    /**
     * @brief 获取消息中已完成的文件数量
     * @param messageId 消息ID
     * @return 已完成文件数
     */
    Q_INVOKABLE int getCompletedFileCount(const QString &messageId) const;

    /**
     * @brief 获取消息中的总文件数量
     * @param messageId 消息ID
     * @return 总文件数
     */
    Q_INVOKABLE int getTotalFileCount(const QString &messageId) const;

    /**
     * @brief 从消息中删除指定索引的媒体文件
     * @param messageId 消息ID
     * @param fileIndex 文件索引
     * @return 是否成功
     */
    Q_INVOKABLE bool removeMediaFile(const QString &messageId, int fileIndex);

    /**
     * @brief 从消息中删除指定ID的媒体文件
     * @param messageId 消息ID
     * @param fileId 文件ID
     * @return 是否成功
     */
    Q_INVOKABLE bool removeMediaFileById(const QString &messageId, const QString &fileId);

    /**
     * @brief 获取消息中已完成文件的列表（供QML使用）
     * @param messageId 消息ID
     * @return 已完成文件的 QVariantList
     */
    Q_INVOKABLE QVariantList getCompletedFiles(const QString &messageId) const;

    /**
     * @brief 获取消息的所有媒体文件列表（供QML使用）
     * @param messageId 消息ID
     * @return 媒体文件的 QVariantList
     */
    Q_INVOKABLE QVariantList getMediaFiles(const QString &messageId) const;

    /**
     * @brief 获取消息的 Shader 参数（供QML使用）
     * @param messageId 消息ID
     * @return Shader 参数的 QVariantMap
     */
    Q_INVOKABLE QVariantMap getShaderParams(const QString &messageId) const;

    /**
     * @brief 获取消息列表
     * @return 消息列表
     */
    QList<Message> messages() const { return m_messages; }
    
    /**
     * @brief 设置消息列表（用于会话切换时加载）
     * @param messages 消息列表
     */
    void setMessages(const QList<Message>& messages);
    
    /**
     * @brief 获取当前会话ID
     * @return 会话ID
     */
    QString currentSessionId() const { return m_currentSessionId; }
    
    /**
     * @brief 设置当前会话ID
     * @param sessionId 会话ID
     */
    void setCurrentSessionId(const QString& sessionId);

    /**
     * @brief 获取指定ID的消息
     * @param messageId 消息ID
     * @return 消息对象
     */
    Message messageById(const QString &messageId) const;

signals:
    /**
     * @brief 消息数量变化信号
     */
    void countChanged();

    /**
     * @brief 消息添加信号
     * @param messageId 消息ID
     */
    void messageAdded(const QString &messageId);

    /**
     * @brief 消息删除信号
     * @param messageId 消息ID
     */
    void messageRemoved(const QString &messageId);

    /**
     * @brief 消息中的媒体文件被删除
     * @param messageId 消息ID
     * @param fileIndex 被删除的文件索引
     */
    void mediaFileRemoved(const QString &messageId, int fileIndex);

    /**
     * @brief 消息状态更新信号
     * @param messageId 消息ID
     * @param status 新状态
     */
    void statusUpdated(const QString &messageId, int status);

    /**
     * @brief 错误信号
     * @param message 错误信息
     */
    void errorOccurred(const QString &message);

private:
    /**
     * @brief 生成唯一ID
     * @return 唯一ID
     */
    QString generateId() const;

    /**
     * @brief 根据ID查找消息索引
     * @param messageId 消息ID
     * @return 索引，-1表示未找到
     */
    int findMessageIndex(const QString &messageId) const;

    /**
     * @brief 获取状态文本
     * @param status 状态枚举
     * @return 状态文本
     */
    QString getStatusText(ProcessingStatus status) const;

    /**
     * @brief 获取状态颜色
     * @param status 状态枚举
     * @return 颜色字符串
     */
    QString getStatusColor(ProcessingStatus status) const;

    /**
     * @brief 获取模式文本
     * @param mode 模式枚举
     * @return 模式文本
     */
    QString getModeText(ProcessingMode mode) const;

    QList<Message> m_messages;  ///< 消息列表
    QString m_currentSessionId; ///< 当前会话ID
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_MESSAGEMODEL_H
