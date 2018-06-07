#ifndef _FIFO_H_
#define _FIFO_H_

#include <stdbool.h>
#include <time.h>
#include "fifo_payload.h"

#define RT_CLOCK CLOCK_REALTIME


typedef struct FIFO_T {
  fifo_payload_t *data;
  int head;
  int tail;
  int size;
} fifo_t;

fifo_t *fifo_create(int size);
void fifo_destroy(fifo_t *f);

int fifo_read(fifo_t *f, fifo_payload_t *data, int ndata);
int fifo_write(fifo_t *f, const fifo_payload_t *data, int ndata);
bool fifo_data_available(fifo_t *f);
void fifo_clear(fifo_t *f);

#endif //_FIFO_H_