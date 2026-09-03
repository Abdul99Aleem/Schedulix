#ifndef EXPERIMENTMODEL_H
#define EXPERIMENTMODEL_H

#include <QObject>
#include <QVariantList>

class ExperimentModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList p99ResponseVals READ p99ResponseVals WRITE setP99ResponseVals NOTIFY experimentDataChanged)
    Q_PROPERTY(QVariantList deadlineMissYs READ deadlineMissYs WRITE setDeadlineMissYs NOTIFY experimentDataChanged)
    Q_PROPERTY(QVariantList ctxReadyWaitCpus READ ctxReadyWaitCpus WRITE setCtxReadyWaitCpus NOTIFY experimentDataChanged)
    Q_PROPERTY(QVariantList kpiMatrix READ kpiMatrix WRITE setKpiMatrix NOTIFY experimentDataChanged)

public:
    explicit ExperimentModel(QObject *parent = nullptr);

    QVariantList p99ResponseVals() const { return m_p99ResponseVals; }
    void setP99ResponseVals(const QVariantList &list);

    QVariantList deadlineMissYs() const { return m_deadlineMissYs; }
    void setDeadlineMissYs(const QVariantList &list);

    QVariantList ctxReadyWaitCpus() const { return m_ctxReadyWaitCpus; }
    void setCtxReadyWaitCpus(const QVariantList &list);

    QVariantList kpiMatrix() const { return m_kpiMatrix; }
    void setKpiMatrix(const QVariantList &list);

signals:
    void experimentDataChanged();

private:
    QVariantList m_p99ResponseVals;
    QVariantList m_deadlineMissYs;
    QVariantList m_ctxReadyWaitCpus;
    QVariantList m_kpiMatrix;
};

#endif // EXPERIMENTMODEL_H
