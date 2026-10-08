#define _POSIX_C_SOURCE 200809L
#include "stress_generator.h"
#include "trace_collector.h"
#include "trace_instrumentation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <errno.h>
#include <sched.h>

#if defined(__QNX__)
#include <sys/neutrino.h>
#endif

static stress_worker_config_t g_cfgs[STRESS_MAX_WORKERS];
static pthread_t g_threads[STRESS_MAX_WORKERS];
static _Atomic int g_running[STRESS_MAX_WORKERS];
static _Atomic int g_stop_all = 0;
static int g_n = 0;

/* S2 mutex */
static pthread_mutex_t g_res_mutex;
static int g_res_inited = 0;

static uint64_t mono_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec; }
static void busy_ms(uint32_t ms){
    uint64_t target = (uint64_t)ms*1000000ULL;
    uint64_t s=mono_ns(); volatile uint64_t d=0;
    while(mono_ns()-s < target){ for(int i=0;i<200;i++) d+= (d*31+17)&0xFFFF; }
    (void)d;
}

static int set_stress_affinity(int cpu){
    if(cpu<0) return 0;
#if defined(__QNX__)
    unsigned mask=(1u<<cpu);
    if(ThreadCtl(_NTO_TCTL_RUNMASK,&mask)!=0) return -1;
    return 0;
#elif defined(__linux__)
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu,&set);
    return pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
#else
    return 0;
#endif
}
static void* stress_worker(void *arg){
    int idx = -1;
    if(!arg) return NULL;
    idx = *(int*)arg; free(arg);
    if(idx<0 || idx>=STRESS_MAX_WORKERS) return NULL;
    stress_worker_config_t *cfg=&g_cfgs[idx];
    fprintf(stderr,"[STRESS] worker %d (%s) start prio %d exec %u period %u cpu %d\n", idx, cfg->name, cfg->priority, cfg->exec_ms, cfg->period_ms, cfg->cpu_affinity);
    if(cfg->cpu_affinity>=0) set_stress_affinity(cfg->cpu_affinity);
#if defined(__QNX__)
    struct sched_param sp; sp.sched_priority=cfg->priority;
    int rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if(rc!=0) fprintf(stderr,"[STRESS] setschedparam failed %d (continuing)\n", rc);
#endif
    uint32_t period_ms = cfg->period_ms ? cfg->period_ms : 10;
    uint32_t exec_ms = cfg->exec_ms;
    while(!atomic_load(&g_stop_all) && atomic_load(&g_running[idx])){
        uint64_t t0=mono_ns();
        busy_ms(exec_ms);
        uint64_t t1=mono_ns();
        /* trace as generic preemption source — guarded, no SchedGetCpuNum here to isolate */
        if(trace_collector_is_initialized()){
            // minimal trace without cpu query to isolate SchedGetCpuNum
            sched_trace_record_t rec={0};
            rec.timestamp_ns=t1;
            rec.event_type=12;
            rec.task_id=100+idx;
            rec.arg0=t1-t0;
            rec.arg1=cfg->priority;
            trace_collector_record(&rec);
        }
        uint64_t elapsed = (t1-t0)/1000000ULL;
        if(elapsed < period_ms){
            struct timespec ts={(period_ms-elapsed)/1000, ((period_ms-elapsed)%1000)*1000000};
            nanosleep(&ts,NULL);
        }
    }
    fprintf(stderr,"[STRESS] worker %d exit\n", idx);
    return NULL;
}

int stress_generator_init(void){
    if(!g_res_inited){ pthread_mutex_init(&g_res_mutex,NULL); g_res_inited=1; }
    memset(g_cfgs,0,sizeof(g_cfgs));
    memset(g_threads,0,sizeof(g_threads));
    for(int i=0;i<STRESS_MAX_WORKERS;i++) atomic_store(&g_running[i],0);
    atomic_store(&g_stop_all,0);
    g_n=0;
    return 0;
}

int stress_generator_start(const stress_worker_config_t *configs, int n){
    if(!configs || n<=0) return -1;
    if(n>STRESS_MAX_WORKERS) n=STRESS_MAX_WORKERS;
    stress_generator_stop();
    stress_generator_init();
    fprintf(stderr,"[STRESS] start %d workers\n", n);
    g_n=n;
    for(int i=0;i<n;i++){
        g_cfgs[i]=configs[i];
        atomic_store(&g_running[i],1);
        int *idx=malloc(sizeof(int));
        if(!idx) return -1;
        *idx=i;
        pthread_attr_t attr; pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 64*1024);
        int rc=pthread_create(&g_threads[i],&attr,stress_worker,idx);
        pthread_attr_destroy(&attr);
        if(rc!=0){ free(idx); fprintf(stderr,"[STRESS] pthread_create failed %d\n", rc); return -1; }
        fprintf(stderr,"[STRESS] created %s\n", g_cfgs[i].name);
    }
    return 0;
}

int stress_generator_start_load(int percent){
    if(percent<0) percent=0;
    if(percent>95) percent=95;
    uint32_t exec = (percent*10)/100;
    if(percent==95) exec=9;
    if(percent>0 && exec==0) exec=1;
    stress_worker_config_t cfg={0};
    snprintf(cfg.name,sizeof(cfg.name),"STRESS_%d",percent);
    cfg.priority=12; // S1-A system-wide: below ADAS, above DIAG
    cfg.cpu_affinity=-1;
    cfg.period_ms=10;
    cfg.exec_ms=exec;
    cfg.target_util=percent;
    if(percent==0) return stress_generator_stop();
    return stress_generator_start(&cfg,1);
}
int stress_generator_start_load_pinned(int percent, int cpu){
    if(percent<0) percent=0;
    if(percent>95) percent=95;
    uint32_t exec = (percent*10)/100;
    if(percent==95) exec=9;
    if(percent>0 && exec==0) exec=1;
    stress_worker_config_t cfg={0};
    snprintf(cfg.name,sizeof(cfg.name),"STRESS_%d_P%d",percent,cpu);
    cfg.priority=22; // S1-B same-CPU: above BRAKE (20) to genuinely preempt and force READY wait
    cfg.cpu_affinity=cpu;
    cfg.period_ms=10;
    cfg.exec_ms=exec;
    cfg.target_util=percent;
    if(percent==0) return stress_generator_stop();
    return stress_generator_start(&cfg,1);
}

int stress_generator_start_workers(int n, int base_prio, int period_ms, int exec_ms, int cpu_affinity){
    stress_worker_config_t cfgs[STRESS_MAX_WORKERS];
    for(int i=0;i<n && i<STRESS_MAX_WORKERS;i++){
        snprintf(cfgs[i].name,sizeof(cfgs[i].name),"STRESS_%d",i);
        cfgs[i].priority=base_prio+i;
        cfgs[i].cpu_affinity=cpu_affinity;
        cfgs[i].period_ms=period_ms;
        cfgs[i].exec_ms=exec_ms;
        cfgs[i].target_util= period_ms? (exec_ms*100/period_ms):0;
    }
    return stress_generator_start(cfgs,n);
}

int stress_generator_stop(void){
    atomic_store(&g_stop_all,1);
    for(int i=0;i<g_n;i++) atomic_store(&g_running[i],0);
    for(int i=0;i<g_n;i++) if(g_threads[i]){ pthread_join(g_threads[i],NULL); g_threads[i]=0; }
    atomic_store(&g_stop_all,0);
    g_n=0;
    return 0;
}
int stress_generator_active(void){ return g_n; }
int stress_generator_get_configs(stress_worker_config_t *out, int max_n){
    int n = g_n<max_n? g_n:max_n;
    for(int i=0;i<n;i++) out[i]=g_cfgs[i];
    return n;
}
int stress_generator_describe(char *buf, size_t len){
    if(!buf||len==0) return -1;
    buf[0]=0;
    for(int i=0;i<g_n;i++){
        char tmp[64];
        snprintf(tmp,sizeof(tmp),"%s(p%d %ums/%ums %u%% cpu%d) ",
            g_cfgs[i].name,g_cfgs[i].priority,g_cfgs[i].exec_ms,g_cfgs[i].period_ms,g_cfgs[i].target_util,g_cfgs[i].cpu_affinity);
        strncat(buf,tmp,len-strlen(buf)-1);
    }
    if(g_n==0) snprintf(buf,len,"none");
    return 0;
}
int stress_generator_utilization(void){
    if(g_n==0) return 0;
    return g_cfgs[0].target_util;
}
int stress_resource_init(void){ if(!g_res_inited){pthread_mutex_init(&g_res_mutex,NULL); g_res_inited=1;} return 0; }
int stress_resource_low_hold(uint32_t hold_ms){
    stress_resource_init();
    pthread_mutex_lock(&g_res_mutex);
    trace_emit(10, 99, 0, 0, hold_ms, 0); // BLOCK_BEGIN mock
    struct timespec ts={hold_ms/1000, (hold_ms%1000)*1000000}; nanosleep(&ts,NULL);
    pthread_mutex_unlock(&g_res_mutex);
    return 0;
}
int stress_resource_brake_try(uint32_t timeout_ms){
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    ts.tv_sec+=timeout_ms/1000; ts.tv_nsec+=(timeout_ms%1000)*1000000; if(ts.tv_nsec>=1000000000){ts.tv_sec++; ts.tv_nsec-=1000000000;}
    int rc=pthread_mutex_timedlock(&g_res_mutex,&ts);
    if(rc==0){ pthread_mutex_unlock(&g_res_mutex); return 0; }
    return -1; // blocked
}
void stress_resource_shutdown(void){ if(g_res_inited) pthread_mutex_destroy(&g_res_mutex); g_res_inited=0; }
