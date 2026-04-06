/**
 * @file ProcessingTimeManager.h
 * @brief 处理任务时间管理器 - 后台管理所有处理中任务的倒计时
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PROCESSINGTIMEMANAGER_H
#define ENHANCEVISION_PROCESSINGTIMEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QDateTime>

namespace EnhanceVision {

class MessageModel;

/**
 * @brief 任务时间信息结构
 */
struct TaskTimeInfo {
    QString messageId;           ///< 消息ID
    qint64 processingStartTime;  ///< 处理开始时间戳（毫秒）
    qint64 predictedTotalSec;    ///< 预测总时间（秒）
    qint64 elapsedSec;           ///< 已用时间（秒）
    qint64 remainingSec;         ///< 剩余时间（秒）
    bool isOvertime;             ///< 是否超时

    TaskTimeInfo()
        : processingStartTime(0)
        , predictedTotalSec(0)
        , elapsedSec(0)
        , remainingSec(0)
        , isOvertime(false)
    {}
};

/**
 * @brief 处理任务时间管理器
 * 
 * 在后台管理所有处理中任务的倒计时，使用 QTimer 每秒更新一次。
 * 不受 QML delegate 生命周期影响，切换会话后继续运行。
 */
class ProcessingTimeManager : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingTimeManager(QObject *parent = nullptr);
    ~ProcessingTimeManager() override;

    /**
     * @brief 设置消息模型引用
     * @param model 消息模型
     */
    void setMessageModel(MessageModel* model);

    /**
     * @brief 注册任务开始处理
     * @param messageId 消息ID
     * @param predictedTotalSec 预测总时间（秒）
     * @param processingStartTime 处理开始时间戳（毫秒），0表示使用当前时间
     */
    void registerTask(const QString& messageId, 
                      qint64 predictedTotalSec,
                      qint64 processingStartTime = 0);

    /**
     * @brief 注销任务（完成/取消/失败）
     * @param messageId 消息ID
     */
    void unregisterTask(const QString& messageId);

    /**
     * @brief 获取任务时间信息
     * @param messageId 消息ID
     * @return 任务时间信息
     */
    TaskTimeInfo getTaskTimeInfo(const QString& messageId) const;

    /**
     * @brief 检查任务是否已注册
     * @param messageId 消息ID
     * @return 是否已注册
     */
    bool isTaskRegistered(const QString& messageId) const;

    /**
     * @brief 获取所有注册的任务ID
     * @return 任务ID列表
     */
    QList<QString> registeredTaskIds() const;

    /**
     * @brief 清空所有任务
     */
    void clear();

    /**
     * @brief 暂停任务计时
     * @param messageId 消息ID
     */
    void pauseTask(const QString& messageId);

    /**
     * @brief 恢复任务计时
     * @param messageId 消息ID
     */
    void resumeTask(const QString& messageId);

    /**
     * @brief 检查任务是否暂停
     * @param messageId 消息ID
     * @return 是否暂停
     */
    bool isTaskPaused(const QString& messageId) const;

signals:
    /**
     * @brief 时间信息更新信号（每秒发出）
     * @param messageId 消息ID
     * @param elapsedSec 已用时间（秒）
     * @param remainingSec 剩余时间（秒）
     * @param isOvertime 是否超时
     */
    void timeInfoUpdated(const QString& messageId,
                         qint64 elapsedSec,
                         qint64 remainingSec,
                         bool isOvertime);

private slots:
    void onTimerTick();

private:
    MessageModel* m_messageModel = nullptr;
    QTimer* m_updateTimer = nullptr;
    QHash<QString, TaskTimeInfo> m_tasks;
    QSet<QString> m_pausedTasks;  ///< 暂停的任务ID集合
    QHash<QString, qint64> m_pausedElapsedSec;  ///< 暂停时的已用时间
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROCESSINGTIMEMANAGER_H
