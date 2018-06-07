#ifndef _FIFO_PAYLOAD_
#define _FIFO_PAYLOAD_

#include <time.h>

typedef struct FIFO_PAYLOAD_T {
    int lusid;
    struct timespec ts_start;
    struct timespec ts_end;
} fifo_payload_t;

#endif // _FIFO_PAYLOAD_