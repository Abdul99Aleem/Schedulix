#include "replay.h"
#include "trace_schema.h"
#include <stdio.h>
#include <stdlib.h>

int replay_from_trace(const char *trace_path){
    FILE *f=fopen(trace_path,"rb"); if(!f) return -1;
    trace_shm_header_t hdr; fread(&hdr,sizeof(hdr),1,f);
    printf("[REPLAY] header magic %08X ver %u cap %u written %llu\n", hdr.magic,hdr.version,hdr.capacity,(unsigned long long)hdr.records_written);
    sched_trace_record_t rec;
    size_t n=0;
    while(fread(&rec,sizeof(rec),1,f)==1){
        printf("[%zu] %s task %u act %u corr %u ts %llu arg0 %llu\n", n++,
            trace_event_to_string(rec.event_type), rec.task_id, rec.activation_id, rec.correlation_id,
            (unsigned long long)rec.timestamp_ns,(unsigned long long)rec.arg0);
        if(n>20){ printf("... truncated\n"); break; }
    }
    fclose(f); return 0;
}
int replay_verify(const char *trace_path){ return replay_from_trace(trace_path); }
