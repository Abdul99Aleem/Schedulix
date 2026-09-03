/* Minimal host-side tests for decoder + simulator.
 * Compiles with QNX qcc and also with any host gcc/tcc.
 * Run: ./build/<config>/schedulix_can  (demo)  or build this test separately.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/can_decoder.h"
#include "../src/sched_event.h"
#include "../src/can_frame.h"

static int tests_run = 0, tests_pass = 0;

#define TEST(name) do { tests_run++; printf("TEST %s ... ", #name); } while(0)
#define PASS() do { tests_pass++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

void test_brake(void) {
    TEST(brake);
    can_frame_t f = { .id=0x100, .dlc=1, .data={0x01}, .timestamp_ns=123 };
    sched_event_t e = decode_can_frame(&f);
    if (e.type != EVENT_BRAKE) { FAIL("expected BRAKE"); return; }
    if (e.can_id != 0x100) { FAIL("can_id"); return; }
    if (e.data_len != 1 || e.data[0] != 0x01) { FAIL("data"); return; }
    PASS();
}
void test_adas(void) {
    TEST(adas);
    can_frame_t f = { .id=0x200, .dlc=1, .data={0x01}, .timestamp_ns=456 };
    sched_event_t e = decode_can_frame(&f);
    if (e.type != EVENT_ADAS) { FAIL("expected ADAS"); return; }
    PASS();
}
void test_diagnostic(void) {
    TEST(diagnostic);
    can_frame_t f = { .id=0x300, .dlc=1, .data={0x01} };
    sched_event_t e = decode_can_frame(&f);
    if (e.type != EVENT_DIAGNOSTIC) { FAIL("expected DIAGNOSTIC"); return; }
    PASS();
}
void test_unknown(void) {
    TEST(unknown);
    can_frame_t f = { .id=0x999, .dlc=1, .data={0x01} };
    sched_event_t e = decode_can_frame(&f);
    if (e.type != EVENT_UNKNOWN) { FAIL("expected UNKNOWN"); return; }
    PASS();
}
void test_multi_byte(void) {
    TEST(multi_byte);
    can_frame_t f = { .id=0x100, .dlc=3, .data={0xAA,0xBB,0xCC} };
    sched_event_t e = decode_can_frame(&f);
    if (e.type != EVENT_BRAKE) { FAIL("type"); return; }
    if (e.data_len != 3 || e.data[0]!=0xAA || e.data[1]!=0xBB || e.data[2]!=0xCC) { FAIL("data"); return; }
    PASS();
}
void test_timestamp_preserved(void) {
    TEST(timestamp);
    can_frame_t f = { .id=0x100, .dlc=1, .data={0x01}, .timestamp_ns=999999 };
    sched_event_t e = decode_can_frame(&f);
    if (e.timestamp_ns != 999999) { FAIL("timestamp not preserved"); return; }
    PASS();
}
void test_parse_string(void) {
    TEST(parse_string);
    can_frame_t f;
    if (can_frame_from_string("0x100 1 01", &f)!=0 || f.id!=0x100 || f.dlc!=1 || f.data[0]!=0x01) { FAIL("parse 0x100"); return; }
    if (can_frame_from_string("100 1 01", &f)!=0 || f.id!=100) { FAIL("parse dec"); return; }
    if (can_frame_from_string("0x100 3 AA BB CC", &f)!=0 || f.dlc!=3 || f.data[2]!=0xCC) { FAIL("parse multi"); return; }
    if (can_frame_from_string("0x999 1 01", &f)!=0 || f.id!=0x999) { FAIL("parse 0x999"); return; }
    PASS();
}

int main(void) {
    printf("Schedulix decoder tests\n");
    printf("=======================\n");
    test_brake();
    test_adas();
    test_diagnostic();
    test_unknown();
    test_multi_byte();
    test_timestamp_preserved();
    test_parse_string();
    printf("\n%d/%d tests passed\n", tests_pass, tests_run);
    return tests_pass == tests_run ? 0 : 1;
}
