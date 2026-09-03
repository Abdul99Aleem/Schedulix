#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QQmlEngine>

#include "SystemMetrics.h"
#include "TaskMetrics.h"
#include "RootCauseModel.h"
#include "TimelineModel.h"
#include "JitterModel.h"
#include "ExperimentModel.h"

class MockProvider;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(SystemMetrics* systemMetrics READ systemMetrics CONSTANT)
    Q_PROPERTY(TaskMetrics* taskMetrics READ taskMetrics CONSTANT)
    Q_PROPERTY(RootCauseModel* rootCauseModel READ rootCauseModel CONSTANT)
    Q_PROPERTY(TimelineModel* timelineModel READ timelineModel CONSTANT)
    Q_PROPERTY(JitterModel* jitterModel READ jitterModel CONSTANT)
    Q_PROPERTY(ExperimentModel* experimentModel READ experimentModel CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    SystemMetrics* systemMetrics() const { return m_systemMetrics; }
    TaskMetrics* taskMetrics() const { return m_taskMetrics; }
    RootCauseModel* rootCauseModel() const { return m_rootCauseModel; }
    TimelineModel* timelineModel() const { return m_timelineModel; }
    JitterModel* jitterModel() const { return m_jitterModel; }
    ExperimentModel* experimentModel() const { return m_experimentModel; }

    void registerContextProperties(QQmlEngine *engine);

private:
    SystemMetrics *m_systemMetrics;
    TaskMetrics *m_taskMetrics;
    RootCauseModel *m_rootCauseModel;
    TimelineModel *m_timelineModel;
    JitterModel *m_jitterModel;
    ExperimentModel *m_experimentModel;
    MockProvider *m_mockProvider;
};

#endif // APPCONTROLLER_H
