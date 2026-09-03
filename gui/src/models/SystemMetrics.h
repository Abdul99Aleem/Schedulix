#ifndef SYSTEMMETRICS_H
#define SYSTEMMETRICS_H

#include <QObject>
#include <QVariantList>
#include <QVector>

class SystemMetrics : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString deadlineMissRate READ deadlineMissRate WRITE setDeadlineMissRate NOTIFY deadlineMissRateChanged)
    Q_PROPERTY(QString p99Response READ p99Response WRITE setP99Response NOTIFY p99ResponseChanged)
    Q_PROPERTY(QString worstCaseResponse READ worstCaseResponse WRITE setWorstCaseResponse NOTIFY worstCaseResponseChanged)
    Q_PROPERTY(QString p99ReadyWait READ p99ReadyWait WRITE setP99ReadyWait NOTIFY p99ReadyWaitChanged)
    Q_PROPERTY(QString canToResponse READ canToResponse WRITE setCanToResponse NOTIFY canToResponseChanged)
    Q_PROPERTY(QString traceIntegrity READ traceIntegrity WRITE setTraceIntegrity NOTIFY traceIntegrityChanged)

    Q_PROPERTY(QString avgCpuUtil READ avgCpuUtil WRITE setAvgCpuUtil NOTIFY avgCpuUtilChanged)
    Q_PROPERTY(QString ctxSwitches READ ctxSwitches WRITE setCtxSwitches NOTIFY ctxSwitchesChanged)
    Q_PROPERTY(QString preemptions READ preemptions WRITE setPreemptions NOTIFY preemptionsChanged)
    Q_PROPERTY(QString migrations READ migrations WRITE setMigrations NOTIFY migrationsChanged)
    Q_PROPERTY(QString canRate READ canRate WRITE setCanRate NOTIFY canRateChanged)
    Q_PROPERTY(QString traceRecords READ traceRecords WRITE setTraceRecords NOTIFY traceRecordsChanged)
    Q_PROPERTY(QString droppedRecords READ droppedRecords WRITE setDroppedRecords NOTIFY droppedRecordsChanged)

    Q_PROPERTY(QVariantList cpu0History READ cpu0History NOTIFY cpuHistoriesChanged)
    Q_PROPERTY(QVariantList cpu1History READ cpu1History NOTIFY cpuHistoriesChanged)
    Q_PROPERTY(QVariantList cpu2History READ cpu2History NOTIFY cpuHistoriesChanged)
    Q_PROPERTY(QVariantList cpu3History READ cpu3History NOTIFY cpuHistoriesChanged)

public:
    explicit SystemMetrics(QObject *parent = nullptr);

    // Getters
    QString deadlineMissRate() const { return m_deadlineMissRate; }
    QString p99Response() const { return m_p99Response; }
    QString worstCaseResponse() const { return m_worstCaseResponse; }
    QString p99ReadyWait() const { return m_p99ReadyWait; }
    QString canToResponse() const { return m_canToResponse; }
    QString traceIntegrity() const { return m_traceIntegrity; }

    QString avgCpuUtil() const { return m_avgCpuUtil; }
    QString ctxSwitches() const { return m_ctxSwitches; }
    QString preemptions() const { return m_preemptions; }
    QString migrations() const { return m_migrations; }
    QString canRate() const { return m_canRate; }
    QString traceRecords() const { return m_traceRecords; }
    QString droppedRecords() const { return m_droppedRecords; }

    QVariantList cpu0History() const;
    QVariantList cpu1History() const;
    QVariantList cpu2History() const;
    QVariantList cpu3History() const;

    // Setters
    void setDeadlineMissRate(const QString &val);
    void setP99Response(const QString &val);
    void setWorstCaseResponse(const QString &val);
    void setP99ReadyWait(const QString &val);
    void setCanToResponse(const QString &val);
    void setTraceIntegrity(const QString &val);

    void setAvgCpuUtil(const QString &val);
    void setCtxSwitches(const QString &val);
    void setPreemptions(const QString &val);
    void setMigrations(const QString &val);
    void setCanRate(const QString &val);
    void setTraceRecords(const QString &val);
    void setDroppedRecords(const QString &val);

    // History methods
    void setCpuHistories(const QVector<double> &cpu0, const QVector<double> &cpu1, const QVector<double> &cpu2, const QVector<double> &cpu3);

signals:
    void deadlineMissRateChanged();
    void p99ResponseChanged();
    void worstCaseResponseChanged();
    void p99ReadyWaitChanged();
    void canToResponseChanged();
    void traceIntegrityChanged();

    void avgCpuUtilChanged();
    void ctxSwitchesChanged();
    void preemptionsChanged();
    void migrationsChanged();
    void canRateChanged();
    void traceRecordsChanged();
    void droppedRecordsChanged();

    void cpuHistoriesChanged();

private:
    QVariantList downsample(const QVector<double> &history) const;

    QString m_deadlineMissRate;
    QString m_p99Response;
    QString m_worstCaseResponse;
    QString m_p99ReadyWait;
    QString m_canToResponse;
    QString m_traceIntegrity;

    QString m_avgCpuUtil;
    QString m_ctxSwitches;
    QString m_preemptions;
    QString m_migrations;
    QString m_canRate;
    QString m_traceRecords;
    QString m_droppedRecords;

    QVector<double> m_cpu0;
    QVector<double> m_cpu1;
    QVector<double> m_cpu2;
    QVector<double> m_cpu3;
};

#endif // SYSTEMMETRICS_H
