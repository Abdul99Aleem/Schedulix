#include "workload_config.h"
#include <string.h>
#include <stdio.h>

const workload_config_t g_workload_table[] = {
    {
        .task_id = TASK_ID_BRAKE,
        .name = "BRAKE",
        .priority = 20,
        .period_ns = 10ULL * 1000000ULL,   /* 10 ms */
        .deadline_ns = 10ULL * 1000000ULL,
        .target_exec_ns = 2ULL * 1000000ULL, /* 2 ms CPU demand */
        .criticality = CRIT_HIGH,
        .cpu_affinity = CPU_AFFINITY_NONE
    },
    {
        .task_id = TASK_ID_ADAS,
        .name = "ADAS",
        .priority = 15,
        .period_ns = 20ULL * 1000000ULL,
        .deadline_ns = 20ULL * 1000000ULL,
        .target_exec_ns = 8ULL * 1000000ULL, /* 8 ms */
        .criticality = CRIT_MEDIUM,
        .cpu_affinity = CPU_AFFINITY_NONE
    },
    {
        .task_id = TASK_ID_DIAG,
        .name = "DIAG",
        .priority = 10,
        .period_ns = 50ULL * 1000000ULL,
        .deadline_ns = 50ULL * 1000000ULL,
        .target_exec_ns = 5ULL * 1000000ULL, /* 5 ms */
        .criticality = CRIT_LOW,
        .cpu_affinity = CPU_AFFINITY_NONE
    },
    {
        .task_id = TASK_ID_IDLE,
        .name = "IDLE_LOAD",
        .priority = 5,
        .period_ns = 0, /* aperiodic, used for S1 saturation */
        .deadline_ns = 0,
        .target_exec_ns = 1ULL * 1000000ULL,
        .criticality = CRIT_LOW,
        .cpu_affinity = CPU_AFFINITY_NONE
    }
};

const uint32_t g_workload_table_size = sizeof(g_workload_table)/sizeof(g_workload_table[0]);

const workload_config_t* workload_config_by_id(uint32_t task_id) {
    for (uint32_t i=0;i<g_workload_table_size;i++) if (g_workload_table[i].task_id==task_id) return &g_workload_table[i];
    return NULL;
}
const workload_config_t* workload_config_by_name(const char *name) {
    if (!name) return NULL;
    for (uint32_t i=0;i<g_workload_table_size;i++) if (strcmp(g_workload_table[i].name,name)==0) return &g_workload_table[i];
    return NULL;
}
void workload_config_print_table(void) {
    printf("Task       Priority  Period  Deadline  Target Exec  Crit  CPU\n");
    printf("------------------------------------------------------------\n");
    for (uint32_t i=0;i<g_workload_table_size;i++) {
        const workload_config_t *c=&g_workload_table[i];
        if (c->task_id==TASK_ID_IDLE) continue; /* hide synthetic */
        printf("%-8s %8d %7.1fms %8.1fms %8.1fms %5u  %s\n",
            c->name, c->priority,
            c->period_ns/1e6, c->deadline_ns/1e6, c->target_exec_ns/1e6,
            (unsigned)c->criticality,
            c->cpu_affinity==CPU_AFFINITY_NONE?"any":"pinned");
    }
}
