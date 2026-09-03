#include "ExperimentModel.h"

ExperimentModel::ExperimentModel(QObject *parent)
    : QObject(parent)
{
}

void ExperimentModel::setP99ResponseVals(const QVariantList &list)
{
    if (m_p99ResponseVals != list) {
        m_p99ResponseVals = list;
        emit experimentDataChanged();
    }
}

void ExperimentModel::setDeadlineMissYs(const QVariantList &list)
{
    if (m_deadlineMissYs != list) {
        m_deadlineMissYs = list;
        emit experimentDataChanged();
    }
}

void ExperimentModel::setCtxReadyWaitCpus(const QVariantList &list)
{
    if (m_ctxReadyWaitCpus != list) {
        m_ctxReadyWaitCpus = list;
        emit experimentDataChanged();
    }
}

void ExperimentModel::setKpiMatrix(const QVariantList &list)
{
    if (m_kpiMatrix != list) {
        m_kpiMatrix = list;
        emit experimentDataChanged();
    }
}
