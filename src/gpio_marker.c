/* GPIO timing markers for instrument validation.
 *
 * Purpose: prove the software timing instrumentation is honest by comparing
 * what the code *says* happened against what the pin *actually* did.
 * START -> GPIO HIGH -> workload executes -> GPIO LOW -> END, with an external
 * logic analyzer or LED/scope on the wire.
 *
 * Implementation notes that matter
 * --------------------------------
 * 1. GPFSELn must be configured for output before GPSET0/GPCLR0 do anything.
 *    At reset every GPFSELn field is 000 = input, so driving the set/clear
 *    registers on an unconfigured pin produces no electrical output at all.
 *    The previous implementation never wrote GPFSEL, which is why the markers
 *    could never have produced a visible edge.
 *
 * 2. The registers are reached with mmap(MAP_PHYS), not /dev/mem. Verified on
 *    the target: there is no /dev/mem in this QNX 8.0.0 image. The pattern
 *    matches the BSP's own GPIO driver, which is what gpio-bcm2715 exposes
 *    through /dev/gpioN on the same board.
 *
 * 3. Only three pins are marker pins (one per workload). They are indexed by
 *    task, so the trace-time arrays are sized 3, not 16.
 *
 * Measured state of the three marker pins on the target before this rewrite,
 * from `gpio-bcm2711 get`:
 *     GPIO  4: level=1 fsel=3 alt=4 func=TXD3  pull=UP    <- alt function!
 *     GPIO 17: level=0 fsel=0     func=INPUT  pull=DOWN
 *     GPIO 27: level=0 fsel=0     func=INPUT  pull=DOWN
 * All three were inputs (or an alt function), confirming no output was possible.
 */

#include "gpio_marker.h"
#include "trace_instrumentation.h"
#include "trace_schema.h"
#include "workload_config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

/* mmap/munmap are needed on host builds too -- gpio_marker_shutdown() is not
 * inside a QNX guard, so the declaration must always be visible. */
#include <sys/mman.h>
#include <errno.h>

#if defined(__QNX__) || defined(__QNXNTO__)
#include <sys/neutrino.h>  /* NOFD, __PAGESIZE */
#else
#ifndef __PAGESIZE
#define __PAGESIZE 4096   /* host builds have no QNX page macro */
#endif
#endif

/* ---- BCM2711 GPIO register map ---------------------------------------
 * Physical base 0xFE200000. Offsets are in bytes.
 *   GPFSELn : function select, 10 pins per 32-bit register, 3 bits per pin
 *             000 = input, 001 = output, 100..111 = alt0..alt3
 *   GPSETn  : write 1 to set        GPCLRn : write 1 to clear
 *   GPLEVn  : pin level (read only)
 * GPSET0/GPCLR0/GPLEV0 cover GPIO 0-31; the bank at +4 covers 32-53.
 */
#define GPIO_PHYS_BASE   0xFE200000ul
#define GPFSEL0_OFF      0x00ul
#define GPSET0_OFF       0x1Cul
#define GPCLR0_OFF       0x28ul
#define GPLEV0_OFF       0x34ul

#define GPIO_MAX_PIN     53      /* BCM2711 valid range */
#define GPIO_BANK_SIZE   32      /* pins per set/clr/lev register */
#define GPFSEL_PINS_REG  10      /* pins per fsel register */
#define GPFSEL_OUT       0x1u    /* 001 = output */
#define MARKER_SLOTS     3

/* Marker pin per task. BCM numbering == header pin - 2 for this range:
 *   BCM 4  = header pin 7   -> BRAKE_CTL
 *   BCM 17 = header pin 11  -> ADAS_FUSION
 *   BCM 27 = header pin 13  -> DIAG_POLL
 */
static const int g_task_pin[MARKER_SLOTS] = { 4, 17, 27 };

/* Software-side edge timestamps, per marker slot. */
static uint64_t g_high_ts[MARKER_SLOTS];
static uint64_t g_low_ts[MARKER_SLOTS];

/* Hardware-side confirmation: level read back immediately after each drive.
 * -1 means "not sampled", which is what mock mode reports. */
static int g_hw_level_high[MARKER_SLOTS];
static int g_hw_level_low[MARKER_SLOTS];

#if defined(__QNX__) || defined(__QNXNTO__)
static volatile uint32_t *g_gpio_base = NULL;
#else
static volatile uint32_t *g_gpio_base = NULL;
#endif

static uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Map a marker slot (0..2) to its BCM pin, or -1. */
static int slot_to_pin(int slot) {
    if (slot < 0 || slot >= MARKER_SLOTS) return -1;
    return g_task_pin[slot];
}

static int task_to_slot(uint32_t task_id) {
    switch (task_id) {
        case TASK_ID_BRAKE: return 0;
        case TASK_ID_ADAS:  return 1;
        case TASK_ID_DIAG:  return 2;
        default: return -1;
    }
}

static int pin_to_slot(int bcm_pin) {
    for (int i = 0; i < MARKER_SLOTS; i++) {
        if (g_task_pin[i] == bcm_pin) return i;
    }
    return -1;
}

int gpio_marker_init(void) {
    for (int i = 0; i < MARKER_SLOTS; i++) {
        g_high_ts[i] = 0; g_low_ts[i] = 0;
        g_hw_level_high[i] = -1; g_hw_level_low[i] = -1;
    }

#if defined(__QNX__) || defined(__QNXNTO__)
    /* Physical mapping. /dev/mem does not exist in this image, so
     * MAP_PHYS is the only route to the GPIO block. MAP_SHARED is required
     * for device memory; PROT_NOCACHE avoids having to manage cache
     * coherency against register writes. */
    void *map = mmap(NULL, __PAGESIZE,
                     PROT_NOCACHE | PROT_READ | PROT_WRITE,
                     MAP_PHYS | MAP_SHARED, NOFD,
                     (off_t)GPIO_PHYS_BASE);
    if (map == MAP_FAILED) {
        g_gpio_base = NULL;
        fprintf(stderr, "[GPIO] mmap(MAP_PHYS 0x%08lx) failed: %s\n",
                GPIO_PHYS_BASE, strerror(errno));
        fprintf(stderr, "[GPIO] no HW, mock mode (validation uses sw timestamps)\n");
        return 0;
    }
    g_gpio_base = (volatile uint32_t *)map;
    fprintf(stderr, "[GPIO] BCM2711 registers mapped at 0x%08lx\n", GPIO_PHYS_BASE);
#else
    fprintf(stderr, "[GPIO] host build, mock mode\n");
#endif
    return 0;
}

void gpio_marker_shutdown(void) {
    /* Return marker pins to input so they stop driving the header. Leaving a
     * pin driven after the process exits is a hazard on a shared bus. */
    for (int i = 0; i < MARKER_SLOTS; i++) {
        int pin = g_task_pin[i];
        (void)gpio_marker_set(pin, 0);
#if defined(__QNX__) || defined(__QNXNTO__)
        if (g_gpio_base) {
            /* fsel field for this pin -> 000 (input) */
            int reg   = pin / GPFSEL_PINS_REG;
            int shift = (pin % GPFSEL_PINS_REG) * 3;
            g_gpio_base[(GPFSEL0_OFF / 4) + reg] &= ~(7u << shift);
        }
#endif
    }
    if (g_gpio_base) {
        munmap((void *)g_gpio_base, __PAGESIZE);
        g_gpio_base = NULL;
    }
}

int gpio_marker_configure_output(int bcm_pin) {
    if (bcm_pin < 0 || bcm_pin > GPIO_MAX_PIN) return -1;
#if defined(__QNX__) || defined(__QNXNTO__)
    if (!g_gpio_base) return -1;
    int reg   = bcm_pin / GPFSEL_PINS_REG;
    int shift = (bcm_pin % GPFSEL_PINS_REG) * 3;
    volatile uint32_t *r = g_gpio_base + (GPFSEL0_OFF / 4) + reg;
    /* Read-modify-write: the register packs ten pins into 32 bits. */
    *r = (*r & ~(7u << shift)) | (GPFSEL_OUT << shift);
    return 0;
#else
    (void)bcm_pin;
    return -1;
#endif
}

int gpio_marker_fsel(int bcm_pin) {
    if (bcm_pin < 0 || bcm_pin > GPIO_MAX_PIN) return -1;
#if defined(__QNX__) || defined(__QNXNTO__)
    if (!g_gpio_base) return -1;
    int reg   = bcm_pin / GPFSEL_PINS_REG;
    int shift = (bcm_pin % GPFSEL_PINS_REG) * 3;
    return (int)((g_gpio_base[(GPFSEL0_OFF / 4) + reg] >> shift) & 7u);
#else
    (void)bcm_pin;
    return -1;
#endif
}

int gpio_marker_get(int bcm_pin) {
    if (bcm_pin < 0 || bcm_pin > GPIO_MAX_PIN) return -1;
#if defined(__QNX__) || defined(__QNXNTO__)
    if (!g_gpio_base) return -1;
    /* GPLEV0 covers GPIO 0-31, +4 covers 32-53. */
    volatile uint32_t *r =
        g_gpio_base + (GPLEV0_OFF / 4) + (bcm_pin / GPIO_BANK_SIZE);
    return ((*r >> (bcm_pin % GPIO_BANK_SIZE)) & 1u) ? 1 : 0;
#else
    (void)bcm_pin;
    return -1;
#endif
}

int gpio_marker_set(int bcm_pin, int value) {
    if (bcm_pin < 0 || bcm_pin > GPIO_MAX_PIN) return -1;

    int slot = pin_to_slot(bcm_pin);
    uint64_t t = mono_ns();
    if (value) { if (slot >= 0) g_high_ts[slot] = t; }
    else       { if (slot >= 0) g_low_ts[slot]  = t; }

#if defined(__QNX__) || defined(__QNXNTO__)
    if (!g_gpio_base) return -1;  /* mock */

    /* Configure as output on first use. Writing GPSET0 while the pin is
     * still an input does nothing at the pad -- this is the bug that made the
     * old markers invisible. */
    if (gpio_marker_configure_output(bcm_pin) != 0) return -1;

    volatile uint32_t *r =
        g_gpio_base + ((value ? GPSET0_OFF : GPCLR0_OFF) / 4)
                   + (bcm_pin / GPIO_BANK_SIZE);
    *r = 1u << (bcm_pin % GPIO_BANK_SIZE);

    /* Read the level straight back. A pin configured as output should read
     * back as driven; if it reads the opposite, the function-select write did
     * not take effect and the marker is not really on the wire. */
    int level = gpio_marker_get(bcm_pin);
    if (slot >= 0) {
        if (value) g_hw_level_high[slot] = level;
        else       g_hw_level_low[slot]  = level;
    }
    return 0;
#else
    return -1;  /* mock */
#endif
}

void gpio_marker_for_task_high(uint32_t task_id) {
    int pin = slot_to_pin(task_to_slot(task_id));
    if (pin < 0) return;
    gpio_marker_set(pin, 1);
}

void gpio_marker_for_task_low(uint32_t task_id) {
    int pin = slot_to_pin(task_to_slot(task_id));
    if (pin < 0) return;
    gpio_marker_set(pin, 0);
}

int gpio_marker_last_observed(uint32_t task_id) {
    int slot = task_to_slot(task_id);
    if (slot < 0) return -1;
    /* Prefer the most recent drive's readback. */
    if (g_low_ts[slot]  && g_low_ts[slot]  >= g_high_ts[slot]) return g_hw_level_low[slot];
    return g_hw_level_high[slot];
}

int gpio_marker_trace_high(uint32_t task_id, uint32_t act, uint32_t corr) {
    gpio_marker_for_task_high(task_id);
    /* Dedicated event type. Previously this emitted TRACE_CPU_MIGRATION,
     * which analyzer.c has no case for, so every marker was silently
     * discarded by the default branch. */
    return trace_emit(TRACE_GPIO_MARKER_HIGH, task_id, act, corr, 1, 0);
}

int gpio_marker_trace_low(uint32_t task_id, uint32_t act, uint32_t corr) {
    gpio_marker_for_task_low(task_id);
    return trace_emit(TRACE_GPIO_MARKER_LOW, task_id, act, corr, 0, 0);
}

bool gpio_marker_is_available(void) { return g_gpio_base != NULL; }

gpio_validation_t gpio_marker_validate(uint32_t task_id,
                                       uint64_t sw_start, uint64_t sw_end) {
    gpio_validation_t v;
    memset(&v, 0, sizeof(v));
    v.sw_start_ns = sw_start;
    v.sw_end_ns   = sw_end;
    v.hw_level_high = -1;
    v.hw_level_low  = -1;

    int slot = task_to_slot(task_id);
    if (slot >= 0) {
        v.gpio_high_ns = g_high_ts[slot];
        v.gpio_low_ns  = g_low_ts[slot];
        v.hw_level_high = g_hw_level_high[slot];
        v.hw_level_low  = g_hw_level_low[slot];
    }

    uint64_t sw_dur   = (sw_end > sw_start) ? sw_end - sw_start : 0;
    uint64_t gpio_dur = (v.gpio_low_ns > v.gpio_high_ns)
                        ? v.gpio_low_ns - v.gpio_high_ns : 0;
    v.delta_ns = (int64_t)gpio_dur - (int64_t)sw_dur;

    /* Hardware confirmation: an output pin must read back the driven level.
     * Without this, validation compares software timestamps against software
     * timestamps and can never fail. */
    v.hw_confirmed = (v.hw_level_high == 1) && (v.hw_level_low == 0);

    /* Tolerance per the instrumentation spec: 50us absolute + 5% relative. */
    int64_t tol  = 50000 + (int64_t)(sw_dur / 20);
    int64_t absd = v.delta_ns < 0 ? -v.delta_ns : v.delta_ns;
    v.valid = (absd <= tol) && v.hw_confirmed;
    return v;
}