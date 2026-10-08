#define _POSIX_C_SOURCE 200809L
#include "workload.h"
#include "workload_config.h"
#include "trace_instrumentation.h"
#include "trace_schema.h"
#include "can_frame.h"
#include "gpio_marker.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>

#if defined(__QNX__)
#include <sys/neutrino.h>
#include <pthread.h>
#endif

workload_context_t g_workloads[8];
uint32_t g_workload_count = 0;
static uint64_t g_exec_overrides[8] = {0}; /* 0 = use config */
static int g_affinity_overrides[8]; /* -1 = any, 0..3 pin */
static int g_affinity_inited = 0;
static void workload_affinity_init(void){
    if(!g_affinity_inited){ for(int i=0;i<8;i++) g_affinity_overrides[i]=-1; g_affinity_inited=1; }
}
static int set_cpu_affinity(int cpu){
    if(cpu<0) return 0;
#if defined(__QNX__)
    unsigned mask = (1u << cpu);
    // QNX: pin current thread to CPU
    if(ThreadCtl(_NTO_TCTL_RUNMASK, &mask)!=0) return -1;
    return 0;
#elif defined(__linux__)
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu,&set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    return 0;
#endif
}

static uint64_t mono_ns(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec;
}

void workload_busy_exec_ns(uint64_t target_ns){
    uint64_t start = mono_ns();
    volatile uint64_t dummy=0;
    while (mono_ns() - start < target_ns) {
        for (int i=0;i<100;i++) dummy += (dummy * 31 + 17) & 0xFFFF;
    }
    (void)dummy;
}
uint64_t workload_busy_calibrate(void){ return 0; }

static void* workload_thread(void *arg){
    workload_context_t *ctx = (workload_context_t*)arg;
    const workload_config_t *cfg = ctx->config;
    workload_affinity_init();
    int aff = g_affinity_overrides[ctx - g_workloads];
    if(aff==-1 && cfg->cpu_affinity != CPU_AFFINITY_NONE) aff = cfg->cpu_affinity;
#if defined(__QNX__)
    struct sched_param sp; sp.sched_priority = cfg->priority;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    if(aff>=0) set_cpu_affinity(aff);
#else
    if(aff>=0) set_cpu_affinity(aff);
#endif
    ctx->pid = getpid();
    ctx->tid = pthread_self();
    uint64_t period = cfg->period_ns;
    uint64_t exec   = g_exec_overrides[ctx - g_workloads] ? g_exec_overrides[ctx - g_workloads] : cfg->target_exec_ns;
    uint64_t t0 = 0;
    uint32_t act_idx = 0;
    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    t0 = (uint64_t)start_ts.tv_sec * 1000000000ULL + start_ts.tv_nsec;

    while (!atomic_load(&ctx->should_stop)) {
        int has_release = 0;
        uint32_t act = 0;
        uint32_t corr = 0;
        uint64_t t_release = 0;
        if (period > 0) {
            uint64_t next_release_ns = t0 + (uint64_t)(act_idx + 1) * period;
            struct timespec ts;
            ts.tv_sec = next_release_ns / 1000000000ULL;
            ts.tv_nsec = next_release_ns % 1000000000ULL;
            pthread_mutex_lock(&ctx->lock);
            while (atomic_load(&ctx->pending_releases) == 0 && !atomic_load(&ctx->should_stop)) {
                int rc = pthread_cond_timedwait(&ctx->cond, &ctx->lock, &ts);
                if (rc == ETIMEDOUT) {
                    uint32_t pact = atomic_fetch_add(&ctx->activation_cnt, 1) + 1;
                    uint64_t now = mono_ns();
                    uint32_t tail = atomic_load(&ctx->pending_tail);
                    ctx->pending_acts[tail % WORKLOAD_PENDING_MAX] = pact;
                    ctx->pending_corrs[tail % WORKLOAD_PENDING_MAX] = pact;
                    ctx->pending_release_ts[tail % WORKLOAD_PENDING_MAX] = now;
                    atomic_store(&ctx->pending_tail, tail + 1);
                    atomic_fetch_add(&ctx->pending_releases, 1);
                    
                    uint64_t ideal_release = t0 + (uint64_t)act_idx * period;
                    uint64_t release_offset = now > ideal_release ? now - ideal_release : 0;
                    trace_emit(TRACE_WORKLOAD_RELEASE, ctx->task_id, pact, pact, ideal_release, release_offset);
                    /* READY deliberately not emitted here -- see the note at
                     * the dispatch point in the worker loop below. */
                    act_idx++;
                    has_release = 1;
                    break;
                }
            }
            if (atomic_load(&ctx->pending_releases) > 0) {
                uint32_t head = atomic_load(&ctx->pending_head);
                act = ctx->pending_acts[head % WORKLOAD_PENDING_MAX];
                corr = ctx->pending_corrs[head % WORKLOAD_PENDING_MAX];
                t_release = ctx->pending_release_ts[head % WORKLOAD_PENDING_MAX];
                atomic_store(&ctx->pending_head, head + 1);
                atomic_fetch_sub(&ctx->pending_releases, 1);
                has_release = 1;
                act_idx++;
            }
            pthread_mutex_unlock(&ctx->lock);
            if (!has_release && atomic_load(&ctx->should_stop)) break;
            if (!has_release) continue;
            // for periodic timeout path, act/corr/t_release already set via queue pop
            // for event-driven, they were set via pop as well
        } else {
            pthread_mutex_lock(&ctx->lock);
            while (atomic_load(&ctx->pending_releases)==0 && !atomic_load(&ctx->should_stop))
                pthread_cond_wait(&ctx->cond,&ctx->lock);
            if (atomic_load(&ctx->should_stop)) { pthread_mutex_unlock(&ctx->lock); break; }
            uint32_t head = atomic_load(&ctx->pending_head);
            act = ctx->pending_acts[head % WORKLOAD_PENDING_MAX];
            corr = ctx->pending_corrs[head % WORKLOAD_PENDING_MAX];
            t_release = ctx->pending_release_ts[head % WORKLOAD_PENDING_MAX];
            atomic_store(&ctx->pending_head, head+1);
            atomic_fetch_sub(&ctx->pending_releases,1);
            pthread_mutex_unlock(&ctx->lock);
            has_release=1;
        }
        if (!has_release) continue;
        ctx->activations_started++;
        // t_release is from the queue (producer time), not now.
        uint64_t t_start = mono_ns();
        /* READY is stamped HERE, not at enqueue. It used to be emitted inside
         * the producer's lock, immediately after RELEASE, so ready_time was
         * effectively equal to release_time and the analyzer's
         * ready_wait = first_run - ready could never see ready-queue waiting.
         * That made the latency curve flat under load: same-core contention at
         * priority 20 still reported 0.00 ms ready wait and 0 deadline misses
         * while occupying 9 of every 10 ms. This is the dispatch point, so it
         * is the correct place to record that the task has become runnable. */
        trace_workload_ready(ctx->task_id, act, corr);
        trace_workload_start(ctx->task_id, act, corr);
        gpio_marker_for_task_high(ctx->task_id);

        // optional block simulation for S2 priority inversion
        // actual blocking via mutex would be here

        workload_busy_exec_ns(exec);

        gpio_marker_for_task_low(ctx->task_id);
        uint64_t t_end = mono_ns();
        uint64_t exec_ns = t_end - t_start;
        trace_workload_end(ctx->task_id, act, corr, exec_ns);
        gpio_validation_t gv = gpio_marker_validate(ctx->task_id, t_start, t_end);
        if (!gv.valid) {
            // emit as analysis note (tolerance 50us + 5%)
            trace_emit(TRACE_PREEMPTION, ctx->task_id, act, corr, (uint64_t)gv.delta_ns, exec_ns);
        }

        int64_t response = (int64_t)(t_end - t_release);
        int64_t slack = (int64_t)cfg->deadline_ns - response;
        trace_workload_deadline(ctx->task_id, act, corr, slack, cfg->deadline_ns);
        if (slack < 0) { ctx->deadline_misses++; }
        ctx->activations_finished++;
    }
    return NULL;
}

int workload_init_all(void){
    workload_affinity_init();
    g_workload_count = g_workload_table_size;
    if (g_workload_count>8) g_workload_count=8;
    for (uint32_t i=0;i<g_workload_count;i++){
        workload_context_t *c=&g_workloads[i];
        memset(c,0,sizeof(*c));
        c->config=&g_workload_table[i];
        c->task_id=c->config->task_id;
        strncpy(c->name,c->config->name,sizeof(c->name)-1);
        c->priority=c->config->priority;
        pthread_mutex_init(&c->lock,NULL);
        pthread_cond_init(&c->cond,NULL);
        atomic_store(&c->activation_cnt,0);
        atomic_store(&c->pending_releases,0);
        atomic_store(&c->pending_head,0);
        atomic_store(&c->pending_tail,0);
        atomic_store(&c->running,0);
        atomic_store(&c->should_stop,0);
    }
    return 0;
}
int workload_start_all(void){
    for (uint32_t i=0;i<g_workload_count;i++){
        workload_context_t *c=&g_workloads[i];
        if (c->config->task_id==TASK_ID_IDLE) continue; /* not auto started */
        pthread_attr_t attr; pthread_attr_init(&attr);
        // QNX: set prio via attr if desired
        int rc=pthread_create(&c->thread,NULL,workload_thread,c);
        pthread_attr_destroy(&attr);
        if (rc!=0) return -1;
        atomic_store(&c->running,1);
    }
    return 0;
}
int workload_stop_all(void){
    for (uint32_t i=0;i<g_workload_count;i++){
        workload_context_t *c=&g_workloads[i];
        atomic_store(&c->should_stop,1);
        pthread_mutex_lock(&c->lock);
        pthread_cond_broadcast(&c->cond);
        pthread_mutex_unlock(&c->lock);
    }
    return 0;
}
void workload_join_all(void){
    for (uint32_t i=0;i<g_workload_count;i++){
        workload_context_t *c=&g_workloads[i];
        if (atomic_load(&c->running)){
            pthread_join(c->thread,NULL);
            atomic_store(&c->running,0);
        }
        pthread_mutex_destroy(&c->lock);
        pthread_cond_destroy(&c->cond);
    }
}
workload_context_t* workload_by_id(uint32_t id){
    for(uint32_t i=0;i<g_workload_count;i++) if(g_workloads[i].task_id==id) return &g_workloads[i];
    return NULL;
}
workload_context_t* workload_by_name(const char *name){
    for(uint32_t i=0;i<g_workload_count;i++) if(strcmp(g_workloads[i].name,name)==0) return &g_workloads[i];
    return NULL;
}
int workload_release_by_id(uint32_t task_id, uint32_t corr){
    workload_context_t *c=workload_by_id(task_id); if(!c) return -1;
    uint32_t act = atomic_fetch_add(&c->activation_cnt,1)+1;
    uint64_t now = mono_ns();
    pthread_mutex_lock(&c->lock);
    uint32_t tail = atomic_load(&c->pending_tail);
    uint32_t head = atomic_load(&c->pending_head);
    if((tail+1)%WORKLOAD_PENDING_MAX == head % WORKLOAD_PENDING_MAX){
        pthread_mutex_unlock(&c->lock);
        return -1;
    }
    c->pending_acts[tail % WORKLOAD_PENDING_MAX] = act;
    c->pending_corrs[tail % WORKLOAD_PENDING_MAX] = corr;
    c->pending_release_ts[tail % WORKLOAD_PENDING_MAX] = now;
    atomic_store(&c->pending_tail, tail+1);
    atomic_fetch_add(&c->pending_releases,1);
    trace_workload_release(c->task_id, act, corr, c->config->deadline_ns);
    /* READY deliberately not emitted here -- see the note at the dispatch
     * point in the worker loop. */
    pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->lock);
    return 0;
}
int workload_release_by_name(const char *name, uint32_t corr){ workload_context_t *c=workload_by_name(name); return c? workload_release_by_id(c->task_id,corr):-1; }
int workload_set_target_exec(uint32_t task_id, uint64_t ns){
    for(uint32_t i=0;i<g_workload_count;i++) if(g_workloads[i].task_id==task_id){ g_exec_overrides[i]=ns; return 0; }
    for(uint32_t i=0;i<g_workload_table_size;i++) if(g_workload_table[i].task_id==task_id){ g_exec_overrides[i]=ns; return 0; }
    return -1;
}
uint64_t workload_get_target_exec(uint32_t task_id){
    for(uint32_t i=0;i<g_workload_count;i++) if(g_workloads[i].task_id==task_id) return g_exec_overrides[i]? g_exec_overrides[i]: g_workloads[i].config->target_exec_ns;
    const workload_config_t *c=workload_config_by_id(task_id); return c? c->target_exec_ns:0;
}
int workload_set_affinity_all(int cpu){
    workload_affinity_init();
    for(int i=0;i<8;i++) g_affinity_overrides[i]=cpu;
    return 0;
}
int workload_get_affinity(uint32_t task_id){
    for(uint32_t i=0;i<g_workload_count;i++) if(g_workloads[i].task_id==task_id) return g_affinity_overrides[i];
    return -1;
}
void workload_manifest_print(void){
    printf("Task       Priority  Period  Deadline  Target Exec  Affinity\n");
    printf("------------------------------------------------------------\n");
    for(uint32_t i=0;i<g_workload_count;i++){
        const workload_config_t *cfg=g_workloads[i].config;
        if(cfg->task_id==TASK_ID_IDLE) continue;
        uint64_t exec = g_exec_overrides[i]? g_exec_overrides[i]: cfg->target_exec_ns;
        int aff = g_affinity_overrides[i];
        if(aff==-1 && cfg->cpu_affinity!=CPU_AFFINITY_NONE) aff=cfg->cpu_affinity;
        const char *affs = (aff==-1)?"any": (aff==0?"CPU0":aff==1?"CPU1":aff==2?"CPU2":"CPU3");
        printf("%-8s %8d %7.1fms %8.1fms %8.1fms %-6s%s\n", cfg->name, cfg->priority, cfg->period_ns/1e6, cfg->deadline_ns/1e6, exec/1e6, affs, g_exec_overrides[i]?"*":"");
    }
}
