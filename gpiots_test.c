/*
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
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "fifo.h"

#define LUSSEN 2
#define NGPIOS (LUSSEN * 2)

int main(int argc, char **argv) {

    struct timespec ts;
    fifo_payload_t lus[LUSSEN];
    for (int i = 0; i < LUSSEN; ++i) {
        lus[i].lusid = i;
        lus[i].ts_start = lus[i].ts_end = (struct timespec) { 0, 0 }; 
    }
    int files[NGPIOS];
    for (int i = 0; i < NGPIOS; ++i) {
        char gpio[64];
        snprintf(gpio, 64, "/dev/gpiots%d", i);
        int fd = open(gpio, O_RDONLY); 
        if (fd < 0) {
            printf("%s open error %d\n", gpio, fd);
            exit(-1);
        }
        files[i] = fd;
    }
    int n;
    struct pollfd fds[4];
    for (int i = 0; i < 4; ++i) {
        fds[i].fd = files[i];
        fds[i].events = POLLPRI | POLLERR;
    }
    while (true) {
        int rc = poll(fds, 4, 2000);
        if (rc < 0) { // error
            perror("poll failed");
            return -1;
        }
        if (rc == 0) { // timeout
            //printf("poll timeout\n");
            continue;
        }
        for (int i = 0; i < 4; ++i) {
            if (fds[i].revents != 0) {
                n = read(files[i], &ts, 1);
                if (n == 1) {
                    //printf("%d  %ld %ld\n", i, ts[i].tv_sec, ts[i].tv_nsec);
                } else {
                    printf("**************read failed for fd%d\n", i);
                }
                int tsi = i / 2;
                if ((i & 1) == 0) {
                    lus[tsi].ts_start = ts;
                } else {
                    lus[tsi].ts_end = ts;
                }
           } 
        }
        for (int i = 0 ; i < 2; ++i) {
            if (lus[i].ts_end.tv_sec > 0) {
                long usecs_start = lus[i].ts_start.tv_sec * 1000000 + (lus[i].ts_start.tv_nsec / 1000);
                long usecs_end = lus[i].ts_end.tv_sec * 1000000 + (lus[i].ts_end.tv_nsec / 1000);
                long micros = usecs_end - usecs_start;
                if (micros > 0) {
                    double kmph = (0.00025 * 3600 * 1000 * 1000) / (double)micros;
                    printf("lus: %d, diff: %ld, kmph: %1.0f\n", lus[i].lusid, micros, round(kmph));    
                } else {
                    printf("lus %d: ***interrupts arrived out of order\n", lus[i].lusid);               
                }
                lus[i].ts_start = lus[i].ts_end = (struct timespec){ 0, 0 };
            }
           
        }
    }
    for (int i = 0; i < 4; ++i) {
        close(files[i]);
    }
    exit(0);
}
