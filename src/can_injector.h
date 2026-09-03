#ifndef CAN_INJECTOR_H
#define CAN_INJECTOR_H

#include "can_frame.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional TCP injection layer.
 *
 * Windows laptop  --TCP:5000-->  QNX / VMware QNX  --> can_frame_t --> decoder
 *
 * Laptop sends lines like:
 *   100 1 01
 *   200 1 01
 *   0x100 3 AA BB CC
 *
 * This header is optional. Local simulator must work without it.
 */

int  can_injector_parse_line(const char *line, can_frame_t *out);

/* Blocking TCP server: listens on port, for each received line decodes+sends to callback */
typedef void (*can_injector_callback_t)(const can_frame_t *frame, void *user);

int can_injector_run_server(uint16_t port, can_injector_callback_t cb, void *user);

/* One-shot helpers */
int can_injector_receive_from_socket(int fd, can_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CAN_INJECTOR_H */
