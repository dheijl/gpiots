#ifndef _GPIO_FIFO_H_
#define _GPIO_FIFO_H_

#include <linux/time.h>

#define RT_CLOCK CLOCK_REALTIME

typedef struct GPIO_FIFO_T {
    struct timespec *data;
    int head;
    int tail;
    int size;
} gpio_fifo_t;

gpio_fifo_t *gpio_fifo_create(int size);
void gpio_fifo_destroy(gpio_fifo_t *f);

int gpio_fifo_read(gpio_fifo_t *f, struct timespec *data, int ntimestamps);
int gpio_fifo_write(gpio_fifo_t *f, const struct timespec *data, int ntimestamps);
bool gpio_fifo_data_available(gpio_fifo_t *f);
void gpio_fifo_clear(gpio_fifo_t *f);

#endif //_GPIO_FIFO_H_