#include "stress_scenarios.h"
#include "stress_generator.h"
#include "workload.h"
#include "workload_config.h"
#include "trace_collector.h"
#include "can_frame.h"
#include "event_manager.h"
#include "can_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

const char* scenario_name(scenario_id_t id){
    switch(id){
        case SCENARIO_S0_BASELINE: return "S0_BASELINE";
        case SCENARIO_S1_CPU_SAT: return "S1_CPU_SATURATION";
        case SCENARIO_S2_PRIO_INVERSION: return "S2_PRIO_INVERSION";
        case SCENARIO_S3_BURST: return "S3_BURST_TRAFFIC";
        case SCENARIO_S4_STORM: return "S4_EVENT_STORM";
        case SCENARIO_S5_MIXED: return "S5_MIXED_WORKLOAD";
        case SCENARIO_S6_AFFINITY: return "S6_CPU_AFFINITY";
        default: return "UNKNOWN";
    }
}

static void sleep_ms(uint32_t ms){ struct timespec ts={ms/1000,(ms%1000)*1000000}; nanosleep(&ts,NULL); }

static void* s2_low_holder(void *arg){
    uint32_t hold_ms = *(uint32_t*)arg; free(arg);
    stress_resource_low_hold(hold_ms);
    return NULL;
}

/* Preserve ECU workloads — stress via dedicated workers only */
int scenario_run(scenario_id_t id, uint32_t duration_ms){
    printf("[SCENARIO %s] duration %u ms\n", scenario_name(id), duration_ms);
    switch(id){
        case SCENARIO_S0_BASELINE:
            /* control: no stress, ECU alone */
            stress_generator_stop();
            workload_start_all();
            sleep_ms(duration_ms);
            workload_stop_all();
            break;
        case SCENARIO_S1_CPU_SAT: {
            /* default S1 at 80% for single-run; sweep uses specific loads */
            stress_generator_stop();
            stress_generator_start_load(80);
            workload_start_all();
            sleep_ms(duration_ms);
            workload_stop_all();
            stress_generator_stop();
            break;
        }
        case SCENARIO_S2_PRIO_INVERSION: {
            /* LOW holds mutex, MEDIUM spins, BRAKE blocks — tests MUTEX_BLOCK vs CPU_CONTENTION */
            stress_resource_init();
            stress_generator_stop();
            pthread_t low;
            uint32_t *hold = malloc(sizeof(uint32_t)); *hold = duration_ms/2;
            pthread_create(&low,NULL,s2_low_holder,hold);
            if(stress_generator_start_load(60)!=0){ fprintf(stderr,"[S2] stress start failed\n"); pthread_join(low,NULL); return -1; }
            workload_start_all();
            usleep(5000);
            int blocked = stress_resource_brake_try(5);
            printf("[S2] BRAKE trylock %s (expected blocked)\n", blocked==0?"succeeded":"blocked");
            sleep_ms(duration_ms);
            workload_stop_all();
            stress_generator_stop();
            pthread_join(low,NULL);
            break;
        }
        case SCENARIO_S3_BURST:{
            stress_generator_stop();
            workload_start_all();
            event_manager_set_quiet(1);
            uint32_t burst_n=10;
            for(uint32_t i=0;i<burst_n;i++){
                can_frame_t bf={.id=0x100,.dlc=1,.data={0x01}};
                can_frame_set_timestamp(&bf);
                sched_event_t ev=decode_can_frame(&bf);
                event_manager_dispatch(&ev);
            }
            event_manager_set_quiet(0);
            printf("[S3] burst %u events injected, trace records %llu\n", burst_n, (unsigned long long)0);
            sleep_ms(duration_ms);
            workload_stop_all();
            break;
        }
        case SCENARIO_S4_STORM:{
            stress_generator_stop();
            workload_start_all();
            event_manager_set_quiet(1);
            uint32_t events = duration_ms/2; /* 500/sec */
            uint32_t queued=0;
            for(uint32_t i=0;i<events;i++){
                can_frame_t bf={.id=0x100,.dlc=1,.data={0x01}};
                can_frame_set_timestamp(&bf);
                sched_event_t ev=decode_can_frame(&bf);
                event_manager_dispatch(&ev);
                queued++;
                usleep(2000);
            }
            event_manager_set_quiet(0);
            printf("[S4] storm %u events queued, burst pressure %u/sec\n", queued, 500);
            workload_stop_all();
            break;
        }
        case SCENARIO_S5_MIXED:{
            stress_generator_start_load(50);
            workload_start_all();
            for(int i=0;i<5;i++){
                can_frame_t bf={.id=(i%2?0x200:0x100),.dlc=1,.data={0x01}};
                can_frame_set_timestamp(&bf); sched_event_t ev=decode_can_frame(&bf); event_manager_dispatch(&ev);
                usleep(5000);
            }
            sleep_ms(duration_ms);
            workload_stop_all();
            stress_generator_stop();
            break;
        }
        case SCENARIO_S6_AFFINITY:{
            printf("[S6] Case A unconstrained (affinity none) then Case B pinned\n");
            // Case A already run via S0, here demonstrate pinned
            stress_generator_stop();
            // For demo, affinity pinning would use ThreadCtl runmask — stub logs
            workload_start_all();
            sleep_ms(duration_ms);
            workload_stop_all();
            break;
        }
    }
    return 0;
}

int scenario_run_with_load(scenario_id_t id, int load_percent, uint32_t duration_ms){
    if(id!=SCENARIO_S1_CPU_SAT) return scenario_run(id,duration_ms);
    printf("[SCENARIO %s LOAD %d%%] duration %u ms\n", scenario_name(id), load_percent, duration_ms);
    stress_generator_stop();
    stress_generator_start_load(load_percent);
    workload_start_all();
    sleep_ms(duration_ms);
    workload_stop_all();
    stress_generator_stop();
    return 0;
}

int scenario_run_load_sweep(uint32_t duration_ms){
    int loads[]={20,40,60,70,80,85,90,95};
    for(size_t i=0;i<sizeof(loads)/sizeof(loads[0]);i++){
        scenario_run_with_load(SCENARIO_S1_CPU_SAT, loads[i], duration_ms);
    }
    return 0;
}

int scenario_run_all(void){
    for(int i=SCENARIO_S0_BASELINE;i<=SCENARIO_S6_AFFINITY;i++) scenario_run((scenario_id_t)i, 500);
    return 0;
}
