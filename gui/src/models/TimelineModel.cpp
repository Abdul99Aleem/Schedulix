#include "TimelineModel.h"

TimelineModel::TimelineModel(QObject *parent)
    : QObject(parent)
{
}

void TimelineModel::setCpuLanes(const QVariantList &list)
{
    if (m_cpuLanes != list) {
        m_cpuLanes = list;
        emit cpuLanesChanged();
    }
}

void TimelineModel::setCriticalTasks(const QVariantList &list)
{
    if (m_criticalTasks != list) {
        m_criticalTasks = list;
        emit criticalTasksChanged();
    }
}

void TimelineModel::setCanEvents(const QVariantList &list)
{
    if (m_canEvents != list) {
        m_canEvents = list;
        emit canEventsChanged();
    }
}

void TimelineModel::setIrqs(const QVariantList &list)
{
    if (m_irqs != list) {
        m_irqs = list;
        emit irqsChanged();
    }
}

void TimelineModel::setGpioMarkers(const QVariantList &list)
{
    if (m_gpioMarkers != list) {
        m_gpioMarkers = list;
        emit gpioMarkersChanged();
    }
}
