#include "can_decoder.h"
#include <string.h>

sched_event_t decode_can_frame(const can_frame_t *frame) {
    sched_event_t ev;
    memset(&ev, 0, sizeof(ev));

    if (!frame) {
        ev.type = EVENT_UNKNOWN;
        return ev;
    }

    ev.can_id = frame->id;
    ev.data_len = frame->dlc > 8 ? 8 : frame->dlc;
    memcpy(ev.data, frame->data, ev.data_len);
    ev.timestamp_ns = frame->timestamp_ns;

    switch (frame->id) {
        case 0x100: ev.type = EVENT_BRAKE;      break;
        case 0x200: ev.type = EVENT_ADAS;       break;
        case 0x300: ev.type = EVENT_DIAGNOSTIC; break;
        default:    ev.type = EVENT_UNKNOWN;    break;
    }
    return ev;
}
