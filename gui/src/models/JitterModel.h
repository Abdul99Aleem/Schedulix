#ifndef JITTERMODEL_H
#define JITTERMODEL_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include "TraceEvent.h"

class JitterModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentTask READ currentTask WRITE setCurrentTask NOTIFY currentTaskChanged)
    Q_PROPERTY(QVariantList derivedResponseTimes READ derivedResponseTimes NOTIFY derivedDataChanged)
    Q_PROPERTY(QVariantList derivedLatencyOffsets READ derivedLatencyOffsets NOTIFY derivedDataChanged)
    Q_PROPERTY(QVariantMap activeData READ activeData NOTIFY derivedDataChanged)

public:
    explicit JitterModel(QObject *parent = nullptr);

    QString currentTask() const { return m_currentTask; }
    void setCurrentTask(const QString &task);

    QVariantList derivedResponseTimes() const;
    QVariantList derivedLatencyOffsets() const;
    QVariantMap activeData() const;

    Q_INVOKABLE void selectTask(const QString &taskName);

    // Timing ingestion
    void pushTiming(int taskId, quint64 idealRelease_ns, quint64 release_ns, quint64 finish_ns, quint64 deadline_ns);

signals:
    void currentTaskChanged();
    void derivedDataChanged();

private:
    void calculateMetrics(const QVector<TaskTiming> &history, quint64 period_ns, quint64 deadline_ns, QVariantMap &outMetrics) const;
    QVariantList getLatestResponseTimes() const;
    QVariantList getLatestLatencyOffsets() const;

    QString m_currentTask;

    // Ring-buffers of size 256
    QVector<TaskTiming> m_brakeHistory;
    QVector<TaskTiming> m_adasHistory;
    QVector<TaskTiming> m_diagHistory;

    // Precalculated static fallback/mock data structures to guarantee consistent startup metrics
    QVariantMap m_brakeMetrics;
    QVariantMap m_adasMetrics;
    QVariantMap m_diagMetrics;
};

#endif // JITTERMODEL_H
