#include "JitterModel.h"
#include <algorithm>
#include <QtMath>

JitterModel::JitterModel(QObject *parent)
    : QObject(parent)
    , m_currentTask("BRAKE_CTL")
{
}

void JitterModel::setCurrentTask(const QString &task)
{
    if (m_currentTask != task) {
        m_currentTask = task;
        emit currentTaskChanged();
        emit derivedDataChanged();
    }
}

void JitterModel::selectTask(const QString &taskName)
{
    setCurrentTask(taskName);
}

QVariantList JitterModel::derivedResponseTimes() const
{
    return getLatestResponseTimes();
}

QVariantList JitterModel::derivedLatencyOffsets() const
{
    return getLatestLatencyOffsets();
}

QVariantMap JitterModel::activeData() const
{
    QVariantMap map;
    const QVector<TaskTiming> *history = nullptr;
    quint64 period = 10000000; // default 10 ms
    quint64 deadline = 10000000;

    if (m_currentTask == "BRAKE_CTL") {
        history = &m_brakeHistory;
        period = 10ULL * 1000000ULL;
        deadline = 10ULL * 1000000ULL;
    } else if (m_currentTask == "ADAS_FUSION") {
        history = &m_adasHistory;
        period = 25ULL * 1000000ULL;
        deadline = 25ULL * 1000000ULL;
    } else {
        history = &m_diagHistory;
        period = 100ULL * 1000000ULL;
        deadline = 100ULL * 1000000ULL;
    }

    if (history && !history->isEmpty()) {
        calculateMetrics(*history, period, deadline, map);
    }
    return map;
}

void JitterModel::pushTiming(int taskId, quint64 idealRelease_ns, quint64 release_ns, quint64 finish_ns, quint64 deadline_ns)
{
    TaskTiming timing;
    timing.release_ns = release_ns;
    timing.ready_ns = release_ns;
    timing.start_ns = release_ns;
    timing.finish_ns = finish_ns;
    timing.deadline_ns = deadline_ns;
    timing.response_ns = finish_ns - release_ns;
    timing.deadline_miss = (timing.response_ns > deadline_ns);
    timing.preemption_count = 0;

    QVector<TaskTiming> *history = nullptr;
    if (taskId == 0) history = &m_brakeHistory;
    else if (taskId == 1) history = &m_adasHistory;
    else history = &m_diagHistory;

    if (history) {
        if (history->size() >= 256) {
            history->removeFirst(); // Enforce bounded rolling window size of 256
        }
        history->append(timing);
        emit derivedDataChanged();
    }
}

void JitterModel::calculateMetrics(const QVector<TaskTiming> &history, quint64 period_ns, quint64 deadline_ns, QVariantMap &outMetrics) const
{
    if (history.isEmpty()) return;

    QVector<double> responses;
    QVector<double> offsets;
    double offsetSum = 0.0;

    double minResponse = history[0].response_ns / 1000000.0;
    double maxResponse = minResponse;

    double minOffset = 0.0;
    double maxOffset = 0.0;

    for (int i = 0; i < history.size(); ++i) {
        double resp_ms = history[i].response_ns / 1000000.0;
        responses.append(resp_ms);
        if (resp_ms < minResponse) minResponse = resp_ms;
        if (resp_ms > maxResponse) maxResponse = resp_ms;

        // ideal release is relative to start of history
        quint64 ideal = i * period_ns;
        double offset_ms = 0.0;
        if (history[i].release_ns > history[0].release_ns) {
            quint64 relativeRelease = history[i].release_ns - history[0].release_ns;
            if (relativeRelease > ideal) {
                offset_ms = (relativeRelease - ideal) / 1000000.0;
            } else {
                offset_ms = (ideal - relativeRelease) / 1000000.0;
            }
        }
        offsets.append(offset_ms);
        offsetSum += offset_ms;

        if (i == 0) {
            minOffset = offset_ms;
            maxOffset = offset_ms;
        } else {
            if (offset_ms < minOffset) minOffset = offset_ms;
            if (offset_ms > maxOffset) maxOffset = offset_ms;
        }
    }

    QVector<double> sortedResponses = responses;
    std::sort(sortedResponses.begin(), sortedResponses.end());

    double p50 = sortedResponses[qRound(0.50 * (sortedResponses.size() - 1))];
    double p95 = sortedResponses[qRound(0.95 * (sortedResponses.size() - 1))];
    double p99 = sortedResponses[qRound(0.99 * (sortedResponses.size() - 1))];

    double meanOffset = offsetSum / history.size();

    // Use exact display requirements
    outMetrics["p50"] = QString::number(p50, 'f', 1) + " ms";
    outMetrics["p95"] = QString::number(p95, 'f', 1) + " ms";
    outMetrics["p99"] = QString::number(p99, 'f', 1) + " ms";
    outMetrics["min"] = QString::number(minResponse, 'f', 1) + " ms";
    outMetrics["max"] = QString::number(maxResponse, 'f', 1) + " ms";
    outMetrics["jitter"] = QString::number(maxResponse - minResponse, 'f', 1) + " ms";

    outMetrics["meanOffset"] = QString::number(meanOffset, 'f', 2) + " ms";
    outMetrics["releaseMin"] = QString::number(minOffset, 'f', 2) + " ms";
    outMetrics["releaseMax"] = QString::number(maxOffset, 'f', 2) + " ms";
    outMetrics["releaseJitter"] = QString::number(maxOffset - minOffset, 'f', 2) + " ms";
}

QVariantList JitterModel::getLatestResponseTimes() const
{
    QVariantList list;
    const QVector<TaskTiming> *history = nullptr;

    if (m_currentTask == "BRAKE_CTL") history = &m_brakeHistory;
    else if (m_currentTask == "ADAS_FUSION") history = &m_adasHistory;
    else history = &m_diagHistory;

    if (history) {
        // Expose only the last 10 points for the visual presentation layout in QML
        int count = qMin(history->size(), 10);
        int startIdx = history->size() - count;
        for (int i = 0; i < count; ++i) {
            list.append(history->at(startIdx + i).response_ns / 1000000.0);
        }
    }
    return list;
}

QVariantList JitterModel::getLatestLatencyOffsets() const
{
    QVariantList list;
    const QVector<TaskTiming> *history = nullptr;
    quint64 period = 10000000;

    if (m_currentTask == "BRAKE_CTL") {
        history = &m_brakeHistory;
        period = 10ULL * 1000000ULL;
    } else if (m_currentTask == "ADAS_FUSION") {
        history = &m_adasHistory;
        period = 25ULL * 1000000ULL;
    } else {
        history = &m_diagHistory;
        period = 100ULL * 1000000ULL;
    }

    if (history) {
        int count = qMin(history->size(), 10);
        int startIdx = history->size() - count;
        for (int i = 0; i < count; ++i) {
            int globalIdx = startIdx + i;
            quint64 ideal = globalIdx * period;
            double offset_ms = 0.0;
            if (history->at(globalIdx).release_ns > history->at(0).release_ns) {
                quint64 relativeRelease = history->at(globalIdx).release_ns - history->at(0).release_ns;
                if (relativeRelease > ideal) {
                    offset_ms = (relativeRelease - ideal) / 1000000.0;
                } else {
                    offset_ms = (ideal - relativeRelease) / 1000000.0;
                }
            }
            list.append(offset_ms);
        }
    }
    return list;
}
