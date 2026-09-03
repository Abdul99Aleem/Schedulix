#ifndef WORKLOAD_H
#define WORKLOAD_H

#include "workload_config.h"
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t task_id;
    char     name[32];
    pthread_t thread;
    pid_t    pid;
    pthread_t tid; /* actually pthread_self value */
    int      priority;
    int      cpu;
    const workload_config_t *config;

    _Atomic uint32_t activation_cnt; /* monotonic activation ID */
    _Atomic uint32_t pending_releases; /* event-driven queue depth */
    _Atomic uint32_t correlation_seq;  /* last corr id */
    _Atomic int      running;
    _Atomic int      should_stop;

    pthread_mutex_t lock;
    pthread_cond_t  cond;
    /* pending queue for correct READY correlation */
#define WORKLOAD_PENDING_MAX 16
    uint32_t pending_acts[WORKLOAD_PENDING_MAX];
    uint32_t pending_corrs[WORKLOAD_PENDING_MAX];
    uint64_t pending_release_ts[WORKLOAD_PENDING_MAX];
    _Atomic uint32_t pending_head;
    _Atomic uint32_t pending_tail;

    /* stats for Phase5 */
    uint64_t activations_started;
    uint64_t activations_finished;
    uint64_t deadline_misses;
} workload_context_t;

/* Global contexts (one per task) */
extern workload_context_t g_workloads[];
extern uint32_t g_workload_count;

int workload_init_all(void); /* create threads but not start */
int workload_start_all(void);
int workload_stop_all(void);
void workload_join_all(void);

/* Release mechanisms */
int workload_release_by_id(uint32_t task_id, uint32_t correlation_id); /* event-driven */
int workload_release_by_name(const char *name, uint32_t correlation_id);
workload_context_t* workload_by_id(uint32_t task_id);
workload_context_t* workload_by_name(const char *name);

/* Controlled CPU demand — not sleep */
void workload_busy_exec_ns(uint64_t target_ns);
uint64_t workload_busy_calibrate(void); /* returns loops per ms est */
int workload_set_target_exec(uint32_t task_id, uint64_t ns); /* for S1 saturation */
uint64_t workload_get_target_exec(uint32_t task_id);
int workload_set_affinity_all(int cpu); /* -1 any, 0..n pin to CPU for S1-B */
int workload_get_affinity(uint32_t task_id);

/* For analyzer: snapshot */
void workload_manifest_print(void);

#ifdef __cplusplus
}
#endif

#endif /* WORKLOAD_H */
