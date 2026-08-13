#include "common.h"

ssize_t read_all(int fd, void *buf, size_t size) {
    size_t total = 0;
    char *ptr = (char *)buf;
    while (total < size) {
        ssize_t n = read(fd, ptr + total, size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (n == 0) {
            return total; // EOF
        }
        total += n;
    }
    return total;
}

ssize_t write_all(int fd, const void *buf, size_t size) {
    size_t total = 0;
    const char *ptr = (const char *)buf;
    while (total < size) {
        ssize_t n = write(fd, ptr + total, size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += n;
    }
    return total;
}