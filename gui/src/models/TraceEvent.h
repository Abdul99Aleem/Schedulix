#ifndef TRACEEVENT_H
#define TRACEEVENT_H

#include <QtGlobal>

// Raw fixed-size transport record (POD only, no pointers, dynamic arrays, or Qt containers)
struct TraceEvent {
    quint64 timestamp_ns;
    quint32 cpu;
    quint32 pid;
    quint32 tid;
    quint32 event_type;
    quint32 priority;
    quint32 task_id; // Numeric identifier mapping to names in the presentation model
    quint64 sequence_number;
};

// Derived analyzed representation
struct TaskTiming {
    quint64 release_ns;
    quint64 ready_ns;
    quint64 start_ns;
    quint64 finish_ns;
    quint64 deadline_ns;
    quint64 response_ns;
    bool deadline_miss;
    int preemption_count;
};

#endif // TRACEEVENT_H
