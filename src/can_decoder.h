#ifndef CAN_DECODER_H
#define CAN_DECODER_H

#include "can_frame.h"
#include "sched_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CAN -> Schedulix decoder.
 * Pure function: no global state, no I/O, no QNX dependency.
 * This file never needs rewriting when CAN hardware is added.
 *
 * Mapping (initial automotive set):
 *   0x100 -> EVENT_BRAKE
 *   0x200 -> EVENT_ADAS
 *   0x300 -> EVENT_DIAGNOSTIC
 *   else  -> EVENT_UNKNOWN
 */

sched_event_t decode_can_frame(const can_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DECODER_H */
