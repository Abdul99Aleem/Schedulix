/*
 * Schedulix — Automotive RTOS Performance & Latency Analyzer
 * Phases 1–5 integrated core + Deliverable 1 CAN pipeline
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>

#include "can_frame.h"
#include "can_decoder.h"
#include "sched_event.h"
#include "event_manager.h"
#include "can_adapter.h"

#include "workload_config.h"
#include "workload.h"
#include "trace_collector.h"
#include "trace_instrumentation.h"
#include "trace_schema.h"
#include "analyzer.h"
#include "stress_scenarios.h"
#include "stress_generator.h"
#include "replay.h"
#include "qnx_tracer.h"
#include "gpio_marker.h"
#include "manifest.h"
#include "uart_adapter.h"
#include "qnx_kernel_trace_parser.h"
#include <sys/stat.h>

#ifdef ENABLE_TCP
#include "can_injector.h"
#endif

static uint32_t g_seq = 1;

static void prepare_atomic_artifacts(const char *trace, const char *manifest, const char *analysis){
    unlink(trace);
    unlink(manifest);
    unlink(analysis);
    unlink("trace_flight.bin");
    // do not unlink .kev here; qnx_tracer does
}
static int verify_artifacts(const char *trace, const char *manifest){
    struct stat st;
    if(stat(trace,&st)!=0 || st.st_size <= (off_t)sizeof(trace_shm_header_t)){
        fprintf(stderr,"[verify] %s missing or incomplete\n", trace);
        return -1;
    }
    if(stat(manifest,&st)!=0){
        fprintf(stderr,"[verify] %s missing\n", manifest);
        return -1;
    }
    return 0;
}
static void write_failure_report(const char *scenario, const char *reason){
    char path[128]; snprintf(path,sizeof(path),"failure_%s.txt", scenario);
    FILE *f=fopen(path,"w");
    if(f){ fprintf(f,"scenario: %s\nreason: %s\nrun_id: %d\n", scenario, reason, (int)getpid()); fclose(f); }
    fprintf(stderr,"[FAIL] %s: %s -> %s\n", scenario, reason, path);
}

static void process_frame(const can_frame_t *frame) {
    if (!frame) return;
    can_frame_print(frame);
    printf("\n");
    sched_event_t ev = decode_can_frame(frame);
    sched_event_print(&ev);
    printf("\n");
    event_manager_dispatch(&ev);
    printf("\n");
}

#ifdef ENABLE_TCP
static void injector_callback(const can_frame_t *frame, void *user) {
    (void)user;
    printf("\n[TCP] Received injected frame\n");
    process_frame(frame);
}
#endif

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("  (no args)               CAN sim (D1) — 5 frames\n");
    printf("  --tcp [port]            TCP injection server (default 5000)\n");
    printf("  --workloads             Phase1: manifest + threads demo\n");
    printf("  --full                  Full core: workloads+trace+CAN+analyze\n");
    printf("  --core                  Alias for --full\n");
    printf("  --stress [id]           Run stress scenario S0..S6 (or all) S1:80%% default, use 1:90 for load\n");
    printf("  --sweep                 S1 CPU load sweep 20/40/60/70/80/85/90/95 (manifest+analysis per load)\n");
    printf("  --analyze <trace.bin>   Analyze trace → report\n");
    printf("  --replay <trace.bin>    Replay trace\n");
    printf("  --manifest              Print workload table\n");
    printf("  --help                  This help\n");
}

static int run_phase1_demo(void){
    printf("=== PHASE 1: Workload Model ===\n");
    workload_config_print_table();
    printf("\n");
    workload_manifest_print();
    printf("\nInitializing workloads...\n");
    prepare_atomic_artifacts("trace_phase1.bin","manifest_phase1.json","analysis_phase1.json");
    if(trace_collector_init_heap(8192, TRACE_MODE_FULL, 0)!=0){ write_failure_report("S0_PHASE1","trace init failed"); return 1; }
    trace_instr_init();
    gpio_marker_init();
    if(workload_init_all()!=0){ write_failure_report("S0_PHASE1","workload init failed"); return 1; }
    workload_start_all();
    printf("3-4 threads running with distinct priorities/periods (BRAKE 20/10ms, ADAS 15/20ms, DIAG 10/50ms)\n");
    for(int i=0;i<4;i++){ workload_release_by_id(TASK_ID_BRAKE, g_seq++); usleep(5000); }
    sleep(1);
    workload_stop_all();
    workload_join_all();
    gpio_marker_shutdown();
    trace_collector_flush("trace_phase1.bin");
    printf("Trace flushed to trace_phase1.bin, dropped=%llu hw=%u\n",
        (unsigned long long)trace_collector_dropped(), trace_collector_high_watermark());
    manifest_collect_full("S0_PHASE1",-1,1000,"trace_phase1.bin","/tmp/schedulix.kev",1000000000ULL,"BRAKE burst");
    manifest_write_json("manifest_phase1.json");
    manifest_write_text("manifest_phase1.txt");
    printf("Manifest: manifest_phase1.json\n");
    if(verify_artifacts("trace_phase1.bin","manifest_phase1.json")!=0){ write_failure_report("S0_PHASE1","artifacts incomplete"); trace_collector_shutdown(); return 1; }
    trace_collector_shutdown();
    return 0;
}

static int run_full_demo(void){
    printf("=== SCHEDULIX CORE FULL DEMO (Phases 1-5) ===\n");
    printf("Diagnostic question: Why did workload miss deadline? — correlation, not just metrics\n\n");
    workload_config_print_table();
    prepare_atomic_artifacts("trace.bin","manifest.json","analysis.json");
    uint64_t t0 = 0; struct timespec ts0; clock_gettime(CLOCK_MONOTONIC,&ts0); t0=(uint64_t)ts0.tv_sec*1000000000ULL+ts0.tv_nsec;
    if(trace_collector_init_heap(16384, TRACE_MODE_FULL, 0)!=0){ write_failure_report("FULL_CORE","trace init failed"); return 1; }
    trace_instr_init();
    gpio_marker_init();
    int trc = qnx_tracer_start("/tmp/schedulix.kev", QNX_TRACE_THREAD|QNX_TRACE_INT);
    if(trc!=0) fprintf(stderr,"[warning] kernel trace not captured (run as root: on -p 63 tracelogger)\n");
    if(workload_init_all()!=0){ write_failure_report("FULL_CORE","workload init failed"); return 1; }
    workload_start_all();
    printf("Workloads started. Feeding CAN events → Event Manager → workload release\n");

    can_adapter_init();
    can_frame_t frame;
    int count=0;
    while(can_adapter_receive(&frame)==0){
        process_frame(&frame);
        usleep(20000);
        count++;
    }
    printf("Fed %d simulated CAN frames\n", count);
    printf("Injecting burst 10x BRAKE in 1ms (S3)\n");
    for(int i=0;i<10;i++){ can_frame_t bf={.id=0x100,.dlc=1,.data={0x01}}; can_frame_set_timestamp(&bf); process_frame(&bf); }
    sleep(1);
    workload_stop_all();
    workload_join_all();
    gpio_marker_shutdown();
    qnx_tracer_stop();
    struct timespec ts1; clock_gettime(CLOCK_MONOTONIC,&ts1); uint64_t t1=(uint64_t)ts1.tv_sec*1000000000ULL+ts1.tv_nsec;
    trace_collector_flush("trace.bin");
    trace_collector_snapshot("trace_flight.bin", 1000);
    printf("\nTrace: trace.bin + trace_flight.bin flushed. Dropped=%llu high=%u\n", (unsigned long long)trace_collector_dropped(), trace_collector_high_watermark());
    qnx_provenance_t prov={0}; qnx_tracer_get_provenance(&prov);
    if(!prov.privileged || prov.file_size==0){
        fprintf(stderr,"[warning] kernel trace not captured (tracelogger requires root, see docs). Analysis will be semantic-only.\n");
    }
    manifest_collect_full("FULL_CORE", -1, (uint32_t)((t1-t0)/1000000ULL), "trace.bin", prov.privileged?"/tmp/schedulix.kev":"FAILED: no kernel trace", t1-t0, "CAN 0x100 burst");
    manifest_write_json("manifest.json");
    manifest_write_text("manifest.txt");
    printf("Manifest: manifest.json / manifest.txt (kernel %s, size %llu, gpio %s)\n", prov.privileged?"OK":"FAILED", (unsigned long long)prov.file_size, gpio_marker_is_available()?"real":"mock");
    activation_analysis_t *acts=NULL; size_t n=0;
    if(analyzer_load_trace_file("trace.bin")==0){
        trace_shm_header_t hdr;
        if(analyzer_get_header(&hdr)==0){
            analyzer_correlate(&acts,&n);
            analyzer_print_report(acts,n,&hdr);
            analyzer_write_json("analysis.json",acts,n,&hdr);
            analyzer_free(acts);
        } else {
            write_failure_report("FULL_CORE","header invalid");
            trace_collector_shutdown(); return 1;
        }
    } else {
        write_failure_report("FULL_CORE","trace integrity failed");
        trace_collector_shutdown(); return 1;
    }
    if(verify_artifacts("trace.bin","manifest.json")!=0){ write_failure_report("FULL_CORE","artifacts incomplete"); trace_collector_shutdown(); return 1; }
    trace_collector_shutdown();
    printf("\nFULL DEMO DONE — replay: %s --replay trace.bin | analyze: %s --analyze trace.bin\n", "schedulix_can","schedulix_can");
    return 0;
}

static void print_subcommand_help(void) {
    printf("Schedulix subcommands:\n");
    printf("  status                         Show overall system status\n");
    printf("  workload list                  List configured workloads\n");
    printf("  workload start                 Run workloads in-process (5s)\n");
    printf("  workload stop                  Stop workloads\n");
    printf("  stress start [--cpu <%%>]       Start stress CPU load (5s)\n");
    printf("  stress stop                    Stop stress generator\n");
    printf("  stress status                  Show stress status\n");
    printf("  trace start                    Initialize trace collector\n");
    printf("  trace stop                     Shutdown trace collector\n");
    printf("  trace flush                    Flush buffer to trace.bin\n");
    printf("  trace status                   Show trace buffer status\n");
    printf("  analyze [file]                 Analyze trace file (default trace.bin)\n");
    printf("  report                         Print report from trace.bin\n");
    printf("  can status                     Show CAN adapter status\n");
    printf("  can send                       Send simulated CAN frame\n");
    printf("  can burst                      Send a burst of 10x BRAKE events\n");
    printf("  uart status                    Show UART adapter status\n");
    printf("  uart send <data>               Send data over UART\n");
    printf("  uart receive                   Read from UART\n");
    printf("  gpio test                      Test physical/mock GPIO toggling\n");
    printf("  experiment list                List benchmark scenarios S0..S6\n");
    printf("  experiment run S<id>           Run benchmark scenario (S0..S6)\n");
}

int handle_subcommands(int argc, char *argv[]) {
    if (argc < 2) return -1;
    const char *cmd = argv[1];
    if (strcmp(cmd, "status") && strcmp(cmd, "workload") && strcmp(cmd, "stress") &&
        strcmp(cmd, "trace") && strcmp(cmd, "analyze") && strcmp(cmd, "report") &&
        strcmp(cmd, "can") && strcmp(cmd, "uart") && strcmp(cmd, "gpio") &&
        strcmp(cmd, "experiment") && strcmp(cmd, "help")) {
        return -1;
    }

    if (!strcmp(cmd, "help")) {
        print_subcommand_help();
        return 0;
    }

    if (!strcmp(cmd, "status")) {
        printf("SCHEDULIX SYSTEM STATUS\n");
        printf("=======================\n");
        printf("Workloads:        %u tasks configured\n", g_workload_count);
        printf("Trace Collector:  %s\n", trace_collector_is_initialized() ? "ACTIVE" : "STOPPED");
        printf("Stress Generator: %s (target util %d%%)\n", stress_generator_active() ? "ACTIVE" : "STOPPED", stress_generator_utilization());
        can_adapter_init();
        printf("CAN Adapter:      %s\n", "simulation (no hardware)");
        can_adapter_shutdown();
        uart_adapter_init(NULL);
        printf("UART Adapter:     %s\n", uart_adapter_status());
        uart_adapter_shutdown();
        gpio_marker_init();
        printf("GPIO Markers:     %s\n", gpio_marker_is_available() ? "REAL (BCM2711 registers mapped)" : "MOCK (software simulation)");
        gpio_marker_shutdown();
        qnx_provenance_t prov;
        qnx_tracer_get_provenance(&prov);
        printf("Kernel Tracer:    %s\n", prov.privileged ? "PRIVILEGED (root)" : "UNPRIVILEGED (trace disabled)");
        return 0;
    }

    if (!strcmp(cmd, "workload")) {
        if (argc < 3) {
            printf("Usage: schedulix workload [list|start|stop]\n");
            return 1;
        }
        const char *sub = argv[2];
        if (!strcmp(sub, "list")) {
            workload_config_print_table();
            return 0;
        } else if (!strcmp(sub, "start")) {
            printf("Running workloads for 5 seconds...\n");
            return run_phase1_demo();
        } else if (!strcmp(sub, "stop")) {
            printf("Workloads stopped.\n");
            return 0;
        } else {
            printf("Unknown workload subcommand: %s\n", sub);
            return 1;
        }
    }

    if (!strcmp(cmd, "stress")) {
        if (argc < 3) {
            printf("Usage: schedulix stress [start|stop|status]\n");
            return 1;
        }
        const char *sub = argv[2];
        if (!strcmp(sub, "start")) {
            int pct = 50;
            if (argc >= 5 && !strcmp(argv[3], "--cpu")) {
                pct = atoi(argv[4]);
            }
            printf("Starting CPU stress generator at %d%% load for 5 seconds...\n", pct);
            stress_generator_init();
            stress_generator_start_load(pct);
            workload_init_all();
            workload_start_all();
            sleep(5);
            workload_stop_all();
            workload_join_all();
            stress_generator_stop();
            printf("Stress generator stopped.\n");
            return 0;
        } else if (!strcmp(sub, "stop")) {
            stress_generator_stop();
            printf("Stress generator stopped.\n");
            return 0;
        } else if (!strcmp(sub, "status")) {
            printf("Stress Generator: %s (target util %d%%)\n", stress_generator_active() ? "ACTIVE" : "STOPPED", stress_generator_utilization());
            return 0;
        } else {
            printf("Unknown stress subcommand: %s\n", sub);
            return 1;
        }
    }

    if (!strcmp(cmd, "trace")) {
        if (argc < 3) {
            printf("Usage: schedulix trace [start|stop|flush|status]\n");
            return 1;
        }
        const char *sub = argv[2];
        if (!strcmp(sub, "start")) {
            if (trace_collector_init_heap(16384, TRACE_MODE_FULL, 0) == 0) {
                printf("Trace collector initialized with capacity 16384.\n");
                return 0;
            } else {
                printf("Trace collector initialization failed.\n");
                return 1;
            }
        } else if (!strcmp(sub, "stop")) {
            trace_collector_shutdown();
            printf("Trace collector stopped.\n");
            return 0;
        } else if (!strcmp(sub, "flush")) {
            if (trace_collector_flush("trace.bin") == 0) {
                printf("Trace flushed to trace.bin\n");
                return 0;
            } else {
                printf("Trace flush failed.\n");
                return 1;
            }
        } else if (!strcmp(sub, "status")) {
            if (trace_collector_is_initialized()) {
                trace_shm_header_t hdr;
                trace_collector_get_header(&hdr);
                printf("Trace Collector: ACTIVE\n");
                printf("Capacity:        %u\n", hdr.capacity);
                printf("Written:         %llu\n", (unsigned long long)hdr.records_written);
                printf("Dropped:         %llu\n", (unsigned long long)hdr.records_dropped);
                printf("Watermark:       %u\n", hdr.high_watermark);
            } else {
                printf("Trace Collector: INACTIVE\n");
            }
            return 0;
        } else if (!strcmp(sub, "decode")) {
            if (argc < 4) {
                printf("Usage: schedulix trace decode <kev_file_path>\n");
                return 1;
            }
            const char *path = argv[3];
            printf("Decoding QNX kernel trace: %s\n", path);
            int rc = qnx_kernel_trace_parse(path);
            if (rc != 0) {
                printf("Error parsing trace: %d\n", rc);
                return 1;
            }
            KernelTraceEvent evs[256];
            size_t n = qnx_kernel_trace_get_events(evs, 256);
            printf("Successfully parsed %d events.\n", (int)n);
            printf("%-18s %-18s %-4s %-10s %-8s %-8s\n", "Cycles", "Time(ns)", "CPU", "Event", "PID", "TID");
            printf("--------------------------------------------------------------------------------\n");
            for (size_t i = 0; i < n; i++) {
                printf("%-18llu %-18llu %-4u %-10u %-8u %-8u\n",
                       (unsigned long long)evs[i].timestamp_cycles,
                       (unsigned long long)evs[i].timestamp_ns,
                       evs[i].cpu, evs[i].event_type, evs[i].pid, evs[i].tid);
            }
            return 0;
        } else {
            printf("Unknown trace subcommand: %s. Expected start|stop|flush|status|decode\n", sub);
            return 1;
        }
    }

    if (!strcmp(cmd, "analyze")) {
        const char *path = (argc >= 3) ? argv[2] : "trace.bin";
        if (analyzer_load_trace_file(path) != 0) {
            fprintf(stderr, "[analyze] failed to parse %s (header integrity)\n", path);
            return 2;
        }
        trace_shm_header_t hdr;
        if (analyzer_get_header(&hdr) != 0) {
            fprintf(stderr, "[analyze] no valid header\n");
            return 2;
        }
        activation_analysis_t *acts = NULL;
        size_t n = 0;
        analyzer_correlate(&acts, &n);
        analyzer_print_report(acts, n, &hdr);
        analyzer_write_json("analysis.json", acts, n, &hdr);
        analyzer_free(acts);
        return 0;
    }

    if (!strcmp(cmd, "report")) {
        return run_full_demo();
    }

    if (!strcmp(cmd, "can")) {
        if (argc < 3) {
            printf("Usage: schedulix can [status|send|burst]\n");
            return 1;
        }
        const char *sub = argv[2];
        if (!strcmp(sub, "status")) {
            can_adapter_init();
            printf("CAN Adapter: simulation (no hardware)\n");
            can_adapter_shutdown();
            return 0;
        } else if (!strcmp(sub, "send")) {
            can_adapter_init();
            can_frame_t frame = {.id = 0x100, .dlc = 1, .data = {0x01}};
            can_frame_set_timestamp(&frame);
            process_frame(&frame);
            can_adapter_shutdown();
            printf("Simulated CAN event 0x100 sent.\n");
            return 0;
        } else if (!strcmp(sub, "burst")) {
            printf("Injecting 10x BRAKE events burst...\n");
            for (int i = 0; i < 10; i++) {
                can_frame_t bf = {.id = 0x100, .dlc = 1, .data = {0x01}};
                can_frame_set_timestamp(&bf);
                process_frame(&bf);
            }
            return 0;
        } else {
            printf("Unknown CAN subcommand: %s\n", sub);
            return 1;
        }
    }

    if (!strcmp(cmd, "uart")) {
        if (argc < 3) {
            printf("Usage: schedulix uart [status|start|send|receive] [--port <path>] [--baud <rate>]\n");
            return 1;
        }
        const char *sub = argv[2];
        const char *port = "/dev/ser1";
        int baud = 115200;
        for (int i = 3; i < argc; i++) {
            if (!strcmp(argv[i], "--port") && i + 1 < argc) {
                port = argv[i+1];
                i++;
            } else if (!strcmp(argv[i], "--baud") && i + 1 < argc) {
                baud = atoi(argv[i+1]);
                i++;
            }
        }

        uart_adapter_init(port);
        if (!strcmp(sub, "status")) {
            printf("UART Adapter Backend: %s\n", uart_adapter_is_simulated() ? "simulated" : "physical");
            printf("UART Status: %s\n", uart_adapter_status());
            uart_adapter_shutdown();
            return 0;
        } else if (!strcmp(sub, "start")) {
            printf("Starting UART on port %s (baud %d)...\n", port, baud);
            printf("UART Adapter Backend: %s\n", uart_adapter_is_simulated() ? "simulated" : "physical");
            printf("UART Status: %s\n", uart_adapter_status());
            return 0;
        } else if (!strcmp(sub, "send")) {
            // Find data argument (typically next argument after --port/--baud flags or if simple, at argv[3])
            const char *data = NULL;
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "--baud") == 0) {
                    i++; // skip flag value
                } else if (argv[i][0] != '-') {
                    data = argv[i];
                    break;
                }
            }
            if (!data) {
                printf("Usage: schedulix uart send <data> [--port <path>] [--baud <rate>]\n");
                uart_adapter_shutdown();
                return 1;
            }
            int n = uart_adapter_send((const uint8_t*)data, strlen(data));
            printf("Sent %d bytes over UART: '%s'\n", n, data);
            printf("Trace record: %s (event type %d, was EXTERNAL_EVENT_RX=1 before the fix)\n",
                   trace_event_to_string(TRACE_EXTERNAL_EVENT_TX), (int)TRACE_EXTERNAL_EVENT_TX);
            uart_adapter_trace_tx(1, g_seq++, data[0]);
            uart_adapter_shutdown();
            return 0;
        } else if (!strcmp(sub, "receive")) {
            uint8_t buf[128];
            memset(buf, 0, sizeof(buf));
            int n = uart_adapter_receive(buf, sizeof(buf) - 1);
            if (n > 0) {
                printf("Received %d bytes from UART: '%s'\n", n, (char*)buf);
                printf("Trace record: %s (event type %d)\n",
                       trace_event_to_string(TRACE_EXTERNAL_EVENT_RX), (int)TRACE_EXTERNAL_EVENT_RX);
                uart_adapter_trace_rx(1, g_seq++, buf[0]);
            } else {
                printf("No UART data available.\n");
            }
            uart_adapter_shutdown();
            return 0;
        } else {
            printf("Unknown UART subcommand: %s. Expected status|start|send|receive\n", sub);
            uart_adapter_shutdown();
            return 1;
        }
    }

    if (!strcmp(cmd, "gpio")) {
        if (argc < 3 || strcmp(argv[2], "test")) {
            printf("Usage: schedulix gpio test\n");
            return 1;
        }
        printf("GPIO Marker validation test:\n");
        gpio_marker_init();
        printf("GPIO Availability: %s\n", gpio_marker_is_available() ? "REAL PHYSICAL" : "MOCK");
        /* Drive every marker pin, not just BRAKE. The previous version toggled only
         * GPIO 4 (BRAKE), so a LED wired to pin 11 (ADAS) or pin 13 (DIAG)
         * stayed dark and the test looked broken. */
        static const int pins[3]   = { 4, 17, 27 };
        static const uint32_t ids[3] = { TASK_ID_BRAKE, TASK_ID_ADAS, TASK_ID_DIAG };
        static const char *names[3] = { "BRAKE(GPIO4/pin7)",
                                        "ADAS (GPIO17/pin11)",
                                        "DIAG (GPIO27/pin13)" };

        printf("fsel before: 4=%d 17=%d 27=%d  (1=output)\n",
               gpio_marker_fsel(4), gpio_marker_fsel(17), gpio_marker_fsel(27));

        printf("Toggling all three marker pins, 200 ms apart...\n");
        for (int i = 0; i < 3; i++) {
            printf("  %-18s ... ", names[i]);
            fflush(stdout);
            gpio_marker_for_task_high(ids[i]);
            usleep(200000);          /* 200 ms: visible on an LED */
            gpio_marker_for_task_low(ids[i]);

            gpio_validation_t v = gpio_marker_validate(ids[i], 0, 200000000ULL);
            printf("fsel=%d  hw high=%d low=%d confirmed=%s  pulse=%llu us  delta=%lld us  valid=%s\n",
                   gpio_marker_fsel(pins[i]),
                   v.hw_level_high, v.hw_level_low,
                   v.hw_confirmed ? "YES" : "NO",
                   (unsigned long long)((v.gpio_low_ns - v.gpio_high_ns) / 1000),
                   (long long)(v.delta_ns / 1000),
                   v.valid ? "YES" : "NO");
        }

        printf("fsel after : 4=%d 17=%d 27=%d  (1=output)\n",
               gpio_marker_fsel(4), gpio_marker_fsel(17), gpio_marker_fsel(27));

        gpio_validation_t v = gpio_marker_validate(TASK_ID_BRAKE, 0, 200000000ULL);
        printf("Validation: delta %lld ns, valid: %s\n", (long long)v.delta_ns, v.valid ? "YES" : "NO");
        /* Report the readback separately: this is what distinguishes a real
         * edge on the wire from a write that silently did nothing. */
        printf("Hardware readback: level after high=%d, after low=%d, confirmed=%s\n",
               v.hw_level_high, v.hw_level_low, v.hw_confirmed ? "YES" : "NO");
        gpio_marker_shutdown();
        return 0;
    }

    if (!strcmp(cmd, "experiment")) {
        if (argc < 3) {
            printf("Usage: schedulix experiment [list|run]\n");
            return 1;
        }
        const char *sub = argv[2];
        if (!strcmp(sub, "list")) {
            printf("SCENARIOS LIST:\n");
            printf("  S0 - baseline (no load, nominal timing)\n");
            printf("  S1 - CPU saturation load sweep\n");
            printf("  S2 - mutex priority inversion contention\n");
            printf("  S3 - high frequency CAN event burst\n");
            printf("  S4 - sustained storm deadline violation\n");
            printf("  S5 - mixed stress workload jitter analysis\n");
            printf("  S6 - core affinity pinning configuration\n");
            return 0;
        } else if (!strcmp(sub, "run")) {
            if (argc < 4) {
                printf("Usage: schedulix experiment run S<id> or <id>\n");
                return 1;
            }
            const char *id_str = argv[3];
            int id = -1;
            if (id_str[0] == 'S' || id_str[0] == 's') {
                id = atoi(id_str + 1);
            } else {
                id = atoi(id_str);
            }
            if (id < 0 || id > 6) {
                printf("Invalid scenario ID: %s. Use 0..6.\n", id_str);
                return 1;
            }
            
            char trace[64]; snprintf(trace,sizeof(trace),"trace_s%d.bin",id);
            char manifest[64]; snprintf(manifest,sizeof(manifest),"manifest_s%d.json",id);
            char analysis[64]; snprintf(analysis,sizeof(analysis),"analysis_s%d.json",id);
            prepare_atomic_artifacts(trace,manifest,analysis);
            if(trace_collector_init_heap(16384,TRACE_MODE_FULL,0)!=0){ write_failure_report(scenario_name((scenario_id_t)id),"trace init failed"); return 1; }
            trace_instr_init(); gpio_marker_init();
            int trc=qnx_tracer_start("/tmp/schedulix.kev", QNX_TRACE_THREAD);
            if(trc!=0) fprintf(stderr,"[%s] kernel trace unavailable\n", scenario_name((scenario_id_t)id));
            if(workload_init_all()!=0){ write_failure_report(scenario_name((scenario_id_t)id),"workload init failed"); return 1; }
            struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
            int rc = scenario_run((scenario_id_t)id, 5000); // run for 5 seconds
            clock_gettime(CLOCK_MONOTONIC,&t1); uint64_t dur=(t1.tv_sec-t0.tv_sec)*1000000000ULL + (t1.tv_nsec-t0.tv_nsec);
            if(rc!=0){ write_failure_report(scenario_name((scenario_id_t)id),"scenario failed"); trace_collector_shutdown(); workload_stop_all(); workload_join_all(); return 1; }
            trace_collector_flush(trace);
            qnx_provenance_t prov; qnx_tracer_get_provenance(&prov);
            manifest_collect_full(scenario_name((scenario_id_t)id),-1,5000,trace,prov.privileged?"/tmp/schedulix.kev":"FAILED: no kernel trace",dur,"command run");
            manifest_write_json(manifest);
            activation_analysis_t *acts=NULL; size_t n=0;
            if(analyzer_load_trace_file(trace)==0){
                trace_shm_header_t hdr;
                if(analyzer_get_header(&hdr)==0){
                    analyzer_correlate(&acts,&n);
                    analyzer_print_report(acts,n,&hdr);
                    analyzer_write_json(analysis,acts,n,&hdr);
                    analyzer_free(acts);
                } else { write_failure_report(scenario_name((scenario_id_t)id),"header invalid"); }
            } else { write_failure_report(scenario_name((scenario_id_t)id),"trace integrity failed"); }
            if(verify_artifacts(trace,manifest)!=0) write_failure_report(scenario_name((scenario_id_t)id),"artifacts incomplete");
            trace_collector_shutdown(); gpio_marker_shutdown(); qnx_tracer_stop();
            workload_stop_all(); workload_join_all();
            printf("Experiment S%d run finished. Wrote %s, %s, %s\n", id, trace, manifest, analysis);
            return 0;
        } else {
            printf("Unknown experiment subcommand: %s\n", sub);
            return 1;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int sub_rc = handle_subcommands(argc, argv);
    if (sub_rc >= 0) {
        return sub_rc;
    }

    printf("SCHEDULIX CAN EVENT MANAGER\n");
    printf("===========================\n");
    printf("Phases 1-5 Core + Deliverable 1 CAN pipeline\n");
    printf("Mode: %s\n\n",
#ifdef ENABLE_TCP
           "TCP-capable build"
#else
           "simulation (no TCP)"
#endif
    );

    if (argc==1) {
        /* Default D1 demo */
        can_adapter_init();
        can_frame_t frame; int n=0;
        while(can_adapter_receive(&frame)==0){ process_frame(&frame); n++; }
        if(n==0){ printf("No frames\n"); return 1; }
        event_manager_stats_t st; event_manager_get_stats(&st);
        printf("SUMMARY\n-------\nFrames %llu BRAKE %llu ADAS %llu DIAG %llu UNK %llu\n",
            (unsigned long long)st.total,(unsigned long long)st.brake_count,(unsigned long long)st.adas_count,(unsigned long long)st.diagnostic_count,(unsigned long long)st.unknown_count);
        printf("Deliverable 1 DONE\n");
        can_adapter_shutdown();
        return 0;
    }

    for (int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){ print_usage(argv[0]); return 0; }
        else if(!strcmp(argv[i],"--tcp")){
            uint16_t port=5000;
            if(i+1<argc){ char *e=NULL; long p=strtol(argv[i+1],&e,10); if(e!=argv[i+1]&&p>0&&p<65536){port=p; i++;} }
#ifndef ENABLE_TCP
            (void)port;
            fprintf(stderr,"TCP not enabled. Rebuild with CCFLAGS=-DENABLE_TCP\n"); return 1;
#else
            printf("--- CAN sim ---\n"); can_adapter_init(); can_frame_t f; int c=0; while(can_adapter_receive(&f)==0){process_frame(&f); c++;} printf("--- sim %d ---\n",c);
            printf("--- TCP server port %u ---\n",port);
            can_injector_run_server(port,injector_callback,NULL); return 0;
#endif
        }
        else if(!strcmp(argv[i],"--manifest")){ workload_config_print_table(); return 0; }
        else if(!strcmp(argv[i],"--workloads")||!strcmp(argv[i],"--phase1")){ return run_phase1_demo(); }
        else if(!strcmp(argv[i],"--full")||!strcmp(argv[i],"--core")){ return run_full_demo(); }
        else if(!strcmp(argv[i],"--sweep")){
            int loads[]={20,40,60,70,80,85,90,95};
            printf("Load sweep S1 20..95%%  S1-A system-wide (prio12 any) vs S1-B same-CPU pinned CPU0 prio18\n");
            printf("Running S1-B same-CPU for controlled contention (BRAKE/ADAS/DIAG/STRESS pinned CPU0)\n");
            for(size_t li=0;li<sizeof(loads)/sizeof(loads[0]);li++){
                int pct=loads[li];
                char trace[64]; snprintf(trace,sizeof(trace),"trace_s1_%d.bin",pct);
                char manifest[64]; snprintf(manifest,sizeof(manifest),"manifest_s1_%d.json",pct);
                char analysis[64]; snprintf(analysis,sizeof(analysis),"analysis_s1_%d.json",pct);
                prepare_atomic_artifacts(trace,manifest,analysis);
                if(trace_collector_init_heap(16384,TRACE_MODE_FULL,0)!=0){ write_failure_report("S1_SWEEP","trace init failed"); continue; }
                trace_instr_init(); gpio_marker_init();
                int trc = qnx_tracer_start("/tmp/schedulix.kev", QNX_TRACE_THREAD);
                if(trc!=0) fprintf(stderr,"[S1 %d%%] kernel trace unavailable (run as root)\n", pct);
                workload_set_affinity_all(0); /* S1-B: pin ECU to CPU0 */
                if(workload_init_all()!=0){ write_failure_report("S1_SWEEP","workload init failed"); trace_collector_shutdown(); continue; }
                struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
                // S1-B pinned: stress on same CPU0 with higher prio 18 to force READY wait
                int rc;
                if(pct==0) rc=scenario_run(SCENARIO_S0_BASELINE,10000);
                else {
                    // use pinned stress
                    stress_generator_init();
                    char stress_name[32]; snprintf(stress_name,sizeof(stress_name),"S1-B %d%% CPU0",pct);
                    printf("[S1-B] %s\n", stress_name);
                    // start pinned stress directly to control prio/affinity
                    rc = stress_generator_start_load_pinned(pct,0);
                    if(rc==0){
                        workload_start_all();
                        struct timespec d={10,0}; nanosleep(&d,NULL); // 10s run while stress contends on same CPU
                        workload_stop_all();
                        stress_generator_stop();
                    }
                }
                clock_gettime(CLOCK_MONOTONIC,&t1); uint64_t dur=(t1.tv_sec-t0.tv_sec)*1000000000ULL + (t1.tv_nsec-t0.tv_nsec);
                if(rc!=0){ write_failure_report("S1_SWEEP","scenario failed"); trace_collector_shutdown(); workload_stop_all(); workload_join_all(); workload_set_affinity_all(-1); continue; }
                trace_collector_flush(trace);
                trace_collector_snapshot("trace_flight.bin",1000);
                qnx_provenance_t prov={0}; qnx_tracer_get_provenance(&prov);
                manifest_collect_full("S1-B",pct,10000,trace, prov.privileged?"/tmp/schedulix.kev":"FAILED: no kernel trace", dur,"S1-B same-CPU CPU0 prio18");
                manifest_write_json(manifest);
                activation_analysis_t *acts=NULL; size_t n=0;
                if(analyzer_load_trace_file(trace)==0){
                    trace_shm_header_t hdr;
                    if(analyzer_get_header(&hdr)==0){
                        analyzer_correlate(&acts,&n);
                        analyzer_print_report(acts,n,&hdr);
                        analyzer_write_json(analysis,acts,n,&hdr);
                        analyzer_free(acts);
                    } else { write_failure_report("S1_SWEEP","header invalid"); }
                } else {
                    write_failure_report("S1_SWEEP","trace integrity failed");
                }
                if(verify_artifacts(trace,manifest)!=0) write_failure_report("S1_SWEEP","artifacts incomplete");
                else printf("Wrote %s %s %s\n",trace,manifest,analysis);
                trace_collector_shutdown(); gpio_marker_shutdown(); qnx_tracer_stop();
                workload_stop_all(); workload_join_all();
                workload_set_affinity_all(-1); /* reset */
            }
            printf("Sweep table: Load vs BRAKE P99 — see analysis_s1_*.json (S1-B pinned should show knee)\n");
            return 0;
        }
        else if(!strcmp(argv[i],"--stress")){
            const char *arg = (i+1<argc)? argv[i+1] : "all";
            if(!strcmp(arg,"all")){
                prepare_atomic_artifacts("trace_stress.bin","manifest_stress.json","analysis_stress.json");
                if(trace_collector_init_heap(16384,TRACE_MODE_FULL,0)!=0){ write_failure_report("S0-S6","trace init failed"); return 1; }
                trace_instr_init(); gpio_marker_init();
                int trc=qnx_tracer_start("/tmp/schedulix.kev", QNX_TRACE_THREAD);
                if(trc!=0) fprintf(stderr,"[S0-S6] kernel trace unavailable\n");
                if(workload_init_all()!=0){ write_failure_report("S0-S6","workload init failed"); return 1; }
                struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
                int rc=scenario_run_all();
                clock_gettime(CLOCK_MONOTONIC,&t1); uint64_t dur=(t1.tv_sec-t0.tv_sec)*1000000000ULL + (t1.tv_nsec-t0.tv_nsec);
                if(rc!=0){ write_failure_report("S0-S6","scenario failed"); trace_collector_shutdown(); workload_stop_all(); workload_join_all(); return 1; }
                trace_collector_flush("trace_stress.bin");
                qnx_provenance_t prov2={0}; qnx_tracer_get_provenance(&prov2);
                manifest_collect_full("S0-S6",-1,500*7,"trace_stress.bin",prov2.privileged?"/tmp/schedulix.kev":"FAILED: no kernel trace",dur,"all scenarios");
                manifest_write_json("manifest_stress.json");
                activation_analysis_t *acts=NULL; size_t n=0;
                if(analyzer_load_trace_file("trace_stress.bin")==0){
                    trace_shm_header_t hdr;
                    if(analyzer_get_header(&hdr)==0){
                        analyzer_correlate(&acts,&n);
                        analyzer_print_report(acts,n,&hdr);
                        analyzer_write_json("analysis_stress.json",acts,n,&hdr);
                        analyzer_free(acts);
                    } else { write_failure_report("S0-S6","header invalid"); }
                } else { write_failure_report("S0-S6","trace integrity failed"); }
                if(verify_artifacts("trace_stress.bin","manifest_stress.json")!=0) write_failure_report("S0-S6","artifacts incomplete");
                trace_collector_shutdown(); gpio_marker_shutdown(); qnx_tracer_stop();
                workload_stop_all(); workload_join_all();
                return 0;
            }
            else {
                int id=atoi(arg);
                int load=-1;
                char *colon=strchr((char*)arg,':');
                if(colon){ *colon=0; id=atoi(arg); load=atoi(colon+1); }
                char trace[64]; snprintf(trace,sizeof(trace),"trace_s%d.bin",id);
                char manifest[64]; snprintf(manifest,sizeof(manifest),"manifest_s%d.json",id);
                char analysis[64]; snprintf(analysis,sizeof(analysis),"analysis_s%d.json",id);
                prepare_atomic_artifacts(trace,manifest,analysis);
                if(trace_collector_init_heap(16384,TRACE_MODE_FULL,0)!=0){ write_failure_report(scenario_name((scenario_id_t)id),"trace init failed"); return 1; }
                trace_instr_init(); gpio_marker_init();
                int trc=qnx_tracer_start("/tmp/schedulix.kev", QNX_TRACE_THREAD);
                if(trc!=0) fprintf(stderr,"[%s] kernel trace unavailable\n", scenario_name((scenario_id_t)id));
                if(workload_init_all()!=0){ write_failure_report(scenario_name((scenario_id_t)id),"workload init failed"); return 1; }
                struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
                int rc;
                if(load>=0) rc=scenario_run_with_load((scenario_id_t)id,load,10000);
                else rc=scenario_run((scenario_id_t)id,10000);
                clock_gettime(CLOCK_MONOTONIC,&t1); uint64_t dur=(t1.tv_sec-t0.tv_sec)*1000000000ULL + (t1.tv_nsec-t0.tv_nsec);
                if(rc!=0){ write_failure_report(scenario_name((scenario_id_t)id),"scenario failed"); trace_collector_shutdown(); workload_stop_all(); workload_join_all(); return 1; }
                trace_collector_flush(trace);
                qnx_provenance_t prov3={0}; qnx_tracer_get_provenance(&prov3);
                manifest_collect_full(scenario_name((scenario_id_t)id),load,10000,trace,prov3.privileged?"/tmp/schedulix.kev":"FAILED: no kernel trace",dur,load>=0?"S1 load":"");
                manifest_write_json(manifest);
                activation_analysis_t *acts=NULL; size_t n=0;
                if(analyzer_load_trace_file(trace)==0){
                    trace_shm_header_t hdr;
                    if(analyzer_get_header(&hdr)==0){
                        analyzer_correlate(&acts,&n);
                        analyzer_print_report(acts,n,&hdr);
                        analyzer_write_json(analysis,acts,n,&hdr);
                        analyzer_free(acts);
                    } else { write_failure_report(scenario_name((scenario_id_t)id),"header invalid"); }
                } else { write_failure_report(scenario_name((scenario_id_t)id),"trace integrity failed"); }
                if(verify_artifacts(trace,manifest)!=0) write_failure_report(scenario_name((scenario_id_t)id),"artifacts incomplete");
                trace_collector_shutdown(); gpio_marker_shutdown(); qnx_tracer_stop();
                workload_stop_all(); workload_join_all();
                return 0;
            }
        }
        else if(!strcmp(argv[i],"--analyze")){
            const char *path = (i+1<argc)? argv[++i] : "trace.bin";
            if(analyzer_load_trace_file(path)!=0){ fprintf(stderr,"[analyze] failed to parse %s (header integrity)\n",path); return 2; }
            trace_shm_header_t hdr;
            if(analyzer_get_header(&hdr)!=0){ fprintf(stderr,"[analyze] no valid header\n"); return 2; }
            activation_analysis_t *acts=NULL; size_t n=0; analyzer_correlate(&acts,&n);
            analyzer_print_report(acts,n,&hdr);
            // also write analysis.json for manifest provenance
            analyzer_write_json("analysis.json",acts,n,&hdr);
            analyzer_free(acts); return 0;
        }
        else if(!strcmp(argv[i],"--replay")){
            const char *path=(i+1<argc)? argv[++i] : "trace.bin";
            return replay_from_trace(path);
        }
        else { printf("Unknown %s\n",argv[i]); print_usage(argv[0]); return 1; }
    }
    return 0;
}
