#include "qnx_kernel_trace_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define CPS 1500000000ULL   /* the board's 1500 MHz */
#define MSB (5ULL << 32)

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

/* ---- timestamp reassembly ---------------------------------------------- */

/* The event timestamp is the 32-bit LSB; _NTO_TRACE_CONTROLTIME carries the MSB
 * at each rollover. The previous code added the LSB to a base that already
 * contained it, permanently anchored to one rollover. */
static void test_rollover_reassembly(void) {
    printf("  rollover reassembly\n");
    /* A NULL path must be rejected outright, not handed to libtraceparser,
     * which faults inside the library. This was the observed hardware crash. */
    CHECK(qnx_kernel_trace_parse(NULL) != 0);
    qnx_kernel_trace_test_reset();

    /* Straddle a rollover with the MSB set, as the control event would. */
    qnx_kernel_trace_test_set_clock(CPS, 0, (uint32_t)(MSB >> 32), true);
    uint32_t lsbs[] = {0xFFFFFFF0u, 0xFFFFFFF5u, 0x00000005u, 0x0000000Au};
    uint64_t prev = 0;
    for (int i = 0; i < 4; i++) {
        qnx_kernel_trace_test_emit(0, 1, 1, 1 + i, lsbs[i]);
        uint64_t full = MSB | (uint64_t)lsbs[i];
        if (i) CHECK(full > prev);
        prev = full;
    }
    KernelTraceEvent ev[8];
    size_t n = qnx_kernel_trace_get_events(ev, 8);
    CHECK(n == 4);
    CHECK(ev[2].timestamp_cycles == (MSB | 0x00000005u));  /* past the rollover */
    CHECK(ev[3].timestamp_cycles == (MSB | 0x0000000Au));
    CHECK(qnx_kernel_trace_get_regressions() == 0);
    /* Monotonic timestamps mean monotonic ns too. */
    for (size_t i = 1; i < n; i++) CHECK(ev[i].timestamp_ns >= ev[i-1].timestamp_ns);
}

/* ---- counting ---------------------------------------------------------- */

/* The count must cover the whole file, not the retained window. This is what
 * makes the number trustworthy regardless of capture size. */
static void test_switch_count_covers_whole_stream(void) {
    printf("  switch count covers whole stream\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    uint64_t t = 2000000ULL;
    for (int i = 0; i < 100; i++) qnx_kernel_trace_test_emit(0, 1, 7, 100 + (i % 3), t++);
    CHECK(qnx_kernel_trace_get_switches() == 99);   /* first is not a switch */

    /* Push far past the ring, then confirm the count still reflects everything. */
    t = 2000000ULL;
    for (int i = 0; i < 50000; i++) qnx_kernel_trace_test_emit(0, 1, 7, 100 + (i % 3), t++);
    CHECK(qnx_kernel_trace_get_events_seen() == 50100);
    CHECK(qnx_kernel_trace_get_switches() == 50099);  /* still every transition */
    size_t retained = qnx_kernel_trace_get_events(NULL, 0);
    CHECK(retained == KERNEL_TRACE_RING);
}

/* Per-CPU: two CPUs alternate independently, so each contributes its own count. */
static void test_counting_is_per_cpu(void) {
    printf("  counting is per-CPU\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    uint64_t t = 2000000ULL;
    for (int i = 0; i < 10; i++) {
        qnx_kernel_trace_test_emit(0, 1, 1, 50 + (i % 2), t++);
        qnx_kernel_trace_test_emit(1, 1, 1, 70 + (i % 2), t++);
    }
    CHECK(qnx_kernel_trace_get_switches() == 18);  /* 9 per CPU */
}

/* ---- the state dump ---------------------------------------------------- */

/* _NTO_TRACE_START emits a burst snapshot of every live thread. Counting it as
 * scheduling is what produced the bogus "~11,700 switches". */
static void test_state_dump_excluded(void) {
    printf("  state dump excluded from the count\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    /* 500 CREATE events inside the first 100 us. */
    uint64_t t = 2000000ULL;
    for (int i = 0; i < 500; i++) qnx_kernel_trace_test_emit(0, 24, 1, 1000 + i, t++);
    /* Real scheduling, well past the 1 ms burst window. */
    for (int i = 0; i < 50; i++) qnx_kernel_trace_test_emit(0, 1, 5000, 2000 + (i % 2), 2000000ULL + 1500000ULL + (uint64_t)i * 1000ULL);
    CHECK(qnx_kernel_trace_get_dump_events() == 500);
    CHECK(qnx_kernel_trace_get_switches() == 49);
}

/* ---- overrun = loss, not inflation ------------------------------------- */

/* Per the SAT guide the kernel "simply drops all events logged for that CPU
 * until that next buffer becomes free", and the resumed timestamps "show a
 * discontinuity". So a gap is MISSING data and the count is a LOWER bound --
 * the opposite of the "upper bound" previously reported. */
static void test_overrun_detected_as_loss(void) {
    printf("  overrun detected as per-CPU loss\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    uint64_t t = 2000000ULL;
    uint64_t first = t;
    for (int i = 0; i < 200; i++) qnx_kernel_trace_test_emit(0, 1, 1, 10 + (i % 3), t++);
    uint64_t before_jump = t;
    t += 75000000ULL;                       /* 50 ms hole on CPU 0 only */
    for (int i = 0; i < 200; i++) qnx_kernel_trace_test_emit(0, 1, 1, 10 + (i % 3), t++);
    /* CPU 1 never stalls, so it must not be implicated. */
    uint64_t t1 = first;
    for (int i = 0; i < 400; i++) qnx_kernel_trace_test_emit(1, 1, 1, 20 + (i % 3), t1++);

    CHECK(qnx_kernel_trace_get_gaps() == 1);
    CHECK(qnx_kernel_trace_get_dropped_cycles() >= 75000000ULL);
    CHECK(!qnx_kernel_trace_is_lossless());
    (void)before_jump;
}

/* A clean capture must report lossless, or the CLI's "measurement" wording
 * would be a false claim. */
static void test_clean_capture_is_lossless(void) {
    printf("  clean capture reports lossless\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    uint64_t t = 2000000ULL;
    for (int i = 0; i < 500; i++) qnx_kernel_trace_test_emit(i % 4, 1, 1, 30 + (i % 5), t++);
    CHECK(qnx_kernel_trace_get_gaps() == 0);
    CHECK(qnx_kernel_trace_get_regressions() == 0);
    CHECK(qnx_kernel_trace_is_lossless());
}

/* ---- ring retention ---------------------------------------------------- */

/* The old code kept the FIRST 32768 events, so on a 24 MB capture the dump at
 * the head pushed every real event out of view. The ring must keep the most
 * recent. */
static void test_ring_keeps_most_recent(void) {
    printf("  ring retains the most recent events\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    int total = KERNEL_TRACE_RING + 1000;
    uint64_t t = 2000000ULL;
    for (int i = 0; i < total; i++) qnx_kernel_trace_test_emit(0, 1, 1, (uint32_t)i, t++);

    KernelTraceEvent *ev = malloc((KERNEL_TRACE_RING) * sizeof(KernelTraceEvent));
    size_t n = qnx_kernel_trace_get_events(ev, KERNEL_TRACE_RING);
    CHECK(n == KERNEL_TRACE_RING);
    CHECK(ev[0].tid == (uint32_t)(total - KERNEL_TRACE_RING));   /* oldest kept */
    CHECK(ev[n-1].tid == (uint32_t)(total - 1));                /* newest kept */
    /* Strictly increasing in time, so the correlator sees them in order. */
    for (size_t i = 1; i < n; i++) CHECK(ev[i].timestamp_cycles > ev[i-1].timestamp_cycles);
    free(ev);
}

/* ---- state names ------------------------------------------------------- */

/* thread_state_name must stay in step with <sys/states.h>, which defines states
 * up to STATE_SEND_NOTIFY (30) with STATE_MAX == 63. The old table stopped at
 * 25 and would have printed UNKNOWN for six real states. */
static void test_state_names_cover_documented_range(void) {
    printf("  state names cover states.h range\n");
    static const char *known[] = {
        "DEAD","RUNNING","READY","STOPPED","SEND","RECEIVE","REPLY",
        "MQ_SEND","MQ_RECEIVE","WAITPAGE","SIGSUSPEND","SIGWAITINFO",
        "NANOSLEEP","MUTEX","CONDVAR","JOIN","INTR","SEM","WAITCTX",
        "RWLOCK_READ","RWLOCK_WRITE","BARRIER","PIPE",
        "<unused: 23>",
        "CREATE","DESTROY","MUON_MUTEX","TRACEBUFFER","INTR_ATTACH_EV",
        "TIMER_DELEGATE","SEND_NOTIFY"
    };
    for (uint32_t s = 0; s <= 30u; s++) {
        if (s == 23u) continue;
        CHECK(strcmp(thread_state_name(s), known[s]) == 0);
    }
    CHECK(strcmp(thread_state_name(31), "UNKNOWN") == 0);
    CHECK(strcmp(thread_state_name(63), "UNKNOWN") == 0);
}

/* ---- ring wrap must not confuse the dump window ------------------------ */

/* The dump window is located from g_first_cycles, not g_ring[0], because once
 * the ring wraps g_ring[0] is no longer the first event of the file. */
static void test_dump_window_survives_ring_wrap(void) {
    printf("  dump window survives ring wrap\n");
    qnx_kernel_trace_test_reset();
    qnx_kernel_trace_test_set_clock(CPS, 0, 0, false);
    uint64_t t = 2000000ULL;
    for (int i = 0; i < 500; i++) qnx_kernel_trace_test_emit(0, 24, 1, 1000 + i, t++);
    CHECK(qnx_kernel_trace_get_dump_events() == 500);
    /* Now overflow the ring many times over. */
    t += 5000000ULL;
    for (int i = 0; i < KERNEL_TRACE_RING + 500; i++) qnx_kernel_trace_test_emit(0, 1, 1, (uint32_t)i, t++);
    /* The recorded dump count must not have been rewritten by the wrap. */
    CHECK(qnx_kernel_trace_get_dump_events() == 500);
    CHECK(qnx_kernel_trace_get_events_seen() == 500 + KERNEL_TRACE_RING + 500);
}

int main(void) {
    printf("=== PARSER UNIT TESTS ===\n");
    test_rollover_reassembly();
    test_switch_count_covers_whole_stream();
    test_counting_is_per_cpu();
    test_state_dump_excluded();
    test_overrun_detected_as_loss();
    test_clean_capture_is_lossless();
    test_ring_keeps_most_recent();
    test_state_names_cover_documented_range();
    test_dump_window_survives_ring_wrap();

    if (failures) {
        printf("FAILED: %d check(s)\n", failures);
        return 1;
    }
    printf("All parser unit tests passed.\n");
    return 0;
}