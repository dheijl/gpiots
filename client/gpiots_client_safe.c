/*
Licensed under The MIT License (MIT)

Copyright (c) 2021 Diamino

This client is an adaptation of the gpiots example of Danny Heijl

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

#define NGPIOS 3
//int gpios[] = {9, 10, 11};

typedef int64_t time64_t;
struct timespec64 {
	time64_t	tv_sec;			/* seconds */
	long		tv_nsec;		/* nanoseconds */
};

// void timespecsub(struct timespec64 *a, struct timespec64 *b, struct timespec64 *result) {
//     result->tv_sec  = a->tv_sec  - b->tv_sec;
//     result->tv_nsec = a->tv_nsec - b->tv_nsec;
//     if (result->tv_nsec < 0) {
//         --result->tv_sec;
//         result->tv_nsec += 1000000000L;
//     }
// }

int main(int argc, char **argv) {
    setbuf(stdout, NULL); // Disable output buffering

    struct timespec64 ts[NGPIOS];
    //struct timespec64 prevts[NGPIOS], deltats[NGPIOS];
    int fd[NGPIOS];
    struct pollfd pfds[NGPIOS];
    
    for (int i = 0; i < NGPIOS; i++) {
        char gpiofile[64];
        snprintf(gpiofile, 64, "/dev/gpiots%d", i);
        fd[i] = open(gpiofile, O_RDONLY);
        if (fd[i] < 0) {
            fprintf(stderr, "%s open error %d\n", gpiofile, fd[i]);
            exit(-1);
        }
        pfds[i].fd = fd[i];
        pfds[i].events = POLLPRI | POLLERR;
    }

    int n;
    while (true) {
        int rc = poll(pfds, NGPIOS, 2000);
        if (rc < 0) { // error
            perror("poll failed");
            return -1;
        }
        if (rc == 0) { // timeout
            fprintf(stderr, "poll timeout\n");
            continue;
        }
        for (int i = 0; i < NGPIOS; i++) {
            if (pfds[i].revents != 0) {
                n = read(fd[i], &ts[i], sizeof(struct timespec64));
                if (n==sizeof(struct timespec64)) {
                    printf("%d,%lld,%ld\n", i, ts[i].tv_sec, ts[i].tv_nsec);
                    fprintf(stderr, " [%d] %lld secs %ld nsecs\n", i, ts[i].tv_sec, ts[i].tv_nsec);
                    //timespecsub(&ts[i], &prevts[i], &deltats[i]);
                    //fprintf(stderr," [%d]   delta: %lld secs %ld nsecs\n", i, deltats[i].tv_sec, deltats[i].tv_nsec);
                    //prevts[i] = ts[i];   
                } else {
                    fprintf(stderr, "************** read failed for fd%d\n", i);
                    continue;
                }
            }
        }
    }
    for (int i = 0; i < NGPIOS; i++) {
        close(fd[i]);
    }
    exit(0);
}
