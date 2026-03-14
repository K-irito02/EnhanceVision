/**
 * @file SessionModel.h
 * @brief 会话列表模型
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_SESSIONMODEL_H
#define ENHANCEVISION_SESSIONMODEL_H

#include <QAbstractListModel>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class SessionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString activeSessionId READ activeSessionId NOTIFY activeSessionChanged)

public:
    /**
     * @brief 数据角色枚举
     */
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CreatedAtRole,
        ModifiedAtRole,
        IsActiveRole,
        IsSelectedRole,
        MessageCountRole,
        IsPinnedRole,
        SortIndexRole
    };

    explicit SessionModel(QObject *parent = nullptr);
    ~SessionModel() override;

    // QAbstractListModel 接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 创建新会话
     * @param name 会话名称（可选）
     * @return 新会话的ID
     */
    Q_INVOKABLE QString createSession(const QString &name = QString());

    /**
     * @brief 切换到指定会话
     * @param sessionId 会话ID
     */
    Q_INVOKABLE void switchSession(const QString &sessionId);

    /**
     * @brief 重命名会话
     * @param sessionId 会话ID
     * @param newName 新名称
     * @return 是否成功
     */
    Q_INVOKABLE bool renameSession(const QString &sessionId, const QString &newName);

    /**
     * @brief 删除会话
     * @param sessionId 会话ID
     * @return 是否成功
     */
    Q_INVOKABLE bool deleteSession(const QString &sessionId);

    /**
     * @brief 清空会话内容
     * @param sessionId 会话ID
     */
    Q_INVOKABLE void clearSession(const QString &sessionId);

    /**
     * @brief 选择会话（批量操作）
     * @param sessionId 会话ID
     * @param selected 是否选中
     */
    Q_INVOKABLE void selectSession(const QString &sessionId, bool selected);

    /**
     * @brief 取消所有选择
     */
    Q_INVOKABLE void clearSelection();

    /**
     * @brief 批量删除选中的会话
     * @return 删除的会话数量
     */
    Q_INVOKABLE int deleteSelectedSessions();

    /**
     * @brief 批量清空选中的会话
     * @return 清空的会话数量
     */
    Q_INVOKABLE int clearSelectedSessions();

    /**
     * @brief 置顶/取消置顶会话
     * @param sessionId 会话ID
     * @param pinned 是否置顶
     */
    Q_INVOKABLE void pinSession(const QString &sessionId, bool pinned);

    /**
     * @brief 移动会话位置（拖拽排序）
     * @param fromIndex 源索引
     * @param toIndex 目标索引
     */
    Q_INVOKABLE void moveSession(int fromIndex, int toIndex);

    /**
     * @brief 全选会话
     */
    Q_INVOKABLE void selectAll();

    /**
     * @brief 获取选中的会话数量
     * @return 选中数量
     */
    Q_INVOKABLE int selectedCount() const;

    /**
     * @brief 获取当前活动会话ID
     * @return 活动会话ID
     */
    QString activeSessionId() const { return m_activeSessionId; }

    /**
     * @brief 获取会话列表
     * @return 会话列表
     */
    QList<Session> sessions() const { return m_sessions; }

    /**
     * @brief 获取指定ID的会话
     * @param sessionId 会话ID
     * @return 会话对象
     */
    Session sessionById(const QString &sessionId) const;

    /**
     * @brief 添加会话（外部调用）
     * @param session 会话对象
     */
    void addSession(const Session &session);

    /**
     * @brief 更新会话列表
     * @param sessions 新的会话列表
     */
    void updateSessions(const QList<Session> &sessions);

    /**
     * @brief 更新单个会话
     * @param session 会话对象
     */
    void updateSession(const Session &session);

    /**
     * @brief 移除会话
     * @param sessionId 会话ID
     */
    void removeSession(const QString &sessionId);

signals:
    /**
     * @brief 会话数量变化信号
     */
    void countChanged();

    /**
     * @brief 活动会话变化信号
     */
    void activeSessionChanged();

    /**
     * @brief 会话创建信号
     * @param sessionId 会话ID
     */
    void sessionCreated(const QString &sessionId);

    /**
     * @brief 会话删除信号
     * @param sessionId 会话ID
     */
    void sessionDeleted(const QString &sessionId);

    /**
     * @brief 会话重命名信号
     * @param sessionId 会话ID
     * @param newName 新名称
     */
    void sessionRenamed(const QString &sessionId, const QString &newName);

    /**
     * @brief 会话置顶状态变化信号
     * @param sessionId 会话ID
     * @param pinned 是否置顶
     */
    void sessionPinned(const QString &sessionId, bool pinned);

    /**
     * @brief 会话清空信号
     * @param sessionId 会话ID
     */
    void sessionCleared(const QString &sessionId);

    /**
     * @brief 会话位置变化信号
     * @param fromIndex 源索引
     * @param toIndex 目标索引
     */
    void sessionMoved(int fromIndex, int toIndex);

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
     * @brief 生成默认会话名称
     * @return 默认名称
     */
    QString generateDefaultName() const;

    /**
     * @brief 根据ID查找会话索引
     * @param sessionId 会话ID
     * @return 索引，-1表示未找到
     */
    int findSessionIndex(const QString &sessionId) const;

    /**
     * @brief 重新排序会话列表（置顶在前）
     */
    void sortSessions();

    /**
     * @brief 更新排序索引
     */
    void updateSortIndices();

    QList<Session> m_sessions;       ///< 会话列表
    QString m_activeSessionId;        ///< 当前活动会话ID
    int m_sessionCounter;             ///< 会话计数器（用于生成默认名称）
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SESSIONMODEL_H
