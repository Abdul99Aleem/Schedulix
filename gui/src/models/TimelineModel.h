#ifndef TIMELINEMODEL_H
#define TIMELINEMODEL_H

#include <QObject>
#include <QVariantList>

class TimelineModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList cpuLanes READ cpuLanes WRITE setCpuLanes NOTIFY cpuLanesChanged)
    Q_PROPERTY(QVariantList criticalTasks READ criticalTasks WRITE setCriticalTasks NOTIFY criticalTasksChanged)
    Q_PROPERTY(QVariantList canEvents READ canEvents WRITE setCanEvents NOTIFY canEventsChanged)
    Q_PROPERTY(QVariantList irqs READ irqs WRITE setIrqs NOTIFY irqsChanged)
    Q_PROPERTY(QVariantList gpioMarkers READ gpioMarkers WRITE setGpioMarkers NOTIFY gpioMarkersChanged)

public:
    explicit TimelineModel(QObject *parent = nullptr);

    QVariantList cpuLanes() const { return m_cpuLanes; }
    void setCpuLanes(const QVariantList &list);

    QVariantList criticalTasks() const { return m_criticalTasks; }
    void setCriticalTasks(const QVariantList &list);

    QVariantList canEvents() const { return m_canEvents; }
    void setCanEvents(const QVariantList &list);

    QVariantList irqs() const { return m_irqs; }
    void setIrqs(const QVariantList &list);

    QVariantList gpioMarkers() const { return m_gpioMarkers; }
    void setGpioMarkers(const QVariantList &list);

signals:
    void cpuLanesChanged();
    void criticalTasksChanged();
    void canEventsChanged();
    void irqsChanged();
    void gpioMarkersChanged();

private:
    QVariantList m_cpuLanes;
    QVariantList m_criticalTasks;
    QVariantList m_canEvents;
    QVariantList m_irqs;
    QVariantList m_gpioMarkers;
};

#endif // TIMELINEMODEL_H
