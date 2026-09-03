#include "TaskMetrics.h"

TaskMetrics::TaskMetrics(QObject *parent)
    : QObject(parent)
{
}

void TaskMetrics::setWorkloadHealthList(const QVariantList &list)
{
    if (m_workloadHealthList != list) {
        m_workloadHealthList = list;
        emit workloadHealthListChanged();
    }
}
