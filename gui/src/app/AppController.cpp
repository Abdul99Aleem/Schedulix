#include "AppController.h"
#include "SystemMetrics.h"
#include "TaskMetrics.h"
#include "RootCauseModel.h"
#include "TimelineModel.h"
#include "JitterModel.h"
#include "ExperimentModel.h"
#include "MockProvider.h"
#include <QQmlContext>

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    // Parent all child models to guarantee standard QObject lifetime management and clean destruction
    m_systemMetrics = new SystemMetrics(this);
    m_taskMetrics = new TaskMetrics(this);
    m_rootCauseModel = new RootCauseModel(this);
    m_timelineModel = new TimelineModel(this);
    m_jitterModel = new JitterModel(this);
    m_experimentModel = new ExperimentModel(this);
    m_mockProvider = new MockProvider(this);

    // Populate all domain models from the canonical MockProvider source of truth
    m_mockProvider->populateSystemMetrics(m_systemMetrics);
    m_mockProvider->populateTaskMetrics(m_taskMetrics);
    m_mockProvider->populateRootCauseModel(m_rootCauseModel);
    m_mockProvider->populateTimelineModel(m_timelineModel);
    m_mockProvider->populateJitterModel(m_jitterModel);
    m_mockProvider->populateExperimentModel(m_experimentModel);
}

AppController::~AppController()
{
    // Destruction of this object automatically stops/releases child resources
}

void AppController::registerContextProperties(QQmlEngine *engine)
{
    if (!engine) return;
    
    QQmlContext *ctx = engine->rootContext();
    ctx->setContextProperty("appController", this);
    ctx->setContextProperty("systemMetrics", m_systemMetrics);
    ctx->setContextProperty("taskMetrics", m_taskMetrics);
    ctx->setContextProperty("rootCauseModel", m_rootCauseModel);
    ctx->setContextProperty("timelineModel", m_timelineModel);
    ctx->setContextProperty("jitterModel", m_jitterModel);
    ctx->setContextProperty("experimentModel", m_experimentModel);
}
