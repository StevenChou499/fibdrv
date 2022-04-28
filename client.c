#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    long long sz;

    char buf[120];
    // char write_buf[] = "testing writing";
    int offset = 500; /* TODO: try test something bigger than the limit */
    struct timespec start, end;

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        for (int type = 3; type < 9; type++) {
            clock_gettime(CLOCK_MONOTONIC, &start);
            sz = write(fd, buf, type);
            clock_gettime(CLOCK_MONOTONIC, &end);
            printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                         (start.tv_sec * 1e9 + start.tv_nsec)));
            printf("%lld ", sz);
            printf("%lld ", (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                         (start.tv_sec * 1e9 + start.tv_nsec)) -
                                sz);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}