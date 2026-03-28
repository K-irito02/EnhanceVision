#ifndef ENHANCEVISION_PROGRESSREPORTER_H
#define ENHANCEVISION_PROGRESSREPORTER_H

#include "EnhanceVision/core/IProgressProvider.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <atomic>
#include <deque>

namespace EnhanceVision {

class ProgressReporter : public QObject, public IProgressProvider
{
    Q_OBJECT
    Q_INTERFACES(EnhanceVision::IProgressProvider)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY stageChanged)
    Q_PROPERTY(int estimatedRemainingSec READ estimatedRemainingSec NOTIFY estimatedRemainingSecChanged)

public:
    explicit ProgressReporter(QObject* parent = nullptr);
    ~ProgressReporter() override = default;

    double progress() const override;
    QString stage() const override;
    int estimatedRemainingSec() const override;
    bool isValid() const override;
    QObject* asQObject() override;

    void setProgress(double value, const QString& stage = QString());
    void reset();
    void forceUpdate(double value, const QString& stage = QString());
    
    void setProgress(double value, int step, int totalSteps, const QString& stage = QString());
    void setSubProgress(double subProgress, const QString& subStage = QString());
    
    void beginBatch(int totalItems, const QString& stage = QString());
    void updateBatch(int completedItems, const QString& subStage = QString());
    void endBatch();
    
    int totalSteps() const;
    int currentStep() const;
    QString subStage() const;
    int batchTotal() const;
    int batchCompleted() const;
    bool isBatchMode() const;

signals:
    void progressChanged(double progress);
    void stageChanged(const QString& stage);
    void estimatedRemainingSecChanged(int seconds);
    void subStageChanged(const QString& subStage);
    void batchProgressChanged(int completed, int total);

private:
    double smoothProgress(double rawValue);
    void updateEstimatedRemainingTime();
    bool isAnomalyJump(double newValue) const;
    void emitSignals(double progress, const QString& stage);

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
    
    std::atomic<int> m_totalSteps{1};
    std::atomic<int> m_currentStep{0};
    std::atomic<double> m_subProgress{0.0};
    QString m_subStage;
    
    std::atomic<int> m_batchTotal{0};
    std::atomic<int> m_batchCompleted{0};
    std::atomic<bool> m_batchMode{false};

    static constexpr double kMaxJumpDelta = 0.10;
    static constexpr qint64 kSmoothIntervalMs = 50;
    static constexpr qint64 kMinEmitIntervalMs = 66;
    static constexpr double kAnomalyThreshold = 0.20;
    static constexpr int kMaxHistorySize = 10;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PROGRESSREPORTER_H
