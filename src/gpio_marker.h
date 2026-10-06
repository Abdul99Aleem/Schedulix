#ifndef GPIO_MARKER_H
#define GPIO_MARKER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 4/5 GPIO validation channel
 * START → GPIO HIGH → exec → GPIO LOW → END
 * Independent of software trace; validates instrumentation.
 */

int  gpio_marker_init(void); /* map pins, open /dev/gpio if present */
void gpio_marker_shutdown(void);

/* Task-specific helpers (pin mapping internal) */
void gpio_marker_for_task_high(uint32_t task_id);
void gpio_marker_for_task_low(uint32_t task_id);

/* Generic pin control (BCM numbering) */
int  gpio_marker_set(int bcm_pin, int value); /* 0/1, -1 on no HW */
int  gpio_marker_get(int bcm_pin);

/* Validation: compare software exec interval vs GPIO pulse width */
typedef struct {
    uint64_t sw_start_ns;
    uint64_t sw_end_ns;
    uint64_t gpio_high_ns;
    uint64_t gpio_low_ns;
    int64_t  delta_ns; /* (gpio - sw) */

    /* Hardware confirmation. Previously validation compared one software
     * timestamp against another, which is vacuous -- it could never fail.
     * These come from reading GPLEV back after each drive, so they report
     * what the pin actually did rather than what we asked for.
     */
    int  hw_level_high;  /* GPLEV sampled immediately after driving high */
    int  hw_level_low;   /* GPLEV sampled immediately after driving low   */
    bool hw_confirmed;   /* both readbacks matched the driven level       */

    bool valid;          /* within tolerance AND confirmed on hardware  */
} gpio_validation_t;

gpio_validation_t gpio_marker_validate(uint32_t task_id, uint64_t sw_start, uint64_t sw_end);
bool gpio_marker_is_available(void); /* true if real HW mapped */

/* Last GPLEV value sampled on a marker pin, or -1 if unknown. */
int  gpio_marker_last_observed(uint32_t task_id);

/* Force a pin to output mode now (writes GPFSELn). Returns 0 on success. */
int  gpio_marker_configure_output(int bcm_pin);

/* Current GPFSEL field value for a pin: 0=input 1=output 4-7=alt0-3. */
int  gpio_marker_fsel(int bcm_pin);

/* For analyzer: record GPIO events into trace as well */
int gpio_marker_trace_high(uint32_t task_id, uint32_t act, uint32_t corr);
int gpio_marker_trace_low(uint32_t task_id, uint32_t act, uint32_t corr);

#ifdef __cplusplus
}
#endif

#endif
