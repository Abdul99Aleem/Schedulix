#include "can_frame.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <time.h>
  #include <errno.h>
#endif

/* Monotonic timestamp in nanoseconds */
uint64_t can_get_monotonic_ns(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    static int freq_init = 0;
    if (!freq_init) {
        QueryPerformanceFrequency(&freq);
        freq_init = 1;
    }
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    /* convert to ns: cnt * 1e9 / freq */
    return (uint64_t)((cnt.QuadPart * 1000000000ULL) / (uint64_t)freq.QuadPart);
#else
    struct timespec ts;
    /* CLOCK_MONOTONIC is required for timing analysis; never use wall-clock */
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
#endif
    /* Fallback: try CLOCK_REALTIME if monotonic unavailable (should not happen on QNX) */
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    return 0;
#endif
}

void can_frame_set_timestamp(can_frame_t *frame) {
    if (frame) frame->timestamp_ns = can_get_monotonic_ns();
}

int can_frame_is_valid(const can_frame_t *frame) {
    if (!frame) return 0;
    if (frame->dlc > 8) return 0;
    /* Standard CAN id 11-bit (0..0x7FF) or Extended 29-bit (0..0x1FFFFFFF).
       We accept up to 29-bit for simulation. */
    if (frame->id > 0x1FFFFFFF) return 0;
    return 1;
}

void can_frame_print(const can_frame_t *frame) {
    if (!frame) return;
    printf("CAN FRAME\n");
    printf("---------\n");
    printf("ID        : 0x%03X\n", (unsigned)frame->id);
    printf("DLC       : %u\n", (unsigned)frame->dlc);
    printf("DATA      :");
    if (frame->dlc == 0) {
        printf(" <empty>");
    } else {
        for (int i = 0; i < frame->dlc; i++) {
            printf(" %02X", frame->data[i]);
        }
    }
    printf("\n");
    printf("TIMESTAMP : %" PRIu64 " ns\n", frame->timestamp_ns);
}

/*
 * Parse a line like:
 *   "100 1 01"
 *   "0x100 3 AA BB CC"
 *   "200 1 01"
 *   "300 2 01 02"
 *   "999 1 01"  (non-hex id also allowed as decimal if no 0x)
 * Returns 0 on success, -1 on parse error.
 */
int can_frame_from_string(const char *line, can_frame_t *out) {
    if (!line || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* skip leading whitespace */
    while (*line && isspace((unsigned char)*line)) line++;
    if (*line == '\0' || *line == '#') return -1;

    char *copy = NULL;
    size_t len = strlen(line);
    copy = (char*)malloc(len + 1);
    if (!copy) return -1;
    strcpy(copy, line);

    /* Tokenize */
    char *save = NULL;
#ifdef _WIN32
    char *tok = strtok_s(copy, " \t\r\n", &save);
#else
    char *tok = strtok_r(copy, " \t\r\n", &save);
#endif
    int idx = 0;
    uint32_t id = 0;
    int dlc = -1;
    uint8_t data[8] = {0};
    int data_idx = 0;

    while (tok) {
        if (idx == 0) {
            char *end = NULL;
            unsigned long v = strtoul(tok, &end, 0); /* base 0: auto hex/dec */
            if (end == tok) { free(copy); return -1; }
            id = (uint32_t)v;
        } else if (idx == 1) {
            char *end = NULL;
            long v = strtol(tok, &end, 0);
            if (end == tok || v < 0 || v > 8) { free(copy); return -1; }
            dlc = (int)v;
        } else {
            if (data_idx >= 8) { free(copy); return -1; }
            char *end = NULL;
            unsigned long v = strtoul(tok, &end, 16); /* data always hex */
            /* Also allow 0x prefix; strtoul with base 16 handles 0x but we pass 16, so handle 0x manually */
            if (end == tok) {
                /* try base 0 fallback */
                v = strtoul(tok, &end, 0);
                if (end == tok) { free(copy); return -1; }
            }
            if (v > 0xFF) { free(copy); return -1; }
            data[data_idx++] = (uint8_t)v;
        }
        idx++;
#ifdef _WIN32
        tok = strtok_s(NULL, " \t\r\n", &save);
#else
        tok = strtok_r(NULL, " \t\r\n", &save);
#endif
    }
    free(copy);

    if (idx < 2) return -1; /* need at least id + dlc */
    if (dlc < 0) return -1;
    if (data_idx != dlc) {
        /* If DLC says 1 but we parsed 1 byte, ok; if mismatch, error */
        return -1;
    }
    out->id = id;
    out->dlc = (uint8_t)dlc;
    memcpy(out->data, data, dlc);
    /* timestamp will be set by caller via can_frame_set_timestamp if zero */
    if (out->timestamp_ns == 0) can_frame_set_timestamp(out);
    return can_frame_is_valid(out) ? 0 : -1;
}
