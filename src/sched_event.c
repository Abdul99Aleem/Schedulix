#include "sched_event.h"
#include <stdio.h>
#include <inttypes.h>

const char* event_type_to_string(event_type_t type) {
    switch (type) {
        case EVENT_BRAKE:      return "BRAKE";
        case EVENT_ADAS:       return "ADAS";
        case EVENT_DIAGNOSTIC: return "DIAGNOSTIC";
        case EVENT_UNKNOWN:
        default:               return "UNKNOWN";
    }
}

void sched_event_print(const sched_event_t *event) {
    if (!event) return;
    printf("SCHEDULIX EVENT\n");
    printf("---------------\n");
    printf("TYPE      : %s\n", event_type_to_string(event->type));
    printf("CAN ID    : 0x%03X\n", (unsigned)event->can_id);
    printf("DATA LEN  : %u\n", (unsigned)event->data_len);
    printf("DATA      :");
    if (event->data_len == 0) {
        printf(" <empty>");
    } else {
        for (int i = 0; i < event->data_len; i++) {
            printf(" %02X", event->data[i]);
        }
    }
    printf("\n");
    printf("TIMESTAMP : %" PRIu64 " ns\n", event->timestamp_ns);
}
