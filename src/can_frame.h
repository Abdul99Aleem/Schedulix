#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Logical CAN frame abstraction.
 * This is the single "can_frame_t" that ALL input sources must produce:
 *   - simulated_can_adapter.c  (tonight, no hardware)
 *   - qnx_can_adapter.c        (tomorrow, real QNX CAN driver)
 *   - can_injector.c TCP       (optional laptop -> QNX injection)
 *
 * The decoder and Event Manager never see the source; they only see can_frame_t.
 */

typedef struct {
    uint32_t id;            /* 11-bit or 29-bit CAN identifier */
    uint8_t  dlc;           /* Data Length Code: 0..8 */
    uint8_t  data[8];       /* Payload bytes, only dlc bytes are valid */
    uint64_t timestamp_ns;  /* Monotonic timestamp (ns since arbitrary epoch) */
} can_frame_t;

/* Timestamp helper: fill frame->timestamp_ns with monotonic clock */
uint64_t can_get_monotonic_ns(void);
void     can_frame_set_timestamp(can_frame_t *frame);

/* Validation + pretty-print helpers */
int      can_frame_is_valid(const can_frame_t *frame);
void     can_frame_print(const can_frame_t *frame);

/* Text parser for TCP injection: "100 1 01" or "0x100 3 AA BB CC" */
int can_frame_from_string(const char *line, can_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CAN_FRAME_H */
