/**
 * @file StatisticsService.cpp
 * @brief 统计查询服务实现
 */

#include "EnhanceVision/services/StatisticsService.h"
#include "EnhanceVision/services/DatabaseService.h"
#include <QSqlQuery>
#include <QDateTime>

namespace EnhanceVision {

StatisticsService::StatisticsService(QObject* parent)
    : QObject(parent)
{
}

void StatisticsService::setDatabaseService(DatabaseService* db)
{
    m_dbService = db;
}

QVariantMap StatisticsService::getProcessingStats(const QString& startDate, const QString& endDate)
{
    QVariantMap result;
    if (!m_dbService) return result;

    QSqlQuery query(m_dbService->db());
    query.prepare(R"(
        SELECT
            date(timestamp, 'unixepoch', 'localtime') AS date,
            COUNT(*) AS total,
            SUM(CASE WHEN status = 3 THEN 1 ELSE 0 END) AS completed,
            SUM(CASE WHEN status = 4 THEN 1 ELSE 0 END) AS failed
        FROM messages
        WHERE timestamp >= ? AND timestamp <= ?
        GROUP BY date ORDER BY date
    )");

    qint64 startTs = QDateTime::fromString(startDate, "yyyy-MM-dd").toSecsSinceEpoch();
    qint64 endTs = QDateTime::fromString(endDate, "yyyy-MM-dd").addDays(1).toSecsSinceEpoch();
    query.addBindValue(startTs);
    query.addBindValue(endTs);

    QVariantList dailyData;
    while (query.next()) {
        QVariantMap day;
        day["date"] = query.value(0).toString();
        day["total"] = query.value(1).toInt();
        day["completed"] = query.value(2).toInt();
        day["failed"] = query.value(3).toInt();
        int total = query.value(1).toInt();
        int completed = query.value(2).toInt();
        day["successRate"] = total > 0 ? qRound(100.0 * completed / total * 10) / 10.0 : 0;
        dailyData.append(day);
    }

    result["daily"] = dailyData;
    return result;
}

QVariantList StatisticsService::getModelUsageStats()
{
    QVariantList result;
    if (!m_dbService) return result;

    QSqlQuery query(m_dbService->db());
    query.prepare(R"(
        SELECT ai_model_id, ai_category, COUNT(*) AS usage_count
        FROM messages WHERE ai_model_id != ''
        GROUP BY ai_model_id ORDER BY usage_count DESC
    )");
    query.exec();

    while (query.next()) {
        QVariantMap item;
        item["modelId"] = query.value(0).toString();
        item["category"] = query.value(1).toInt();
        item["usageCount"] = query.value(2).toInt();
        result.append(item);
    }
    return result;
}

QVariantMap StatisticsService::getShaderParamDistribution(const QString& paramName)
{
    QVariantMap result;
    if (!m_dbService || paramName.isEmpty()) return result;

    QSqlQuery query(m_dbService->db());
    query.prepare(QString(
        "SELECT CAST(json_extract(shader_params, '$.%1') * 10 AS INTEGER) / 10.0 AS bucket, "
        "COUNT(*) AS count "
        "FROM messages WHERE mode = 0 AND shader_params != '{}' AND json_extract(shader_params, '$.%1') IS NOT NULL "
        "GROUP BY bucket ORDER BY bucket"
    ).arg(paramName));
    query.exec();

    QVariantList buckets;
    while (query.next()) {
        QVariantMap b;
        b["value"] = query.value(0).toDouble();
        b["count"] = query.value(1).toInt();
        buckets.append(b);
    }
    result[paramName] = buckets;
    return result;
}

QVariantList StatisticsService::getMediaTypeDistribution()
{
    QVariantList result;
    if (!m_dbService) return result;

    QSqlQuery query(m_dbService->db());
    query.exec(R"(
        SELECT mf.media_type, COUNT(DISTINCT mf.id) AS file_count
        FROM media_files mf JOIN messages m ON mf.message_id = m.id
        GROUP BY mf.media_type ORDER BY file_count DESC
    )");

    while (query.next()) {
        QVariantMap item;
        item["mediaType"] = query.value(0).toInt();
        item["fileCount"] = query.value(1).toInt();
        result.append(item);
    }
    return result;
}

QVariantList StatisticsService::getResolutionDistribution()
{
    QVariantList result;
    if (!m_dbService) return result;

    QSqlQuery query(m_dbService->db());
    query.exec(R"(
        SELECT
            CASE
                WHEN mf.resolution_w >= 3840 THEN '4K+'
                WHEN mf.resolution_w >= 2560 THEN '2K~4K'
                WHEN mf.resolution_w >= 1920 THEN '1080p~2K'
                WHEN mf.resolution_w >= 1280 THEN '720p~1080p'
                ELSE '<720p'
            END AS resolution_range,
            COUNT(*) AS count
        FROM media_files mf
        GROUP BY resolution_range ORDER BY count DESC
    )");

    while (query.next()) {
        QVariantMap item;
        item["range"] = query.value(0).toString();
        item["count"] = query.value(1).toInt();
        result.append(item);
    }
    return result;
}

QVariantMap StatisticsService::getDataOverview()
{
    QVariantMap overview;
    if (!m_dbService) return overview;

    QVariantMap tableStats = m_dbService->getTableStats();
    overview["sessions"] = tableStats["sessions"];
    overview["messages"] = tableStats["messages"];
    overview["mediaFiles"] = tableStats["media_files"];
    overview["thumbnails"] = tableStats["thumbnails"];
    overview["databaseSize"] = m_dbService->databaseFileSize();
    overview["thumbnailSize"] = m_dbService->totalThumbnailSize();
    overview["thumbnailCount"] = m_dbService->thumbnailCount();

    return overview;
}

} // namespace EnhanceVision
