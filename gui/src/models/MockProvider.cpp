#include "MockProvider.h"
#include "SystemMetrics.h"
#include "TaskMetrics.h"
#include "RootCauseModel.h"
#include "TimelineModel.h"
#include "JitterModel.h"
#include "ExperimentModel.h"
#include <QtMath>
#include <QVariantMap>

MockProvider::MockProvider(QObject *parent)
    : QObject(parent)
{
}

void MockProvider::populateSystemMetrics(SystemMetrics *metrics)
{
    metrics->setDeadlineMissRate("0.7%");
    metrics->setP99Response("8.7 ms");
    metrics->setWorstCaseResponse("12.8 ms");
    metrics->setP99ReadyWait("2.1 ms");
    metrics->setCanToResponse("11.2 ms");
    metrics->setTraceIntegrity("VALID");

    metrics->setAvgCpuUtil("68.4%");
    metrics->setCtxSwitches("4,821");
    metrics->setPreemptions("1,204");
    metrics->setMigrations("42");
    metrics->setCanRate("850 msg/s");
    metrics->setTraceRecords("2.4 M");
    metrics->setDroppedRecords("0");

    // The visual mockup requires these 10 points
    QVector<double> cpu0Pts = {0.65, 0.68, 0.62, 0.74, 0.80, 0.68, 0.70, 0.78, 0.66, 0.75};
    QVector<double> cpu1Pts = {0.82, 0.85, 0.80, 0.88, 0.90, 0.84, 0.86, 0.92, 0.85, 0.88};
    QVector<double> cpu2Pts = {0.30, 0.32, 0.28, 0.38, 0.42, 0.30, 0.32, 0.40, 0.32, 0.35};
    QVector<double> cpu3Pts = {0.82, 0.84, 0.79, 0.88, 0.89, 0.82, 0.84, 0.90, 0.80, 0.83};

    // Interpolate to create 300 points in SystemMetrics' canonical CPU histories
    QVector<double> cpu0(300), cpu1(300), cpu2(300), cpu3(300);
    for (int i = 0; i < 299; ++i) {
        double ratio = i / 299.0;
        double segFloat = ratio * 9.0;
        int idx = qFloor(segFloat);
        double fract = segFloat - idx;
        cpu0[i] = (1.0 - fract) * cpu0Pts[idx] + fract * cpu0Pts[idx + 1];
        cpu1[i] = (1.0 - fract) * cpu1Pts[idx] + fract * cpu1Pts[idx + 1];
        cpu2[i] = (1.0 - fract) * cpu2Pts[idx] + fract * cpu2Pts[idx + 1];
        cpu3[i] = (1.0 - fract) * cpu3Pts[idx] + fract * cpu3Pts[idx + 1];
    }
    cpu0[299] = cpu0Pts[9];
    cpu1[299] = cpu1Pts[9];
    cpu2[299] = cpu2Pts[9];
    cpu3[299] = cpu3Pts[9];

    metrics->setCpuHistories(cpu0, cpu1, cpu2, cpu3);
}

void MockProvider::populateTaskMetrics(TaskMetrics *metrics)
{
    QVariantList list;

    QVariantMap brake;
    brake["name"] = "BRAKE";
    brake["prio"] = 20;
    brake["period"] = "10 ms";
    brake["deadline"] = "10 ms";
    brake["p99"] = "8.7 ms";
    brake["miss"] = "0.7%";
    brake["status"] = "NOMINAL";
    list.append(brake);

    QVariantMap adas;
    adas["name"] = "ADAS";
    adas["prio"] = 18;
    adas["period"] = "25 ms";
    adas["deadline"] = "25 ms";
    adas["p99"] = "18.2 ms";
    adas["miss"] = "0.0%";
    adas["status"] = "NOMINAL";
    list.append(adas);

    QVariantMap diag;
    diag["name"] = "DIAGNOSTIC";
    diag["prio"] = 10;
    diag["period"] = "100 ms";
    diag["deadline"] = "100 ms";
    diag["p99"] = "45.0 ms";
    diag["miss"] = "0.0%";
    diag["status"] = "NOMINAL";
    list.append(diag);

    metrics->setWorkloadHealthList(list);
}

void MockProvider::populateRootCauseModel(RootCauseModel *model)
{
    QVariantMap summary;
    summary["task"] = "BRAKE #482";
    summary["type"] = "Deadline miss";
    summary["actual"] = "12.8 ms";
    summary["limit"] = "10.0 ms";
    summary["violation"] = "+2.8 ms";
    summary["cause"] = "CPU_CONTENTION";
    summary["confidence"] = "96%";
    summary["readyWait"] = "2.1 ms";
    summary["preemption"] = "5.8 ms";
    summary["blocking"] = "0.7 ms";
    summary["execution"] = "4.2 ms";
    model->setRootCauseSummary(summary);

    QVariantList causal;
    causal.append(QVariantMap{{"n", "1"}, {"t", "CAN 0x100 Received"}, {"d", "T+0.0 ms"}, {"c", "#22c55e"}});
    causal.append(QVariantMap{{"n", "2"}, {"t", "BRAKE Task Released"}, {"d", "T+0.18 ms"}, {"c", "#22c55e"}});
    causal.append(QVariantMap{{"n", "3"}, {"t", "Task Ready (Ready Wait Start)"}, {"d", "T+0.25 ms"}, {"c", "#3b82f6"}});
    causal.append(QVariantMap{{"n", "4"}, {"t", "Preempted by ADAS Task"}, {"d", "T+2.35 ms"}, {"c", "#ef4444"}});
    causal.append(QVariantMap{{"n", "5"}, {"t", "Execution Resumed on CPU 0"}, {"d", "T+8.15 ms"}, {"c", "#fb923c"}});
    causal.append(QVariantMap{{"n", "6"}, {"t", "Execution Finished"}, {"d", "T+12.80 ms"}, {"c", "#3b82f6"}});
    causal.append(QVariantMap{{"n", "7"}, {"t", "Deadline Miss Violation Detected"}, {"d", "T+12.80 ms"}, {"c", "#ef4444"}});
    model->setCausalFlowList(causal);

    QVariantList classes;
    classes.append(QVariantMap{{"cat", "CPU Contention (High Prio Preemption)"}, {"dur", "5.8 ms"}, {"pct", "45%"}, {"conf", "98%"}});
    classes.append(QVariantMap{{"cat", "Excessive Ready Latency"}, {"dur", "2.1 ms"}, {"pct", "16%"}, {"conf", "92%"}});
    classes.append(QVariantMap{{"cat", "Priority Inversion (Mutex A)"}, {"dur", "0.7 ms"}, {"pct", "5%"}, {"conf", "88%"}});
    classes.append(QVariantMap{{"cat", "Task Execution Time (CPU 0)"}, {"dur", "4.2 ms"}, {"pct", "33%"}, {"conf", "100%"}});
    model->setClassificationList(classes);
}

void MockProvider::populateTimelineModel(TimelineModel *model)
{
    QVariantList lanes;

    QVariantMap cpu0;
    cpu0["label"] = "CPU 0";
    QVariantList cpu0Bars;
    cpu0Bars.append(QVariantMap{{"l", "IDLE"}, {"c", "#e5e7eb"}, {"tc", "#6b7280"}, {"x", 0.0}, {"w", 0.14}});
    cpu0Bars.append(QVariantMap{{"l", "BRAKE"}, {"c", "#22c55e"}, {"x", 0.15}, {"w", 0.11}});
    cpu0Bars.append(QVariantMap{{"l", "ADAS_1"}, {"c", "#60a5fa"}, {"x", 0.27}, {"w", 0.13}});
    cpu0Bars.append(QVariantMap{{"l", "BRAKE #482"}, {"c", "#22c55e"}, {"x", 0.42}, {"w", 0.16}});
    cpu0Bars.append(QVariantMap{{"l", "BRAKE #482"}, {"c", "#22c55e"}, {"x", 0.60}, {"w", 0.13}});
    cpu0Bars.append(QVariantMap{{"l", "DIAG"}, {"c", "#fb923c"}, {"x", 0.74}, {"w", 0.10}});
    cpu0Bars.append(QVariantMap{{"l", "IDLE"}, {"c", "#e5e7eb"}, {"tc", "#6b7280"}, {"x", 0.85}, {"w", 0.10}});
    cpu0["bars"] = cpu0Bars;
    lanes.append(cpu0);

    QVariantMap cpu1;
    cpu1["label"] = "CPU 1";
    QVariantList cpu1Bars;
    cpu1Bars.append(QVariantMap{{"l", "ADAS_2"}, {"c", "#1f2937"}, {"x", 0.02}, {"w", 0.16}});
    cpu1Bars.append(QVariantMap{{"l", "STRESS"}, {"c", "#374151"}, {"x", 0.62}, {"w", 0.20}});
    cpu1["bars"] = cpu1Bars;
    lanes.append(cpu1);

    QVariantMap cpu2;
    cpu2["label"] = "CPU 2";
    QVariantList cpu2Bars;
    cpu2Bars.append(QVariantMap{{"l", "DIAG_POLL"}, {"c", "#8b5cf6"}, {"x", 0.20}, {"w", 0.20}});
    cpu2["bars"] = cpu2Bars;
    lanes.append(cpu2);

    QVariantMap cpu3;
    cpu3["label"] = "CPU 3";
    QVariantList cpu3Bars;
    cpu3Bars.append(QVariantMap{{"l", "IDLE"}, {"c", "#e5e7eb"}, {"tc", "#6b7280"}, {"x", 0.02}, {"w", 0.12}});
    cpu3Bars.append(QVariantMap{{"l", "STRESS"}, {"c", "#9ca3af"}, {"x", 0.52}, {"w", 0.30}});
    cpu3["bars"] = cpu3Bars;
    lanes.append(cpu3);

    model->setCpuLanes(lanes);

    model->setCanEvents(QVariantList{0.08, 0.62});
    model->setIrqs(QVariantList{0.10, 0.44, 0.73});
    model->setGpioMarkers(QVariantList{0.45});
}

void MockProvider::populateJitterModel(JitterModel *model)
{
    // Generate 256 rolling activations to fill the C++ model history capacity
    QVector<double> brakeOffsets = {0.18, 0.25, 0.31, 0.42, 0.55, 0.37, 0.82, 2.10, 0.44, 0.29};
    QVector<double> brakeResponses = {1.2, 1.5, 1.8, 2.1, 4.2, 3.1, 8.7, 12.8, 4.2, 2.0};
    quint64 brakePeriod = 10ULL * 1000000ULL;
    quint64 brakeDeadline = 10ULL * 1000000ULL;

    QVector<double> adasOffsets = {0.30, 0.45, 0.60, 0.85, 1.20, 0.70, 2.10, 4.80, 0.95, 0.50};
    QVector<double> adasResponses = {3.5, 3.9, 4.1, 4.5, 11.3, 8.2, 18.2, 22.1, 6.4, 4.0};
    quint64 adasPeriod = 25ULL * 1000000ULL;
    quint64 adasDeadline = 25ULL * 1000000ULL;

    QVector<double> diagOffsets = {0.90, 1.40, 1.80, 2.40, 4.20, 2.10, 6.80, 9.50, 3.10, 1.60};
    QVector<double> diagResponses = {7.2, 8.5, 9.0, 12.4, 28.0, 18.5, 45.0, 62.3, 14.8, 9.2};
    quint64 diagPeriod = 100ULL * 1000000ULL;
    quint64 diagDeadline = 100ULL * 1000000ULL;

    for (int i = 0; i < 256; ++i) {
        int idx = i % 10;
        
        // BRAKE_CTL
        quint64 bIdeal = i * brakePeriod;
        quint64 bRelease = bIdeal + static_cast<quint64>(brakeOffsets[idx] * 1000000.0);
        quint64 bFinish = bRelease + static_cast<quint64>(brakeResponses[idx] * 1000000.0);
        model->pushTiming(0, bIdeal, bRelease, bFinish, brakeDeadline);

        // ADAS_FUSION
        quint64 aIdeal = i * adasPeriod;
        quint64 aRelease = aIdeal + static_cast<quint64>(adasOffsets[idx] * 1000000.0);
        quint64 aFinish = aRelease + static_cast<quint64>(adasResponses[idx] * 1000000.0);
        model->pushTiming(1, aIdeal, aRelease, aFinish, adasDeadline);

        // DIAG_POLL
        quint64 dIdeal = i * diagPeriod;
        quint64 dRelease = dIdeal + static_cast<quint64>(diagOffsets[idx] * 1000000.0);
        quint64 dFinish = dRelease + static_cast<quint64>(diagResponses[idx] * 1000000.0);
        model->pushTiming(2, dIdeal, dRelease, dFinish, diagDeadline);
    }
}

void MockProvider::populateExperimentModel(ExperimentModel *model)
{
    model->setP99ResponseVals(QVariantList{26, 46, 68, 75, 118, 92, 102});
    model->setDeadlineMissYs(QVariantList{0.98, 0.95, 0.82, 0.68, 0.28, 0.22, 0.10});

    QVariantList cpus;
    cpus.append(QVariantMap{{"pts", QVariantList{0.35, 0.28, 0.38, 0.48, 0.58, 0.72, 0.78}}, {"col", "#3b82f6"}});
    cpus.append(QVariantMap{{"pts", QVariantList{0.42, 0.36, 0.40, 0.32, 0.45, 0.52, 0.62}}, {"col", "#ef4444"}});
    cpus.append(QVariantMap{{"pts", QVariantList{0.18, 0.15, 0.12, 0.18, 0.14, 0.22, 0.18}}, {"col", "#22c55e"}});
    cpus.append(QVariantMap{{"pts", QVariantList{0.30, 0.32, 0.35, 0.30, 0.38, 0.32, 0.42}}, {"col", "#f59e0b"}});
    model->setCtxReadyWaitCpus(cpus);

    QVariantList matrix;
    matrix.append(QVariantMap{{"name", "S0 - Baseline"}, {"status", "PASS"}, {"p99", "8.7 ms"}, {"miss", "0.7%"}, {"wait", "2.1 ms"}, {"jitter", "11.6 ms"}, {"cpu", "68.4%"}, {"can", "11.2 ms"}});
    matrix.append(QVariantMap{{"name", "S1 - CPU Saturation"}, {"status", "PASS"}, {"p99", "18.4 ms"}, {"miss", "0.0%"}, {"wait", "12.4 ms"}, {"jitter", "18.2 ms"}, {"cpu", "98.5%"}, {"can", "24.1 ms"}});
    matrix.append(QVariantMap{{"name", "S2 - Contention"}, {"status", "WARN"}, {"p99", "25.2 ms"}, {"miss", "2.4%"}, {"wait", "18.1 ms"}, {"jitter", "26.3 ms"}, {"cpu", "88.2%"}, {"can", "32.4 ms"}});
    matrix.append(QVariantMap{{"name", "S3 - CAN Burst"}, {"status", "PASS"}, {"p99", "12.8 ms"}, {"miss", "0.0%"}, {"wait", "4.2 ms"}, {"jitter", "14.8 ms"}, {"cpu", "72.1%"}, {"can", "15.6 ms"}});
    matrix.append(QVariantMap{{"name", "S4 - Event Storm"}, {"status", "CRITICAL"}, {"p99", "45.0 ms"}, {"miss", "8.4%"}, {"wait", "32.8 ms"}, {"jitter", "42.0 ms"}, {"cpu", "99.2%"}, {"can", "62.3 ms"}});
    matrix.append(QVariantMap{{"name", "S5 - Mixed Workload"}, {"status", "WARN"}, {"p99", "28.5 ms"}, {"miss", "3.8%"}, {"wait", "19.5 ms"}, {"jitter", "29.1 ms"}, {"cpu", "89.6%"}, {"can", "36.8 ms"}});
    matrix.append(QVariantMap{{"name", "S6 - CPU Affinity"}, {"status", "PASS"}, {"p99", "14.1 ms"}, {"miss", "0.0%"}, {"wait", "3.8 ms"}, {"jitter", "12.4 ms"}, {"cpu", "64.2%"}, {"can", "18.4 ms"}});
    model->setKpiMatrix(matrix);
}
