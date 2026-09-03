#include "can_adapter.h"
#include "can_frame.h"
#include <string.h>
#include <stddef.h>

/*
 * Simulated CAN adapter – no hardware required.
 * Produces the exact can_frame_t that a real QNX CAN driver would produce.
 *
 * Tomorrow: replace this .c file with qnx_can_adapter.c that implements
 * the same can_adapter_receive() by talking to the real QNX CAN driver.
 * No decoder or Event Manager changes required.
 */

static const can_frame_t k_sim_frames[] = {
    /* Required minimum tests */
    { .id = 0x100, .dlc = 1, .data = {0x01} },
    { .id = 0x200, .dlc = 1, .data = {0x01} },
    { .id = 0x300, .dlc = 1, .data = {0x01} },
    { .id = 0x999, .dlc = 1, .data = {0x01} }, /* UNKNOWN */
    /* Multi-byte payload test */
    { .id = 0x100, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} },
};

static size_t g_idx = 0;
static int    g_initialized = 0;

void can_adapter_init(void) {
    g_idx = 0;
    g_initialized = 1;
}

void can_adapter_shutdown(void) {
    g_initialized = 0;
}

/* Return 0 and fill *frame if a simulated frame is available, -1 when exhausted */
int can_adapter_receive(can_frame_t *frame) {
    if (!frame) return -1;
    if (!g_initialized) can_adapter_init();

    if (g_idx >= sizeof(k_sim_frames)/sizeof(k_sim_frames[0])) {
        return -1; /* no more frames */
    }

    *frame = k_sim_frames[g_idx++];
    /* Apply monotonic timestamp at receive time (like real driver would) */
    can_frame_set_timestamp(frame);
    return 0;
}

int can_adapter_poll(can_frame_t *frame, int timeout_ms) {
    (void)timeout_ms; /* simulated adapter ignores timeout */
    return can_adapter_receive(frame);
}

/* Helper for test harness: reset to beginning */
void simulated_can_adapter_reset(void) {
    g_idx = 0;
}

/* Helper: number of simulated frames */
size_t simulated_can_adapter_count(void) {
    return sizeof(k_sim_frames)/sizeof(k_sim_frames[0]);
}
