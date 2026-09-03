#ifndef MOCKPROVIDER_H
#define MOCKPROVIDER_H

#include <QObject>

class SystemMetrics;
class TaskMetrics;
class RootCauseModel;
class TimelineModel;
class JitterModel;
class ExperimentModel;

class MockProvider : public QObject {
    Q_OBJECT

public:
    explicit MockProvider(QObject *parent = nullptr);

    void populateSystemMetrics(SystemMetrics *metrics);
    void populateTaskMetrics(TaskMetrics *metrics);
    void populateRootCauseModel(RootCauseModel *model);
    void populateTimelineModel(TimelineModel *model);
    void populateJitterModel(JitterModel *model);
    void populateExperimentModel(ExperimentModel *model);
};

#endif // MOCKPROVIDER_H
