#define _POSIX_C_SOURCE 200809L
#include "trace_collector.h"
#include "can_frame.h" /* for monotonic helper, optional */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>

#if defined(__QNX__)
#include <sys/neutrino.h>
#include <sys/syspage.h>
#endif

static trace_shm_header_t *g_hdr = NULL;
static sched_trace_record_t *g_buf = NULL;
static size_t g_shm_size = 0;
static int    g_fd = -1;
static char   g_name[128] = {0};
static int    g_heap = 0;

static int need_drop(trace_mode_t mode, uint32_t n, const sched_trace_record_t *rec) {
    (void)n;
    if (mode == TRACE_MODE_FULL) return 0;
    if (mode == TRACE_MODE_EVENT_ONLY) {
        /* GPIO markers are kept: they are the instrument-validation signal,
         * so dropping them in EVENT_ONLY mode would defeat their purpose. */
        return !(rec->event_type == TRACE_EXTERNAL_EVENT_RX ||
                 rec->event_type == TRACE_EVENT_DECODED ||
                 rec->event_type == TRACE_WORKLOAD_DEADLINE_MISS ||
                 rec->event_type == TRACE_WORKLOAD_DEADLINE ||
                 rec->event_type == TRACE_GPIO_MARKER_HIGH ||
                 rec->event_type == TRACE_GPIO_MARKER_LOW);
    }
    if (mode == TRACE_MODE_SAMPLED) {
        if (n==0) return 0;
        return (rec->activation_id % n) != 0;
    }
    return 0;
}

int trace_collector_init(const char *shm_name, uint32_t capacity, trace_mode_t mode, uint32_t sampling_n) {
    if (g_hdr) return 0;
    if (capacity==0) capacity=8192;
    if (!shm_name) shm_name=TRACE_SHM_DEFAULT_NAME;
    strncpy(g_name, shm_name, sizeof(g_name)-1);
    g_shm_size = sizeof(trace_shm_header_t) + (size_t)capacity * sizeof(sched_trace_record_t);
    g_fd = shm_open(g_name, O_CREAT | O_RDWR, 0666);
    if (g_fd < 0) {
        // fallback to heap
        return trace_collector_init_heap(capacity, mode, sampling_n);
    }
    if (ftruncate(g_fd, g_shm_size) != 0) { close(g_fd); g_fd=-1; return -1; }
    void *addr = mmap(NULL, g_shm_size, PROT_READ|PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (addr==MAP_FAILED) { close(g_fd); g_fd=-1; return -1; }
    g_hdr = (trace_shm_header_t*)addr;
    g_buf = (sched_trace_record_t*)((char*)addr + sizeof(trace_shm_header_t));
    memset(g_hdr, 0, sizeof(*g_hdr));
    memset(g_buf, 0, capacity * sizeof(sched_trace_record_t));
    g_hdr->magic = TRACE_SHM_MAGIC;
    g_hdr->version = TRACE_SHM_VERSION;
    g_hdr->record_size = sizeof(sched_trace_record_t);
    g_hdr->capacity = capacity;
    g_hdr->mode = mode;
    g_hdr->sampling_n = sampling_n;
    g_hdr->start_time_ns = 0;
    g_hdr->cpu_count = 0;
    /* Robust CPU count for QNX RPi4: hardcode 4 (Pi 4 Cortex-A72 x4), verified via pidin info */
#if defined(__QNX__)
    g_hdr->cpu_count = 4;
#elif defined(_SC_NPROCESSORS_ONLN)
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus>0) g_hdr->cpu_count = (uint32_t)cpus;
#endif
    return 0;
}

int trace_collector_init_heap(uint32_t capacity, trace_mode_t mode, uint32_t sampling_n) {
    if (g_hdr) return 0;
    if (capacity==0) capacity=8192;
    g_shm_size = sizeof(trace_shm_header_t) + (size_t)capacity * sizeof(sched_trace_record_t);
    void *addr = calloc(1, g_shm_size);
    if (!addr) return -1;
    g_heap = 1;
    g_hdr = (trace_shm_header_t*)addr;
    g_buf = (sched_trace_record_t*)((char*)addr + sizeof(trace_shm_header_t));
    g_hdr->magic = TRACE_SHM_MAGIC;
    g_hdr->version = TRACE_SHM_VERSION;
    g_hdr->record_size = sizeof(sched_trace_record_t);
    g_hdr->capacity = capacity;
    g_hdr->mode = mode;
    g_hdr->sampling_n = sampling_n;
    g_hdr->start_time_ns = 0;
#if defined(__QNX__)
    g_hdr->cpu_count = 4;
#endif
    return 0;
}

void trace_collector_shutdown(void) {
    if (!g_hdr) return;
    if (g_heap) {
        free(g_hdr);
    } else {
        munmap(g_hdr, g_shm_size);
        if (g_fd>=0) { close(g_fd); shm_unlink(g_name); }
    }
    g_hdr=NULL; g_buf=NULL; g_fd=-1; g_shm_size=0; g_heap=0;
}

int trace_collector_is_initialized(void){ return g_hdr!=NULL; }

int trace_collector_record(const sched_trace_record_t *rec) {
    if (!g_hdr || !rec) return -2;
    if (need_drop((trace_mode_t)g_hdr->mode, g_hdr->sampling_n, rec)) return 0; /* sampled out, not dropped */
    // lock-free write_index bump
    uint32_t idx = atomic_fetch_add((_Atomic uint32_t*)&g_hdr->write_index, 1);
    uint32_t cap = g_hdr->capacity;
    uint32_t slot = idx % cap;
    // overflow detection: if write - read > capacity, we drop
    uint32_t r = atomic_load((_Atomic uint32_t*)&g_hdr->read_index);
    // approximate used
    uint32_t used = (idx >= r) ? (idx - r) : 0;
    if (used >= cap) {
        atomic_fetch_add((_Atomic uint64_t*)&g_hdr->records_dropped, 1);
        // revert write? we already bumped, but slot overwritten — count as drop
        return -1;
    }
    
    // Copy the payload first without writing sequence yet
    sched_trace_record_t slot_rec = *rec;
    slot_rec.sequence = 0; // provisional state
    g_buf[slot] = slot_rec;
    
    // Memory release barrier to ensure copy-writing is visible to consumer first
    atomic_thread_fence(memory_order_release);
    
    // Publish the slot
    g_buf[slot].sequence = idx + 1;

    atomic_fetch_add((_Atomic uint64_t*)&g_hdr->records_written, 1);
    if (used+1 > g_hdr->high_watermark) atomic_store((_Atomic uint32_t*)&g_hdr->high_watermark, used+1);
    return 0;
}

int trace_collector_record_simple(uint32_t event_type, uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t arg0, uint64_t arg1) {
    sched_trace_record_t rec;
    memset(&rec,0,sizeof(rec));
    // timestamp via monotonic
#if defined(_WIN32)
    // fallback: use can_frame helper if available, else 0
    rec.timestamp_ns = 0;
#else
    // use clock_gettime
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC,&ts)==0) rec.timestamp_ns = (uint64_t)ts.tv_sec*1000000000ULL + ts.tv_nsec;
#endif
    rec.event_type = event_type;
    rec.task_id = task_id;
    rec.activation_id = activation_id;
    rec.correlation_id = corr_id;
    rec.arg0 = arg0;
    rec.arg1 = arg1;
    // cpu/prio fill if possible — real SMP attribution
    rec.cpu = 0xFFFF;
    rec.priority = 0;
#if defined(__QNX__)
    unsigned int c = SchedGetCpuNum();
    if (c < 256) rec.cpu = (uint16_t)c;
#elif defined(__linux__)
    int c = sched_getcpu();
    if (c >= 0) rec.cpu = (uint16_t)c;
#endif
    return trace_collector_record(&rec);
}

int trace_collector_flush(const char *path) {
    if (!g_hdr || !path) return -1;
    FILE *f = fopen(path,"wb");
    if (!f) return -1;
    // write header + all written records up to capacity
    uint32_t w = atomic_load((_Atomic uint32_t*)&g_hdr->write_index);
    uint32_t r = atomic_load((_Atomic uint32_t*)&g_hdr->read_index);
    uint32_t cap = g_hdr->capacity;
    uint64_t total = (w>r)? (w-r) : 0;
    if (total > cap) total = cap;
    // write header copy
    trace_shm_header_t hdr = *g_hdr;
    fwrite(&hdr, sizeof(hdr), 1, f);
    for (uint64_t i=0;i<total;i++) {
        uint32_t curr_idx = r + (uint32_t)i;
        uint32_t slot = curr_idx % cap;
        // Acquire check: verify sequence matches index + 1
        if (g_buf[slot].sequence != curr_idx + 1) {
            // Producer hasn't finished writing this slot yet.
            // Stop to prevent reading partially written payload (head-of-line blocking).
            fprintf(stderr, "[Trace] Flush stopped at slot %u due to unpublished sequence %u (expected %u)\n",
                    slot, g_buf[slot].sequence, curr_idx + 1);
            break;
        }
        atomic_thread_fence(memory_order_acquire);
        fwrite(&g_buf[slot], sizeof(sched_trace_record_t), 1, f);
    }
    fclose(f);
    return 0;
}

int trace_collector_flush_range(const char *path, uint32_t max_records) { return trace_collector_flush(path); }

int trace_collector_snapshot(const char *path, uint32_t window) {
    if (!g_hdr || !path) return -1;
    FILE *f = fopen(path,"wb");
    if (!f) return -1;
    trace_shm_header_t hdr = *g_hdr;
    fwrite(&hdr,sizeof(hdr),1,f);
    uint32_t w = atomic_load((_Atomic uint32_t*)&g_hdr->write_index);
    uint32_t cap = g_hdr->capacity;
    uint32_t start = (w > window) ? (w - window) : 0;
    for (uint32_t i=start;i<w;i++) fwrite(&g_buf[i%cap],sizeof(sched_trace_record_t),1,f);
    fclose(f);
    return 0;
}

void trace_collector_get_header(trace_shm_header_t *out){ if(out && g_hdr) *out=*g_hdr; }
uint64_t trace_collector_dropped(void){ return g_hdr? atomic_load((_Atomic uint64_t*)&g_hdr->records_dropped):0; }
uint32_t trace_collector_high_watermark(void){ return g_hdr? atomic_load((_Atomic uint32_t*)&g_hdr->high_watermark):0; }
void trace_collector_set_mode(trace_mode_t mode, uint32_t n){ if(g_hdr){ g_hdr->mode=mode; g_hdr->sampling_n=n; } }
const trace_shm_header_t* trace_collector_header_ro(void){ return g_hdr; }
const sched_trace_record_t* trace_collector_records_ro(void){ return g_buf; }
