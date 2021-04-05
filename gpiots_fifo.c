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

#include <linux/slab.h>
#include <linux/time.h>
#include "gpiots_fifo.h"

// This initializes the FIFO structure with the given buffer and size
gpio_fifo_t *gpio_fifo_create(int size) {
    gpio_fifo_t *f = (gpio_fifo_t *)kmalloc(sizeof(gpio_fifo_t), GFP_KERNEL);
    if (f == NULL) {
        printk(KERN_ERR "fifo_create: out of memory\n");
        return NULL;
    }
    f->head = 0;
    f->tail = 0;
    f->size = size + 1;
    f->data = (struct timespec64 *)kmalloc((size + 1) * sizeof(struct timespec64), GFP_KERNEL);
    if (f->data == NULL) {
        printk(KERN_ERR "fifo_create: out of memory\n");
        return NULL;
    }
    return f;
}

// release the allocated memory for the FIFO.
void gpio_fifo_destroy(gpio_fifo_t *f) {
    if (f->data != NULL) {
        kfree(f->data);
    }
    if (f != NULL) {
        kfree(f);
    }
}
// This reads up to n timestamps from the FIFO
// The number of timestamps actually read is returned
int gpio_fifo_read(gpio_fifo_t *f, struct timespec64 *data, int ntimestamps) {
    int i;
    struct timespec64 *p = data;
    for (i = 0; i < ntimestamps; i++) {
        if (f->tail != f->head) {     // see if any data is available
            *p++ = f->data[f->tail];  // grab a timestamp from the buffer
            f->tail++;                // increment the tail
            if (f->tail == f->size) { // check for wrap-around
                f->tail = 0;
            }
        } else {
            return i; // number of timestamps read
        }
    }
    return ntimestamps;
}
// This writes up to n timestamps to the FIFO
// If the head runs in to the tail, not all timestamps are written
// The number of timestamps actually written is returned
int gpio_fifo_write(gpio_fifo_t *f, const struct timespec64 *data, int ntimestamps) {
    int i;
    const struct timespec64 *p;
    p = data;
    for (i = 0; i < ntimestamps; i++) {
        // first check to see if there is space in the buffer
        if ((f->head + 1 == f->tail) || ((f->head + 1 == f->size) && (f->tail == 0))) {
            return i; // no more room
        } else {
            f->data[f->head] = *p++;
            f->head++;                // increment the head
            if (f->head == f->size) { // check for wrap-around
                f->head = 0;
            }
        }
    }
    return ntimestamps;
}

// returns true if the FIFO has data available
bool gpio_fifo_data_available(gpio_fifo_t *f) {
    return(f->tail != f->head);
}

// clears all entries in the FIFO
void gpio_fifo_clear(gpio_fifo_t *f) {
    f->tail = f->head = 0;
}