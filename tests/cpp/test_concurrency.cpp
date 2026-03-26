/**
 * @file test_concurrency.cpp
 * @brief 并发架构组件单元测试
 * @author Qt客户端开发工程师
 */

#include <QTest>
#include <QSignalSpy>
#include <QCoreApplication>

#include "EnhanceVision/core/PriorityTaskQueue.h"
#include "EnhanceVision/core/PriorityAdjuster.h"
#include "EnhanceVision/core/DeadlockDetector.h"
#include "EnhanceVision/core/TaskTimeoutWatchdog.h"
#include "EnhanceVision/core/TaskRetryPolicy.h"
#include "EnhanceVision/core/ConcurrencyManager.h"

using namespace EnhanceVision;

// ==================== PriorityTaskQueue Tests ====================

class TestPriorityTaskQueue : public QObject
{
    Q_OBJECT

private:
    PriorityTaskEntry makeEntry(const QString& id, TaskPriority prio,
                                 const QString& session = "s1",
                                 const QString& message = "m1")
    {
        PriorityTaskEntry e;
        e.taskId = id;
        e.priority = prio;
        e.sessionId = session;
        e.messageId = message;
        e.enqueuedAt = QDateTime::currentDateTime();
        return e;
    }

private slots:
    void testEnqueueDequeue()
    {
        PriorityTaskQueue q;
        QCOMPARE(q.size(), 0);
        QVERIFY(q.isEmpty());

        q.enqueue(makeEntry("t1", TaskPriority::Background));
        q.enqueue(makeEntry("t2", TaskPriority::UserInteractive));
        q.enqueue(makeEntry("t3", TaskPriority::Utility));

        QCOMPARE(q.size(), 3);
        QVERIFY(!q.isEmpty());

        auto top = q.dequeue();
        QCOMPARE(top.taskId, QString("t2"));
        QCOMPARE(top.priority, TaskPriority::UserInteractive);

        top = q.dequeue();
        QCOMPARE(top.taskId, QString("t3"));

        top = q.dequeue();
        QCOMPARE(top.taskId, QString("t1"));

        QVERIFY(q.isEmpty());
    }

    void testRemove()
    {
        PriorityTaskQueue q;
        q.enqueue(makeEntry("t1", TaskPriority::UserInitiated));
        q.enqueue(makeEntry("t2", TaskPriority::Background));

        q.remove("t1");
        QCOMPARE(q.size(), 1);

        auto top = q.dequeue();
        QCOMPARE(top.taskId, QString("t2"));
    }

    void testUpdatePriority()
    {
        PriorityTaskQueue q;
        q.enqueue(makeEntry("t1", TaskPriority::Background));
        q.enqueue(makeEntry("t2", TaskPriority::Background));

        q.updatePriority("t1", TaskPriority::UserInteractive);

        auto top = q.dequeue();
        QCOMPARE(top.taskId, QString("t1"));
        QCOMPARE(top.priority, TaskPriority::UserInteractive);
    }

    void testContains()
    {
        PriorityTaskQueue q;
        q.enqueue(makeEntry("t1", TaskPriority::Utility));

        QVERIFY(q.contains("t1"));
        QVERIFY(!q.contains("t2"));

        q.remove("t1");
        QVERIFY(!q.contains("t1"));
    }

    void testFIFOWithinSamePriority()
    {
        PriorityTaskQueue q;
        q.enqueue(makeEntry("first", TaskPriority::Utility));
        q.enqueue(makeEntry("second", TaskPriority::Utility));
        q.enqueue(makeEntry("third", TaskPriority::Utility));

        QCOMPARE(q.dequeue().taskId, QString("first"));
        QCOMPARE(q.dequeue().taskId, QString("second"));
        QCOMPARE(q.dequeue().taskId, QString("third"));
    }
};

// ==================== DeadlockDetector Tests ====================

class TestDeadlockDetector : public QObject
{
    Q_OBJECT

private slots:
    void testNoCycle()
    {
        DeadlockDetector dd;
        dd.addWaitEdge("A", "B", "res1");
        dd.addWaitEdge("B", "C", "res2");

        QVERIFY(!dd.detectCycle());
        QCOMPARE(dd.edgeCount(), 2);
    }

    void testSimpleCycle()
    {
        DeadlockDetector dd;
        dd.addWaitEdge("A", "B", "res1");
        dd.addWaitEdge("B", "A", "res2");

        QVERIFY(dd.detectCycle());

        QStringList participants = dd.getCycleParticipants();
        QVERIFY(!participants.isEmpty());
        QVERIFY(participants.contains("A"));
        QVERIFY(participants.contains("B"));
    }

    void testTriangleCycle()
    {
        DeadlockDetector dd;
        dd.addWaitEdge("A", "B", "r1");
        dd.addWaitEdge("B", "C", "r2");
        dd.addWaitEdge("C", "A", "r3");

        QVERIFY(dd.detectCycle());
    }

    void testRemoveBreaksCycle()
    {
        DeadlockDetector dd;
        dd.addWaitEdge("A", "B", "r1");
        dd.addWaitEdge("B", "A", "r2");
        QVERIFY(dd.detectCycle());

        dd.removeWaitEdge("A", "r1");
        QVERIFY(!dd.detectCycle());
    }

    void testRemoveAllEdges()
    {
        DeadlockDetector dd;
        dd.addWaitEdge("A", "B", "r1");
        dd.addWaitEdge("A", "C", "r2");
        dd.addWaitEdge("B", "A", "r3");

        dd.removeAllEdgesFor("A");
        QCOMPARE(dd.edgeCount(), 0);
    }

    void testSignalEmission()
    {
        DeadlockDetector dd;
        dd.setCheckInterval(1000);

        QSignalSpy deadlockSpy(&dd, &DeadlockDetector::deadlockDetected);
        QSignalSpy recoverySpy(&dd, &DeadlockDetector::recoveryRequested);

        dd.addWaitEdge("A", "B", "r1");
        dd.addWaitEdge("B", "A", "r2");

        dd.start();
        QVERIFY(deadlockSpy.wait(3000));
        QVERIFY(deadlockSpy.count() >= 1);
        QVERIFY(recoverySpy.count() >= 1);
        dd.stop();
    }
};

// ==================== TaskTimeoutWatchdog Tests ====================

class TestTaskTimeoutWatchdog : public QObject
{
    Q_OBJECT

private slots:
    void testWatchUnwatch()
    {
        TaskTimeoutWatchdog wdog;
        wdog.watchTask("t1", 60000);
        QCOMPARE(wdog.watchedTaskCount(), 1);

        wdog.unwatchTask("t1");
        QCOMPARE(wdog.watchedTaskCount(), 0);
    }

    void testTimeoutSignal()
    {
        TaskTimeoutWatchdog wdog;
        wdog.setCheckInterval(200); // 缩短检查间隔以加速测试
        QSignalSpy timeoutSpy(&wdog, &TaskTimeoutWatchdog::taskTimedOut);

        wdog.watchTask("t1", 500);
        wdog.start();

        QVERIFY(timeoutSpy.wait(5000));
        QCOMPARE(timeoutSpy.count(), 1);
        QCOMPARE(timeoutSpy.at(0).at(0).toString(), QString("t1"));
        wdog.stop();
    }

    void testResetPreventsTimeout()
    {
        TaskTimeoutWatchdog wdog;
        QSignalSpy timeoutSpy(&wdog, &TaskTimeoutWatchdog::taskTimedOut);

        wdog.watchTask("t1", 800);
        wdog.start();

        QTest::qWait(400);
        wdog.resetTimer("t1");

        QTest::qWait(600);
        QCOMPARE(timeoutSpy.count(), 0);
        wdog.stop();
    }
};

// ==================== TaskRetryPolicy Tests ====================

class TestTaskRetryPolicy : public QObject
{
    Q_OBJECT

private slots:
    void testRetryAttempts()
    {
        TaskRetryPolicy policy;
        QSignalSpy retrySpy(&policy, &TaskRetryPolicy::retryScheduled);
        QSignalSpy exhaustedSpy(&policy, &TaskRetryPolicy::retriesExhausted);

        policy.recordAttempt("t1", "error");
        QCOMPARE(retrySpy.count(), 1);
        QCOMPARE(exhaustedSpy.count(), 0);

        policy.recordAttempt("t1", "error");
        policy.recordAttempt("t1", "error");

        QVERIFY(exhaustedSpy.count() >= 1);
    }

    void testResetTask()
    {
        TaskRetryPolicy policy;
        policy.recordAttempt("t1", "error");
        policy.resetTask("t1");

        QSignalSpy retrySpy(&policy, &TaskRetryPolicy::retryScheduled);
        policy.recordAttempt("t1", "error");
        QCOMPARE(retrySpy.count(), 1);
    }

    void testExponentialBackoff()
    {
        TaskRetryPolicy policy;
        QSignalSpy retrySpy(&policy, &TaskRetryPolicy::retryScheduled);

        policy.recordAttempt("t1", "err");
        int delay1 = retrySpy.at(0).at(1).toInt();

        policy.recordAttempt("t1", "err");
        int delay2 = retrySpy.at(1).at(1).toInt();

        QVERIFY(delay2 > delay1);
    }
};

// ==================== ConcurrencyManager Tests ====================

class TestConcurrencyManager : public QObject
{
    Q_OBJECT

private:
    PriorityTaskEntry makeEntry(const QString& id, TaskPriority prio,
                                 const QString& session = "s1",
                                 const QString& message = "m1")
    {
        PriorityTaskEntry e;
        e.taskId = id;
        e.priority = prio;
        e.sessionId = session;
        e.messageId = message;
        e.enqueuedAt = QDateTime::currentDateTime();
        return e;
    }

private slots:
    void testSlotAcquireRelease()
    {
        ConcurrencyManager mgr;
        QCOMPARE(mgr.activeSessionCount(), 0);

        mgr.acquireSlot("t1", "s1", "m1");
        QCOMPARE(mgr.activeSessionCount(), 1);
        QCOMPARE(mgr.activeTasksForSession("s1"), 1);
        QCOMPARE(mgr.activeTasksForMessage("m1"), 1);

        mgr.acquireSlot("t2", "s1", "m1");
        QCOMPARE(mgr.activeSessionCount(), 1);
        QCOMPARE(mgr.activeTasksForSession("s1"), 2);

        mgr.releaseSlot("t1", "s1", "m1");
        QCOMPARE(mgr.activeTasksForSession("s1"), 1);

        mgr.releaseSlot("t2", "s1", "m1");
        QCOMPARE(mgr.activeTasksForSession("s1"), 0);
    }

    void testMultiSessionTracking()
    {
        ConcurrencyManager mgr;
        mgr.acquireSlot("t1", "s1", "m1");
        mgr.acquireSlot("t2", "s2", "m2");

        QCOMPARE(mgr.activeSessionCount(), 2);
        QCOMPARE(mgr.totalActiveSlots(), 2);

        mgr.releaseSlot("t1", "s1", "m1");
        QCOMPARE(mgr.activeSessionCount(), 1);
    }

    void testEnqueueRemove()
    {
        ConcurrencyManager mgr;
        mgr.enqueueTask(makeEntry("t1", TaskPriority::Utility));
        QCOMPARE(mgr.pendingTaskCount(), 1);

        mgr.removeTask("t1");
        QCOMPARE(mgr.pendingTaskCount(), 0);
    }
};

// ==================== Main ====================

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    int status = 0;

    {
        TestPriorityTaskQueue t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestDeadlockDetector t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestTaskTimeoutWatchdog t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestTaskRetryPolicy t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestConcurrencyManager t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_concurrency.moc"
