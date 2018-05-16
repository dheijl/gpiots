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

int main(int argc, char **argv) {

    struct timespec ts[4];
    int fd1, fd2, fd3, fd4;
    fd1 = open("/dev/gpiots0", O_RDONLY);
    fd2 = open("/dev/gpiots1", O_RDONLY);
    fd3 = open("/dev/gpiots2", O_RDONLY);
    fd4 = open("/dev/gpiots3", O_RDONLY);
    if (fd1 < 0) {
        printf("open error\n");
        exit(-1);
    }
    int files[] = { fd1, fd2, fd3, fd4 };
    int n;
    struct pollfd fds[4];
    for (int i = 0; i < 4; ++i) {
        fds[i].fd = files[i];
        fds[i].events = POLLPRI | POLLERR;
    }
    while (true) {
        int rc = poll(fds, 4, 100);
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
                ts[i].tv_sec = ts[i].tv_nsec = 0;
                n = read(files[i], &ts[i], 1);
                if (n == 1) {
                    //printf("%d  %ld %ld\n", i, ts[i].tv_sec, ts[i].tv_nsec);
                } else {
                    printf("**************read failed for fd%d\n", i);
                }
            } 
        }
        if (fds[3].revents != 0) {
            for (int j = 0; j < 4; j += 2) {
                long usecs1 = ts[j].tv_sec * 1000000 + (ts[j].tv_nsec / 1000);
                long usecs2 = ts[j + 1].tv_sec * 1000000 + (ts[j + 1].tv_nsec / 1000);
                long micros = usecs2 - usecs1;
                if (micros < 0) {
                    printf("************interrupts arrived out of order\n");
                    continue;
                }
                double kmph = (0.00025 * 3600 * 1000 * 1000) / (double)micros;
                printf("Channel: %d, diff: %ld, kmph: %1.0f\n", j, micros, round(kmph));
            }
        }

    }
    for (int i = 0; i < 4; ++i) {
        close(files[i]);
    }
    exit(0);
}
