/**
 * @file StatisticsService.h
 * @brief 统计查询服务
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_STATISTICSSERVICE_H
#define ENHANCEVISION_STATISTICSSERVICE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QString>

namespace EnhanceVision {

class DatabaseService;

class StatisticsService : public QObject
{
    Q_OBJECT

public:
    explicit StatisticsService(QObject* parent = nullptr);
    ~StatisticsService() override = default;

    void setDatabaseService(DatabaseService* db);

    Q_INVOKABLE QVariantMap getProcessingStats(const QString& startDate, const QString& endDate);
    Q_INVOKABLE QVariantList getModelUsageStats();
    Q_INVOKABLE QVariantMap getShaderParamDistribution(const QString& paramName);
    Q_INVOKABLE QVariantList getMediaTypeDistribution();
    Q_INVOKABLE QVariantList getResolutionDistribution();
    Q_INVOKABLE QVariantMap getDataOverview();

private:
    DatabaseService* m_dbService = nullptr;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_STATISTICSSERVICE_H
