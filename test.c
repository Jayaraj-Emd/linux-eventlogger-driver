// test_read.c - simple userspace test client for /dev/eventlogger
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include<linux/ioctl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#define MAGIC_NUM    'k'
#define GET_TOTAL_EVENT_LOGGED  _IOR(MAGIC_NUM,1,uint64_t)
#define GET_RING_BUF_COUNT  _IOR(MAGIC_NUM,2,uint64_t)
#define DEVICE_PATH "/dev/eventlogger"
#define READ_BUF_SIZE 128

int main(int argc, char *argv[]) {
    int fd;
    char buf[READ_BUF_SIZE];
    ssize_t bytes_read;
    int iterations = 20;     
    int delay_sec = 1;        
    if (argc > 1) iterations = atoi(argv[1]);
    if (argc > 2) delay_sec = atoi(argv[2]);
    uint64_t total_event;
    uint64_t total_count;
    int ret1;
    int ret2;
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return 1;
    }

    printf("Opened %s successfully. Reading %d times, %ds apart...\n",
           DEVICE_PATH, iterations, delay_sec);

    for (int i = 0; i < iterations; i++) {
        memset(buf, 0, sizeof(buf));
        bytes_read = read(fd, buf, sizeof(buf) - 1);
        ret1 = ioctl(fd,GET_TOTAL_EVENT_LOGGED,&total_event);
        ret2 = ioctl(fd,GET_RING_BUF_COUNT,&total_count);
        if (bytes_read < 0 || ret1 < 0 ||ret2 < 0) {
            printf("[%d] read() error: %s\n", i, strerror(errno));
        } else if (bytes_read == 0) {
            printf("[%d] read() returned 0 (buffer empty)\n", i);
        } else {
            buf[bytes_read] = '\0';
            printf("[%d] read %zd bytes: %s", i, bytes_read, buf);
            printf("[%d] total_Event %ld\n",i,total_event);
            printf("[%d] total_count in ring buf = %ld\n",i,total_count);

        }

        sleep(delay_sec);
    }

    close(fd);
    printf("Closed device. Test complete.\n");
    return 0;
}