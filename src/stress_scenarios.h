#ifndef STRESS_SCENARIOS_H
#define STRESS_SCENARIOS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 5 S0-S6 */
typedef enum {
    SCENARIO_S0_BASELINE = 0,
    SCENARIO_S1_CPU_SAT,
    SCENARIO_S2_PRIO_INVERSION,
    SCENARIO_S3_BURST,
    SCENARIO_S4_STORM,
    SCENARIO_S5_MIXED,
    SCENARIO_S6_AFFINITY
} scenario_id_t;

const char* scenario_name(scenario_id_t id);
int scenario_run(scenario_id_t id, uint32_t duration_ms);
int scenario_run_all(void);
int scenario_run_load_sweep(uint32_t duration_ms); /* S1 20..95 */
int scenario_run_with_load(scenario_id_t id, int load_percent, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif
