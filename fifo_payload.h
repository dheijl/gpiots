#ifndef _FIFO_PAYLOAD_
#define _FIFO_PAYLOAD_

#include <stdint.h>

typedef int64_t time64_t;
struct timespec64 {
	time64_t	tv_sec;			/* seconds */
	long		tv_nsec;		/* nanoseconds */
};

typedef struct FIFO_PAYLOAD_T {
    int lusid;
    struct timespec64 ts_start;
    struct timespec64 ts_end;
} fifo_payload_t;

#endif // _FIFO_PAYLOAD_