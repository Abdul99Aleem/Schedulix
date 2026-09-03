#ifndef REPLAY_H
#define REPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Scenario replay: re-create event sequence from trace file */
int replay_from_trace(const char *trace_path);
int replay_verify(const char *trace_path);

#ifdef __cplusplus
}
#endif

#endif
