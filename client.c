#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int comp(const void *elem1, const void *elem2)
{
    int f = *((int *) elem1);
    int s = *((int *) elem2);
    if (f > s)
        return 1;
    if (f < s)
        return -1;
    return 0;
}

int main()
{
    long long sz;

    char buf[300];
    int offset = 1000; /* TODO: try test something bigger than the limit */
    struct timespec start, end;
    long long total[1000];

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        for (int type = 0; type < 2; type++) {
            for (int j = 0; j < 1000; j++) {
                clock_gettime(CLOCK_MONOTONIC, &start);
                sz = write(fd, buf, type);
                clock_gettime(CLOCK_MONOTONIC, &end);
                total[j] = sz;
            }
            qsort(total, 1000, sizeof(long long), comp);
            long long time = 0LL;
            for (int num = 30; num < 980; num++) {
                time += total[num];
            }
            time /= 950;
            printf("%lld ", time);
        }
        printf("\n");
    }

    close(fd);
    return 0;
}