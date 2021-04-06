/*

A FIFO buffer for timespec structures

Licensed under The MIT License (MIT)

Copyright (c) 2018 Danny Heijl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef _GPIOTS_FIFO_H_
#define _GPIOTS_FIFO_H_

#include <linux/time.h>

#define RT_CLOCK CLOCK_REALTIME

typedef struct GPIO_FIFO_T {
    struct timespec64 *data;
    int head;
    int tail;
    int size;
} gpio_fifo_t;

gpio_fifo_t *gpio_fifo_create(int size);
void gpio_fifo_destroy(gpio_fifo_t *f);

int gpio_fifo_read(gpio_fifo_t *f, struct timespec64 *data, int ntimestamps);
int gpio_fifo_write(gpio_fifo_t *f, const struct timespec64 *data, int ntimestamps);
bool gpio_fifo_data_available(gpio_fifo_t *f);
void gpio_fifo_clear(gpio_fifo_t *f);

#endif //_GPIOTS_FIFO_H_