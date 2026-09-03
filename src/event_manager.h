#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include "sched_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Event Manager – deliverable #1 scope.
 * Today: prints/logs dispatch. No real QNX workload threads yet.
 * Tomorrow: can be swapped to dispatch to QNX threads / pulse / proxy
 * without touching decoder or can_frame_t.
 */

void event_manager_dispatch(const sched_event_t *event);
void event_manager_set_quiet(int quiet); /* 1 suppress per-event stdout */
int  event_manager_is_quiet(void);

/* Optional: statistics for later trace collector */
typedef struct {
    uint64_t brake_count;
    uint64_t adas_count;
    uint64_t diagnostic_count;
    uint64_t unknown_count;
    uint64_t total;
} event_manager_stats_t;

void event_manager_get_stats(event_manager_stats_t *out);
void event_manager_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_MANAGER_H */
