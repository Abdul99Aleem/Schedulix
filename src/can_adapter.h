#ifndef CAN_ADAPTER_H
#define CAN_ADAPTER_H

#include "can_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Modular CAN input interface.
 *
 *                 INPUT SOURCES
 *                     |
 *        +------------+------------+
 *        |            |            |
 *    Real CAN     Simulator      TCP/UART
 *    adapter
 *        |            |            |
 *        +------------+------------+
 *                     |
 *                can_frame_t
 *                     |
 *                  Decoder
 *                     |
 *               sched_event_t
 *                     |
 *               Event Manager
 *
 * Contract:
 *   int can_adapter_receive(can_frame_t *frame)
 *     0  -> frame filled
 *     -1 -> no frame available / error / EOF
 *
 * Implementations:
 *   - simulated_can_adapter.c  : tonight (software queue)
 *   - qnx_can_adapter.c        : tomorrow (real QNX CAN driver, see QNX_CAN_STATUS below)
 *
 * Build selection: link exactly ONE adapter implementation.
 * Default for this deliverable: simulated_can_adapter.c
 */

int  can_adapter_receive(can_frame_t *frame);
void can_adapter_init(void);
void can_adapter_shutdown(void);

/* Optional non-blocking poll with timeout (ms). Default impl ignores timeout. */
int can_adapter_poll(can_frame_t *frame, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* CAN_ADAPTER_H */
