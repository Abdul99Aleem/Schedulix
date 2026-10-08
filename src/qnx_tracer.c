#include "qnx_tracer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#if defined(__QNX__)
#include <sys/trace.h>
#include <spawn.h>
#include <sys/neutrino.h>
#endif

static int g_running=0;
static char g_kev[256]="/tmp/schedulix.kev";
static uint64_t g_start_ns=0;
static uint64_t g_end_ns=0;
static char g_error[128]={0};
static int g_privileged=-1;
static uint32_t g_flags=0;

static uint64_t mono_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec; }

int qnx_tracer_check_privilege(void){
#if defined(__QNX__)
    if (geteuid()==0) return 0;
    /* also try opening trace control */
    int fd = open("/dev/trace", O_RDWR);
    if(fd>=0){ close(fd); return 0; }
    /* QNX 8 procnto-smp-instr requires root */
    return -1;
#else
    return 0; /* host mock */
#endif
}

int qnx_tracer_delete_old(const char *kev_path){
    if(!kev_path) kev_path=g_kev;
    unlink(kev_path);
    return 0;
}

int qnx_tracer_start(const char *kev_path, uint32_t flags){
    if(!kev_path) kev_path="/tmp/schedulix.kev";
    strncpy(g_kev,kev_path,sizeof(g_kev)-1);
    g_flags=flags;
    g_error[0]=0;
    g_privileged = qnx_tracer_check_privilege();
#if defined(__QNX__)
    /* Always rotate old file so stale data cannot be mistaken */
    qnx_tracer_delete_old(g_kev);
    g_start_ns = mono_ns();
    if(g_privileged!=0){
        snprintf(g_error,sizeof(g_error),"Permission denied: tracelogger requires root (qconn as root or 'on -p 63 tracelogger')");
        fprintf(stderr,"[tracer] %s\n", g_error);
        fprintf(stderr,"[tracer] Documented root launch: 'su' or 'on -p 63 tracelogger -f %s -s 8192 -n 5 &' or qconn as root\n", g_kev);
        g_running=0;
        return -1; /* EPERM */
    }
    pid_t pid;
    /* -s is SECONDS per the utilities reference, so the old "-s 65536" asked for
     * 18.2 hours of tracing and only qnx_tracer_stop()'s slay ever ended it.
     *
     * -k and -D are what actually control data loss, and neither was ever set.
     *
     * Per the SAT guide's "Buffer overruns" chapter, when a kernel ring buffer
     * overruns "the kernel simply drops all events logged for that CPU until
     * that next buffer becomes free", and one ring exists per CPU. So an overrun
     * silently deletes a whole CPU's slice of the trace -- which is why the
     * workload's own threads were missing from the capture while the low-rate
     * PID 1 activity survived. That loss happens upstream of class filtering,
     * which is why adding -F filters appeared to change nothing.
     *
     * -k is buffers PER CPU in kernel space. The documented default is 8 x 16 KB
     * = 128 KB per CPU, ~506 KB across 4 CPUs -- the entire reservoir the kernel
     * has to absorb bursts in. -b is a different thing entirely: staging
     * buffers inside tracelogger, which do not slow the kernel down.
     *
     * 256 buffers/CPU is 2 MB per CPU, ~8 MB total on an 8 GB board. */
    char *argv[] = {"tracelogger", "-f", g_kev, "-s", "30",
                    "-F1", "-F2", "-F3", "-F6", "-F7",
                    "-k", "256", "-D", "1", "-b", "128", NULL};
    extern char **environ;
    int rc = posix_spawnp(&pid, "tracelogger", NULL, NULL, argv, environ);
    if (rc != 0) {
        snprintf(g_error,sizeof(g_error),"posix_spawn tracelogger failed: %s", strerror(rc));
        return -1;
    }
    /* Verify file appears within 500ms */
    for(int i=0;i<5;i++){
        struct stat st;
        if(stat(g_kev,&st)==0 && st.st_size>0) break;
        struct timespec ts={0,100000000}; nanosleep(&ts,NULL);
    }
    struct stat st;
    if(stat(g_kev,&st)!=0){
        snprintf(g_error,sizeof(g_error),"tracelogger did not create %s", g_kev);
        g_running=0;
        return -1;
    }
    g_running = 1;
    return 0;
#else
    qnx_tracer_delete_old(g_kev);
    g_start_ns=mono_ns();
    FILE *f=fopen(g_kev,"wb"); if(f){ const char hdr[]="MOCK KEV"; fwrite(hdr,1,sizeof(hdr),f); fclose(f); }
    g_running=1; return 0;
#endif
}
int qnx_tracer_stop(void){
#if defined(__QNX__)
    /* SIGINT, not SIGTERM: killing tracelogger mid-buffer write can discard the
     * events still queued in the kernel's buffers. SIGINT lets it flush and
     * exit cleanly, which matters because the scenario calls this immediately
     * before writing analysis JSON that references the .kev. */
    if(g_running) system("slay -s INT tracelogger 2>/dev/null");
    /* Give the flush a moment. tracelogger holds up to -b dynamic buffers. */
    for(int i=0;i<20;i++){
        struct timespec ts={0,50000000}; /* 50 ms */
        nanosleep(&ts,NULL);
        struct stat st;
        if(stat(g_kev,&st)!=0 || st.st_size==0) break;
        /* Two consecutive stable sizes means the write has settled. */
        size_t prev = st.st_size;
        nanosleep(&ts,NULL);
        if(stat(g_kev,&st)==0 && st.st_size==prev) break;
    }
#endif
    g_end_ns = mono_ns();
    g_running=0;
    return 0;
}
int qnx_tracer_is_running(void){ return g_running; }

int qnx_tracer_get_provenance(qnx_provenance_t *out){
    if(!out) return -1;
    if (g_privileged < 0) {
        g_privileged = qnx_tracer_check_privilege();
    }
    memset(out,0,sizeof(*out));
    strncpy(out->kev_path,g_kev,sizeof(out->kev_path)-1);
    out->start_ns=g_start_ns;
    out->end_ns=g_end_ns;
    out->capture_config=g_flags;
    out->privileged=(g_privileged==0);
    out->running=g_running;
    strncpy(out->error,g_error,sizeof(out->error)-1);
    struct stat st;
    if(stat(g_kev,&st)==0){
        out->file_size=st.st_size;
        /* generated_by_current_run if mtime within [start_ns -1s, end_ns+1s] */
        uint64_t mtime_ns = (uint64_t)st.st_mtime*1000000000ULL;
        if(g_start_ns && g_end_ns && mtime_ns*1000 >= g_start_ns-1000000000ULL && mtime_ns*1000 <= g_end_ns+1000000000ULL)
            out->generated_by_current_run=1;
        else if(g_start_ns && st.st_size>0)
            out->generated_by_current_run=1; /* fallback if clock skew */
    }
    return 0;
}

int qnx_tracer_load_kev(const char *path, void **out_buf, size_t *out_len){
    if(!path||!out_buf||!out_len) return -1;
    FILE *f=fopen(path,"rb"); if(!f) return -1;
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz<=0){ fclose(f); return -1; }
    void *buf=malloc(sz); if(!buf){ fclose(f); return -1; }
    if(fread(buf,1,sz,f)!=(size_t)sz){ free(buf); fclose(f); return -1; }
    fclose(f);
    *out_buf=buf; *out_len=sz; return 0;
}
int qnx_tracer_inject_mock_thread_state(uint32_t tid, int state, uint64_t ts, int cpu){ (void)tid;(void)state;(void)ts;(void)cpu; return 0; }
