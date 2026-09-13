// test_read.c - simple userspace test client for /dev/eventlogger
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

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

        if (bytes_read < 0) {
            printf("[%d] read() error: %s\n", i, strerror(errno));
        } else if (bytes_read == 0) {
            printf("[%d] read() returned 0 (buffer empty)\n", i);
        } else {
            buf[bytes_read] = '\0';
            printf("[%d] read %zd bytes: %s", i, bytes_read, buf);
        }

        sleep(delay_sec);
    }

    close(fd);
    printf("Closed device. Test complete.\n");
    return 0;
}