#include "SystemMetrics.h"

SystemMetrics::SystemMetrics(QObject *parent)
    : QObject(parent)
{
}

QVariantList SystemMetrics::cpu0History() const
{
    return downsample(m_cpu0);
}

QVariantList SystemMetrics::cpu1History() const
{
    return downsample(m_cpu1);
}

QVariantList SystemMetrics::cpu2History() const
{
    return downsample(m_cpu2);
}

QVariantList SystemMetrics::cpu3History() const
{
    return downsample(m_cpu3);
}

void SystemMetrics::setDeadlineMissRate(const QString &val)
{
    if (m_deadlineMissRate != val) {
        m_deadlineMissRate = val;
        emit deadlineMissRateChanged();
    }
}

void SystemMetrics::setP99Response(const QString &val)
{
    if (m_p99Response != val) {
        m_p99Response = val;
        emit p99ResponseChanged();
    }
}

void SystemMetrics::setWorstCaseResponse(const QString &val)
{
    if (m_worstCaseResponse != val) {
        m_worstCaseResponse = val;
        emit worstCaseResponseChanged();
    }
}

void SystemMetrics::setP99ReadyWait(const QString &val)
{
    if (m_p99ReadyWait != val) {
        m_p99ReadyWait = val;
        emit p99ReadyWaitChanged();
    }
}

void SystemMetrics::setCanToResponse(const QString &val)
{
    if (m_canToResponse != val) {
        m_canToResponse = val;
        emit canToResponseChanged();
    }
}

void SystemMetrics::setTraceIntegrity(const QString &val)
{
    if (m_traceIntegrity != val) {
        m_traceIntegrity = val;
        emit traceIntegrityChanged();
    }
}

void SystemMetrics::setAvgCpuUtil(const QString &val)
{
    if (m_avgCpuUtil != val) {
        m_avgCpuUtil = val;
        emit avgCpuUtilChanged();
    }
}

void SystemMetrics::setCtxSwitches(const QString &val)
{
    if (m_ctxSwitches != val) {
        m_ctxSwitches = val;
        emit ctxSwitchesChanged();
    }
}

void SystemMetrics::setPreemptions(const QString &val)
{
    if (m_preemptions != val) {
        m_preemptions = val;
        emit preemptionsChanged();
    }
}

void SystemMetrics::setMigrations(const QString &val)
{
    if (m_migrations != val) {
        m_migrations = val;
        emit migrationsChanged();
    }
}

void SystemMetrics::setCanRate(const QString &val)
{
    if (m_canRate != val) {
        m_canRate = val;
        emit canRateChanged();
    }
}

void SystemMetrics::setTraceRecords(const QString &val)
{
    if (m_traceRecords != val) {
        m_traceRecords = val;
        emit traceRecordsChanged();
    }
}

void SystemMetrics::setDroppedRecords(const QString &val)
{
    if (m_droppedRecords != val) {
        m_droppedRecords = val;
        emit droppedRecordsChanged();
    }
}

void SystemMetrics::setCpuHistories(const QVector<double> &cpu0, const QVector<double> &cpu1, const QVector<double> &cpu2, const QVector<double> &cpu3)
{
    m_cpu0 = cpu0;
    m_cpu1 = cpu1;
    m_cpu2 = cpu2;
    m_cpu3 = cpu3;
    emit cpuHistoriesChanged();
}

QVariantList SystemMetrics::downsample(const QVector<double> &history) const
{
    QVariantList list;
    if (history.isEmpty()) return list;

    // Take exactly 10 points for the visual presentation layout
    int size = history.size();
    if (size <= 10) {
        for (double val : history) {
            list.append(val);
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            int idx = (i * (size - 1)) / 9;
            list.append(history[idx]);
        }
    }
    return list;
}
