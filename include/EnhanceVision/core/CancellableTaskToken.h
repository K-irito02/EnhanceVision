#ifndef ENHANCEVISION_CANCELLABLE_TASK_TOKEN_H
#define ENHANCEVISION_CANCELLABLE_TASK_TOKEN_H

#include <QObject>
#include <QAtomicInt>
#include <stdexcept>

namespace EnhanceVision {

class TaskCancelledException : public std::runtime_error
{
public:
    TaskCancelledException()
        : std::runtime_error("Task has been cancelled")
    {}
};

class CancellableTaskToken : public QObject
{
    Q_OBJECT

public:
    explicit CancellableTaskToken(QObject* parent = nullptr);
    ~CancellableTaskToken() override = default;

    bool isCancelled() const;
    bool isForceCancelled() const;
    
    void cancel();
    void forceCancel();
    
    void checkAndThrow();
    void checkAndThrowIfForceCancelled();

    bool tryAcquire();
    void release();

signals:
    void cancelled();
    void forceCancelled();

private:
    QAtomicInt m_cancelled{0};
    QAtomicInt m_forceCancelled{0};
    QAtomicInt m_acquired{0};
};

inline bool CancellableTaskToken::isCancelled() const
{
    return m_cancelled.loadRelaxed() != 0;
}

inline bool CancellableTaskToken::isForceCancelled() const
{
    return m_forceCancelled.loadRelaxed() != 0;
}

inline void CancellableTaskToken::cancel()
{
    if (m_cancelled.testAndSetRelaxed(0, 1)) {
        emit cancelled();
    }
}

inline void CancellableTaskToken::forceCancel()
{
    m_forceCancelled.storeRelaxed(1);
    cancel();
    emit forceCancelled();
}

inline void CancellableTaskToken::checkAndThrow()
{
    if (isCancelled()) {
        throw TaskCancelledException();
    }
}

inline void CancellableTaskToken::checkAndThrowIfForceCancelled()
{
    if (isForceCancelled()) {
        throw TaskCancelledException();
    }
}

inline bool CancellableTaskToken::tryAcquire()
{
    return m_acquired.testAndSetRelaxed(0, 1);
}

inline void CancellableTaskToken::release()
{
    m_acquired.storeRelaxed(0);
}

} // namespace EnhanceVision

#endif // ENHANCEVISION_CANCELLABLE_TASK_TOKEN_H
