#define _POSIX_C_SOURCE 200809L
#include "manifest.h"
#include "trace_collector.h"
#include "workload_config.h"
#include "stress_generator.h"
#include "qnx_tracer.h"
#include "gpio_marker.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/utsname.h>
#if defined(__QNX__)
#include <sys/syspage.h>
#endif

static manifest_t g_man;

void manifest_collect(const char *scenario_id, const char *semantic_trace, const char *kernel_trace, uint64_t duration_ns){
    manifest_collect_full(scenario_id, -1, (uint32_t)(duration_ns/1000000ULL), semantic_trace, kernel_trace, duration_ns, "");
}
void manifest_collect_full(const char *scenario_id, int stress_level, uint32_t duration_ms, const char *semantic_trace, const char *kernel_trace, uint64_t duration_ns, const char *event_seq){
    memset(&g_man,0,sizeof(g_man));
    if(scenario_id) strncpy(g_man.scenario_id, scenario_id, sizeof(g_man.scenario_id)-1);
    if(semantic_trace) strncpy(g_man.semantic_trace_file, semantic_trace, sizeof(g_man.semantic_trace_file)-1);
    if(kernel_trace) strncpy(g_man.kernel_trace_file, kernel_trace, sizeof(g_man.kernel_trace_file)-1);
    strncpy(g_man.analysis_output_file, "analysis.json", sizeof(g_man.analysis_output_file)-1);
    if(event_seq) strncpy(g_man.event_sequence, event_seq, sizeof(g_man.event_sequence)-1);
    g_man.stress_level=stress_level;
    g_man.duration_ms=duration_ms;
    strncpy(g_man.version,"1.0.0",sizeof(g_man.version)-1);
    // run_id: timestamp-pid
    {
        struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
        snprintf(g_man.run_id,sizeof(g_man.run_id),"%lld-%d", (long long)ts.tv_sec, (int)getpid());
    }
    // QNX version via uname
    struct utsname u; if(uname(&u)==0){ snprintf(g_man.qnx_version,sizeof(g_man.qnx_version),"%s %s",u.sysname,u.release); }
    else strncpy(g_man.qnx_version,"unknown",sizeof(g_man.qnx_version)-1);
#if defined(__QNX__)
    FILE *f=fopen("/etc/qnx/version","r");
    if(f){ fgets(g_man.bsp_version,sizeof(g_man.bsp_version),f); fclose(f); g_man.bsp_version[strcspn(g_man.bsp_version,"\r\n")]=0; }
    else strncpy(g_man.bsp_version,"qnx800-rpi4",sizeof(g_man.bsp_version)-1);
#else
    strncpy(g_man.bsp_version,"host-mock",sizeof(g_man.bsp_version)-1);
#endif
    const trace_shm_header_t *hdr=trace_collector_header_ro();
    if(hdr){ g_man.cpu_count=hdr->cpu_count; g_man.sampling_mode=hdr->mode; g_man.sampling_n=hdr->sampling_n; g_man.trace_capacity=hdr->capacity; g_man.records_written=hdr->records_written; g_man.records_dropped=hdr->records_dropped;
        const char *modes[]={"FULL","SAMPLED","EVENT_ONLY"}; strncpy(g_man.trace_mode_str, modes[hdr->mode<3?hdr->mode:0], sizeof(g_man.trace_mode_str)-1);
    } else { strncpy(g_man.trace_mode_str,"FULL",sizeof(g_man.trace_mode_str)-1); }
    /* Fix cpu_count 0 on Pi: hardcode 4 for RPi4 (pidin info shows 4) */
    if(g_man.cpu_count==0){
        g_man.cpu_count=4;
    }
    // cpu affinity from workload table
    {
        char aff[64]=""; for(uint32_t i=0;i<g_workload_table_size;i++){ char tmp[16]; if(g_workload_table[i].cpu_affinity==0xFF) snprintf(tmp,sizeof(tmp),"%s:any ",g_workload_table[i].name); else snprintf(tmp,sizeof(tmp),"%s:%u ",g_workload_table[i].name,g_workload_table[i].cpu_affinity); strncat(aff,tmp,sizeof(aff)-strlen(aff)-1); }
        strncpy(g_man.cpu_affinity, aff, sizeof(g_man.cpu_affinity)-1);
    }
    // stress config via stress_generator if active
    stress_generator_describe(g_man.stress_config,sizeof(g_man.stress_config));
    g_man.trace_duration_ns=duration_ns;
    // kernel provenance
    {
        qnx_provenance_t prov={0};
        qnx_tracer_get_provenance(&prov);
        snprintf(g_man.kernel_provenance,sizeof(g_man.kernel_provenance),"size=%llu generated=%d privileged=%d err=%s",
            (unsigned long long)prov.file_size, prov.generated_by_current_run, prov.privileged, prov.error);
        if(!prov.privileged || prov.file_size==0){
            // keep FAILED marker already in kernel_trace_file
        }
    }
    // gpio mode
    snprintf(g_man.gpio_mode,sizeof(g_man.gpio_mode),"%s", gpio_marker_is_available()?"real":"mock");
}

int manifest_write_json(const char *path){
    if(!path) path="manifest.json";
    FILE *f=fopen(path,"w"); if(!f) return -1;
    fprintf(f,"{\n");
    fprintf(f,"  \"run_id\": \"%s\",\n", g_man.run_id);
    fprintf(f,"  \"version\": \"%s\",\n", g_man.version);
    fprintf(f,"  \"scenario\": \"%s\",\n", g_man.scenario_id);
    fprintf(f,"  \"stress_level\": %d,\n", g_man.stress_level);
    fprintf(f,"  \"duration_ms\": %u,\n", g_man.duration_ms);
    fprintf(f,"  \"qnx_version\": \"%s\",\n", g_man.qnx_version);
    fprintf(f,"  \"bsp_version\": \"%s\",\n", g_man.bsp_version);
    fprintf(f,"  \"cpu_count\": %u,\n", g_man.cpu_count);
    fprintf(f,"  \"cpu_affinity\": \"%s\",\n", g_man.cpu_affinity);
    fprintf(f,"  \"stress_config\": \"%s\",\n", g_man.stress_config);
    fprintf(f,"  \"event_sequence\": \"%s\",\n", g_man.event_sequence);
    fprintf(f,"  \"trace_mode\": \"%s\",\n", g_man.trace_mode_str);
    fprintf(f,"  \"tasks\": [\n");
    for(uint32_t i=0;i<g_workload_table_size;i++){
        const workload_config_t *c=&g_workload_table[i];
        fprintf(f,"    {\"task_id\":%u,\"name\":\"%s\",\"priority\":%d,\"period_ns\":%llu,\"deadline_ns\":%llu,\"target_exec_ns\":%llu,\"criticality\":%u,\"cpu_affinity\":%u},\n",
            c->task_id,c->name,c->priority,(unsigned long long)c->period_ns,(unsigned long long)c->deadline_ns,(unsigned long long)c->target_exec_ns,c->criticality,c->cpu_affinity);
    }
    // remove last comma by overwriting
    fprintf(f,"    {\"note\":\"end\"}\n");
    fprintf(f,"  ],\n");
    fprintf(f,"  \"priorities\": \"BRAKE20 ADAS15 DIAG10\",\n");
    fprintf(f,"  \"periods\": \"BRAKE10ms ADAS20ms DIAG50ms\",\n");
    fprintf(f,"  \"deadlines\": \"BRAKE10ms ADAS20ms DIAG50ms\",\n");
    fprintf(f,"  \"trace\": {\"capacity\":%u,\"mode\":%u,\"sampling_n\":%u,\"duration_ns\":%llu,\"records\":%llu,\"dropped_records\":%llu},\n",
        g_man.trace_capacity,g_man.sampling_mode,g_man.sampling_n,(unsigned long long)g_man.trace_duration_ns,(unsigned long long)g_man.records_written,(unsigned long long)g_man.records_dropped);
    fprintf(f,"  \"kernel_provenance\": \"%s\",\n", g_man.kernel_provenance);
    fprintf(f,"  \"gpio_mode\": \"%s\",\n", g_man.gpio_mode);
    fprintf(f,"  \"files\": {\"semantic\":\"%s\",\"kernel\":\"%s\",\"analysis\":\"%s\"}\n", g_man.semantic_trace_file,g_man.kernel_trace_file,g_man.analysis_output_file);
    fprintf(f,"}\n");
    fclose(f); return 0;
}
int manifest_write_text(const char *path){
    if(!path) path="manifest.txt";
    FILE *f=fopen(path,"w"); if(!f) return -1;
    fprintf(f,"scenario_id: %s\n",g_man.scenario_id);
    fprintf(f,"qnx_version: %s\n",g_man.qnx_version);
    fprintf(f,"bsp_version: %s\n",g_man.bsp_version);
    fprintf(f,"cpu_count: %u\n",g_man.cpu_count);
    for(uint32_t i=0;i<g_workload_table_size;i++){
        const workload_config_t *c=&g_workload_table[i];
        fprintf(f,"task %s prio %d period %llu deadline %llu exec %llu crit %u cpu %u\n",
            c->name,c->priority,(unsigned long long)c->period_ns,(unsigned long long)c->deadline_ns,(unsigned long long)c->target_exec_ns,c->criticality,c->cpu_affinity);
    }
    fprintf(f,"trace cap %u mode %u n %u dur %llu rec %llu dropped %llu\n",g_man.trace_capacity,g_man.sampling_mode,g_man.sampling_n,(unsigned long long)g_man.trace_duration_ns,(unsigned long long)g_man.records_written,(unsigned long long)g_man.records_dropped);
    fprintf(f,"semantic %s kernel %s\n",g_man.semantic_trace_file,g_man.kernel_trace_file);
    fclose(f); return 0;
}
const manifest_t* manifest_get(void){ return &g_man; }
