#ifndef ENHANCEVISION_PROGRESSREPORTER_H
#define ENHANCEVISION_PROGRESSREPORTER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <atomic>
#include <deque>

namespace EnhanceVision {

class ProgressReporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY stageChanged)
    Q_PROPERTY(int estimatedRemainingSec READ estimatedRemainingSec NOTIFY estimatedRemainingSecChanged)

public:
    explicit ProgressReporter(QObject* parent = nullptr);
    ~ProgressReporter() override = default;

    double progress() const;
    QString stage() const;
    int estimatedRemainingSec() const;

    void setProgress(double value, const QString& stage = QString());
    void reset();
    void forceUpdate(double value, const QString& stage = QString());

signals:
    void progressChanged(double progress);
    void stageChanged(const QString& stage);
    void estimatedRemainingSecChanged(int seconds);

private:
    double smoothProgress(double rawValue);
    void updateEstimatedRemainingTime();
    bool isAnomalyJump(double newValue) const;

private:
    std::atomic<double> m_progress{0.0};
    std::atomic<double> m_rawProgress{0.0};
    QString m_stage;
    std::atomic<int> m_estimatedRemainingSec{0};

    std::atomic<qint64> m_startTimeMs{0};
    std::atomic<qint64> m_lastEmitMs{0};

    mutable QMutex m_mutex;

    std::deque<double> m_progressHistory;
    std::deque<qint64> m_timeHistory;

    static constexpr double kMaxJumpDelta = 0.10;
    static constexpr qint64 kSmoothIntervalMs = 50;
    static constexpr qint64 kMinEmitIntervalMs = 66;
    static constexpr double kAnomalyThreshold = 0.20;
    static constexpr int kMaxHistorySize = 10;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROGRESSREPORTER_H
