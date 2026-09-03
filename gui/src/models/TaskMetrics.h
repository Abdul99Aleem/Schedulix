#ifndef TASKMETRICS_H
#define TASKMETRICS_H

#include <QObject>
#include <QVariantList>

class TaskMetrics : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList workloadHealthList READ workloadHealthList WRITE setWorkloadHealthList NOTIFY workloadHealthListChanged)

public:
    explicit TaskMetrics(QObject *parent = nullptr);

    QVariantList workloadHealthList() const { return m_workloadHealthList; }
    void setWorkloadHealthList(const QVariantList &list);

signals:
    void workloadHealthListChanged();

private:
    QVariantList m_workloadHealthList;
};

#endif // TASKMETRICS_H
