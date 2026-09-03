#ifndef SCHED_EVENT_H
#define SCHED_EVENT_H

#include <stdint.h>
#include "can_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVENT_UNKNOWN    = 0,
    EVENT_BRAKE      = 1,
    EVENT_ADAS       = 2,
    EVENT_DIAGNOSTIC = 3
} event_type_t;

typedef struct {
    uint32_t     can_id;
    event_type_t type;
    uint8_t      data[8];
    uint8_t      data_len;
    uint64_t     timestamp_ns;
} sched_event_t;

/* Human-readable names */
const char* event_type_to_string(event_type_t type);
void        sched_event_print(const sched_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* SCHED_EVENT_H */
