#include "gpio_marker.h"
#include "trace_instrumentation.h"
#include "workload_config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdatomic.h>

#if defined(__QNX__)
#include <sys/mman.h>
#include <sys/neutrino.h>
#endif

static int g_available = 0;
static int g_fd = -1;
static volatile uint32_t *g_gpio_base = NULL; /* BCM2711 GPIO base if mmap'd */
static uint64_t g_high_ts[16] = {0}; /* per pin */
static uint64_t g_low_ts[16] = {0};

/* BCM pin mapping for tasks */
static int task_to_pin(uint32_t task_id){
    switch(task_id){
        case TASK_ID_BRAKE: return 4;
        case TASK_ID_ADAS:  return 17;
        case TASK_ID_DIAG:  return 27;
        default: return -1;
    }
}
static int pin_to_idx(int pin){
    switch(pin){ case 4: return 0; case 17: return 1; case 27: return 2; default: return -1; }
}

static uint64_t mono_ns(void){
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec;
}

int gpio_marker_init(void){
    g_available = 0;
#if defined(__QNX__)
    /* Try QNX gpio driver first: /dev/gpio, /dev/gpio-0 */
    const char *paths[] = {"/dev/gpio-0","/dev/gpio","/dev/gpio1",NULL};
    for(int i=0;paths[i];i++){
        int fd = open(paths[i], O_RDWR);
        if(fd>=0){ g_fd=fd; g_available=1; fprintf(stderr,"[GPIO] opened %s\n",paths[i]); break; }
    }
    if(!g_available){
        /* Fallback: mmap BCM2711 GPIO physical base 0xFE200000 (QNX may require -o mem) */
        /* QNX: open /dev/mem */
        int mem = open("/dev/mem", O_RDWR|O_SYNC);
        if(mem>=0){
            void *map = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, mem, 0xFE200000);
            if(map!=MAP_FAILED){ g_gpio_base=(volatile uint32_t*)map; g_available=1; fprintf(stderr,"[GPIO] mmap BCM2711 0xFE200000\n"); }
            close(mem);
        }
    }
    if(!g_available) fprintf(stderr,"[GPIO] no HW, mock mode (validation uses sw timestamps)\n");
#else
    fprintf(stderr,"[GPIO] host mock mode\n");
#endif
    return 0;
}
void gpio_marker_shutdown(void){
    if(g_fd>=0) close(g_fd);
    if(g_gpio_base) munmap((void*)g_gpio_base,4096);
    g_fd=-1; g_gpio_base=NULL;
}
int gpio_marker_set(int bcm_pin, int value){
    int idx = pin_to_idx(bcm_pin);
    if(idx>=0){ if(value) g_high_ts[idx]=mono_ns(); else g_low_ts[idx]=mono_ns(); }
    if(!g_available) return -1; /* mock */
#if defined(__QNX__)
    if(g_gpio_base){
        /* BCM2711: GPSET0 offset 7, GPCLR0 offset 10, GPLEV etc. Simplified */
        /* Real BSP would configure GPFSEL first — stub sets only */
        volatile uint32_t *set = g_gpio_base + 7;
        volatile uint32_t *clr = g_gpio_base + 10;
        if(value) *set = (1u<<bcm_pin); else *clr = (1u<<bcm_pin);
        return 0;
    }
    if(g_fd>=0){
        /* ioctl stub — QNX gpio driver typically uses devctl */
        return 0;
    }
#endif
    return -1;
}
int gpio_marker_get(int bcm_pin){ (void)bcm_pin; return -1; }

void gpio_marker_for_task_high(uint32_t task_id){
    int pin=task_to_pin(task_id); if(pin<0) return;
    gpio_marker_set(pin,1);
}
void gpio_marker_for_task_low(uint32_t task_id){
    int pin=task_to_pin(task_id); if(pin<0) return;
    gpio_marker_set(pin,0);
}
int gpio_marker_trace_high(uint32_t task_id, uint32_t act, uint32_t corr){
    gpio_marker_for_task_high(task_id);
    return trace_emit(TRACE_CPU_MIGRATION, task_id, act, corr, 1, 0); /* reuse for GPIO marker */
}
int gpio_marker_trace_low(uint32_t task_id, uint32_t act, uint32_t corr){
    gpio_marker_for_task_low(task_id);
    return trace_emit(TRACE_CPU_MIGRATION, task_id, act, corr, 0, 0);
}
bool gpio_marker_is_available(void){ return g_available; }

gpio_validation_t gpio_marker_validate(uint32_t task_id, uint64_t sw_start, uint64_t sw_end){
    gpio_validation_t v={0};
    v.sw_start_ns=sw_start; v.sw_end_ns=sw_end;
    int idx=pin_to_idx(task_to_pin(task_id));
    if(idx>=0){ v.gpio_high_ns=g_high_ts[idx]; v.gpio_low_ns=g_low_ts[idx]; }
    uint64_t sw_dur = (sw_end>sw_start)? sw_end-sw_start:0;
    uint64_t gpio_dur = (v.gpio_low_ns>v.gpio_high_ns)? v.gpio_low_ns - v.gpio_high_ns:0;
    v.delta_ns = (int64_t)gpio_dur - (int64_t)sw_dur;
    /* Tolerance: 50us absolute + 5% relative per instrumentation spec */
    int64_t tol = 50000 + (int64_t)(sw_dur/20);
    int64_t absd = v.delta_ns<0? -v.delta_ns: v.delta_ns;
    v.valid = (absd <= tol);
    return v;
}
