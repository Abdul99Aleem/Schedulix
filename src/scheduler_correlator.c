#include "scheduler_correlator.h"
#include <string.h>
#include <stdio.h>

static SchedulingFact g_facts[MAX_CORRELATOR_FACTS];
static size_t g_fact_count = 0;
static uint32_t g_cswitches = 0;
static uint32_t g_preemptions = 0;

/* CPU tracking states */
typedef struct {
    uint32_t current_pid;
    uint32_t current_tid;
    uint64_t last_switch_cycles;
} cpu_state_t;

static cpu_state_t g_cpus[MAX_CPUS];

/* Thread tracking states to track where threads are running */
typedef struct {
    uint32_t pid;
    uint32_t tid;
    uint32_t last_cpu;
    uint32_t last_state; /* 1=RUNNING, 2=READY, 3=BLOCKED, 4=DEAD */
    uint64_t state_change_ns;
    uint64_t total_running_ns;  /* Accumulated RUNNING time */
    uint64_t last_running_start_ns; /* When current RUNNING segment started */
} thread_state_t;

#define MAX_TRACKED_THREADS 2048
static thread_state_t g_threads[MAX_TRACKED_THREADS];
static size_t g_thread_count = 0;
static uint64_t g_trace_start_ns = 0;
static uint64_t g_trace_end_ns = 0;

static thread_state_t* find_or_create_thread(uint32_t pid, uint32_t tid) {
    for (size_t i = 0; i < g_thread_count; i++) {
        if (g_threads[i].pid == pid && g_threads[i].tid == tid) {
            return &g_threads[i];
        }
    }
    if (g_thread_count < MAX_TRACKED_THREADS) {
        thread_state_t *t = &g_threads[g_thread_count++];
        t->pid = pid;
        t->tid = tid;
        t->last_cpu = 0xFFFFu;
        t->last_state = 0;
        t->state_change_ns = 0;
        return t;
    }
    return NULL;
}

int scheduler_correlator_process(const KernelTraceEvent *events, size_t n) {
    g_fact_count = 0;
    g_cswitches = 0;
    g_preemptions = 0;
    g_thread_count = 0;
    g_trace_start_ns = 0;
    g_trace_end_ns = 0;
    memset(g_cpus, 0, sizeof(g_cpus));
    memset(g_threads, 0, sizeof(g_threads));

    for (size_t i = 0; i < n; i++) {
        const KernelTraceEvent *ev = &events[i];
        uint32_t cpu = ev->cpu;
        if (cpu >= MAX_CPUS) cpu = 0;

        /* Track trace time bounds */
        if (g_trace_start_ns == 0 || ev->timestamp_ns < g_trace_start_ns) {
            g_trace_start_ns = ev->timestamp_ns;
        }
        if (ev->timestamp_ns > g_trace_end_ns) {
            g_trace_end_ns = ev->timestamp_ns;
        }

        thread_state_t *t = find_or_create_thread(ev->pid, ev->tid);

        if (ev->event_type == 1 /* RUNNING */) {
            cpu_state_t *cpu_state = &g_cpus[cpu];
            uint32_t prev_pid = cpu_state->current_pid;
            uint32_t prev_tid = cpu_state->current_tid;

            /* Accumulate RUNNING time for the thread that was preempted */
            if (prev_tid != 0 && (prev_pid != ev->pid || prev_tid != ev->tid)) {
                thread_state_t *prev_t = find_or_create_thread(prev_pid, prev_tid);
                if (prev_t && prev_t->last_running_start_ns > 0) {
                    prev_t->total_running_ns += ev->timestamp_ns - prev_t->last_running_start_ns;
                    prev_t->last_running_start_ns = 0;
                }
            }

            if (prev_pid != ev->pid || prev_tid != ev->tid) {
                // If it is not the first owner of this CPU, count context switch
                if (prev_tid != 0) {
                    g_cswitches++;
                    thread_state_t *prev_t = find_or_create_thread(prev_pid, prev_tid);
                    if (prev_t) {
                        prev_t->last_state = 2; // Inferred READY by eviction
                        prev_t->state_change_ns = ev->timestamp_ns;
                    }
                }
                cpu_state->current_pid = ev->pid;
                cpu_state->current_tid = ev->tid;
                cpu_state->last_switch_cycles = ev->timestamp_cycles;

                if (g_fact_count < MAX_CORRELATOR_FACTS) {
                    SchedulingFact *f = &g_facts[g_fact_count++];
                    f->timestamp_ns = ev->timestamp_ns;
                    f->cpu = cpu;
                    f->fact_type = FACT_RUNNING;
                    f->pid = ev->pid;
                    f->tid = ev->tid;
                    f->prev_pid = prev_pid;
                    f->prev_tid = prev_tid;
                }
            }

            if (t) {
                t->last_cpu = cpu;
                t->last_state = 1; // RUNNING
                t->state_change_ns = ev->timestamp_ns;
                if (t->last_running_start_ns == 0) {
                    t->last_running_start_ns = ev->timestamp_ns;
                }
            }
        } 
        else if (ev->event_type == 2 /* READY */) {
            /* Accumulate RUNNING time for the thread that was preempted */
            if (t && t->last_state == 1 && t->last_running_start_ns > 0) {
                t->total_running_ns += ev->timestamp_ns - t->last_running_start_ns;
                t->last_running_start_ns = 0;
            }
            // Preemption verification: if it went from RUNNING directly to READY
            if (t && t->last_state == 1) {
                g_preemptions++;
                t->last_state = 2; // READY
                t->state_change_ns = ev->timestamp_ns;
            }

            if (g_fact_count < MAX_CORRELATOR_FACTS) {
                SchedulingFact *f = &g_facts[g_fact_count++];
                f->timestamp_ns = ev->timestamp_ns;
                f->cpu = cpu;
                f->fact_type = FACT_READY;
                f->pid = ev->pid;
                f->tid = ev->tid;
                f->prev_pid = 0;
                f->prev_tid = 0;
            }
        } 
        /* Real STATE_* values from <sys/states.h>: MUTEX=13, CONDVAR=14,
         * SEM=17. The previous literals (11, 12, 15) were SIGWAITINFO,
         * NANOSLEEP and JOIN -- so nanosleep between periodic activations was
         * misread as a mutex block, and genuine mutex blocking was missed. */
        else if (ev->event_type == 13 /* STATE_MUTEX */
              || ev->event_type == 14 /* STATE_CONDVAR */
              || ev->event_type == 17 /* STATE_SEM */) {
            /* Accumulate RUNNING time for the thread that is now blocking */
            if (t && t->last_state == 1 && t->last_running_start_ns > 0) {
                t->total_running_ns += ev->timestamp_ns - t->last_running_start_ns;
                t->last_running_start_ns = 0;
            }
            if (t) {
                t->last_state = 3; // BLOCKED
                t->state_change_ns = ev->timestamp_ns;
            }

            if (g_fact_count < MAX_CORRELATOR_FACTS) {
                SchedulingFact *f = &g_facts[g_fact_count++];
                f->timestamp_ns = ev->timestamp_ns;
                f->cpu = cpu;
                f->fact_type = FACT_BLOCKED;
                f->pid = ev->pid;
                f->tid = ev->tid;
                f->prev_pid = 0;
                f->prev_tid = 0;
            }
        } 
        else if (ev->event_type == 25 /* STATE_DESTROY; STATE_CREATE is 24 */) {
            /* Accumulate any remaining RUNNING time */
            if (t && t->last_state == 1 && t->last_running_start_ns > 0) {
                t->total_running_ns += ev->timestamp_ns - t->last_running_start_ns;
                t->last_running_start_ns = 0;
            }
            if (t) {
                t->last_state = 4; // DEAD
                t->state_change_ns = ev->timestamp_ns;
            }

            if (g_fact_count < MAX_CORRELATOR_FACTS) {
                SchedulingFact *f = &g_facts[g_fact_count++];
                f->timestamp_ns = ev->timestamp_ns;
                f->cpu = cpu;
                f->fact_type = FACT_TERMINATED;
                f->pid = ev->pid;
                f->tid = ev->tid;
                f->prev_pid = 0;
                f->prev_tid = 0;
            }
        }
    }

    /* Accumulate RUNNING time for threads still RUNNING at trace end */
    for (size_t i = 0; i < g_thread_count; i++) {
        thread_state_t *t = &g_threads[i];
        if (t->last_state == 1 && t->last_running_start_ns > 0) {
            t->total_running_ns += g_trace_end_ns - t->last_running_start_ns;
            t->last_running_start_ns = 0;
        }
    }

    return 0;
}

size_t scheduler_correlator_get_facts(SchedulingFact *out_facts, size_t max_facts) {
    size_t n = g_fact_count < max_facts ? g_fact_count : max_facts;
    if (out_facts && n > 0) {
        memcpy(out_facts, g_facts, n * sizeof(SchedulingFact));
    }
    return n;
}

uint32_t scheduler_correlator_get_cswitches(void) {
    return g_cswitches;
}

uint32_t scheduler_correlator_get_preemptions(void) {
    return g_preemptions;
}

int scheduler_correlator_find_preemption_overlap(uint32_t target_pid, uint32_t target_tid,
                                                uint64_t start_ns, uint64_t end_ns,
                                                uint32_t *interfering_pid, uint32_t *interfering_tid,
                                                uint64_t *overlap_start_ns, uint64_t *overlap_end_ns) {
    if (!interfering_pid || !interfering_tid || !overlap_start_ns || !overlap_end_ns) return -1;

    // Scan correlated facts to find if any other thread was RUNNING on the same CPU while the target was READY
    uint32_t target_cpu = 0xFFFFu;
    uint64_t ready_time = start_ns;

    // First find what CPU the target thread runs on
    for (size_t i = 0; i < g_fact_count; i++) {
        const SchedulingFact *f = &g_facts[i];
        if (f->pid == target_pid && f->tid == target_tid && f->fact_type == FACT_RUNNING) {
            target_cpu = f->cpu;
            break;
        }
    }
    if (target_cpu == 0xFFFFu) return -1; // no running fact found

    // Look for RUNNING facts of other threads on target_cpu during [ready_time, end_ns]
    for (size_t i = 0; i < g_fact_count; i++) {
        const SchedulingFact *f = &g_facts[i];
        if (f->cpu == target_cpu && f->fact_type == FACT_RUNNING) {
            if (f->pid != target_pid || f->tid != target_tid) {
                if (f->timestamp_ns >= ready_time && f->timestamp_ns < end_ns) {
                    *interfering_pid = f->pid;
                    *interfering_tid = f->tid;
                    *overlap_start_ns = f->timestamp_ns;
                    
                    // find when it stopped running or when target resumed
                    uint64_t next_t = end_ns;
                    for (size_t j = i + 1; j < g_fact_count; j++) {
                        const SchedulingFact *f2 = &g_facts[j];
                        if (f2->cpu == target_cpu && f2->fact_type == FACT_RUNNING) {
                            next_t = f2->timestamp_ns;
                            break;
                        }
                    }
                    *overlap_end_ns = next_t < end_ns ? next_t : end_ns;
                    return 0; // preemption found!
                }
            }
        }
    }

    return -1;
}

uint64_t scheduler_correlator_get_thread_running_ns(uint32_t pid, uint32_t tid) {
    for (size_t i = 0; i < g_thread_count; i++) {
        if (g_threads[i].pid == pid && g_threads[i].tid == tid) {
            return g_threads[i].total_running_ns;
        }
    }
    return 0;
}

uint64_t scheduler_correlator_get_trace_start_ns(void) {
    return g_trace_start_ns;
}

uint64_t scheduler_correlator_get_trace_end_ns(void) {
    return g_trace_end_ns;
}
