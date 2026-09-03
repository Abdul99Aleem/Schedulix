#ifndef WORKLOAD_CONFIG_H
#define WORKLOAD_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Task IDs — stable identity, not pthread_t */
#define TASK_ID_BRAKE  1u
#define TASK_ID_ADAS   2u
#define TASK_ID_DIAG   3u
#define TASK_ID_IDLE   4u  /* optional load generator */

typedef enum {
    CRIT_LOW    = 0,
    CRIT_MEDIUM = 1,
    CRIT_HIGH   = 2
} criticality_t;

#define CPU_AFFINITY_NONE 0xFFu

typedef struct {
    uint32_t task_id;
    char     name[32];

    int      priority;           /* QNX prio 1..255, higher = more important */

    uint64_t period_ns;          /* 0 = aperiodic / event-driven only */
    uint64_t deadline_ns;        /* relative deadline */
    uint64_t target_exec_ns;     /* deterministic CPU demand, not sleep */

    uint8_t  criticality;
    uint8_t  cpu_affinity;       /* 0..n or CPU_AFFINITY_NONE */
} workload_config_t;

/* Simulation parameters — not OEM policy claims */
extern const workload_config_t g_workload_table[];
extern const uint32_t          g_workload_table_size;

const workload_config_t* workload_config_by_id(uint32_t task_id);
const workload_config_t* workload_config_by_name(const char *name);
void workload_config_print_table(void);

#ifdef __cplusplus
}
#endif

#endif /* WORKLOAD_CONFIG_H */
