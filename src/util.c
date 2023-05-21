#include "util.h"


#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int parse_u32(const char *str, uint32_t *res) {
    errno = 0;
    char* end = (char *) str;
    *res = strtoul(str, &end, 10);
    if (errno == ERANGE || *end) return -1;
   
    return 0;
}

int parse_u64(const char *str, uint64_t *res) {
    errno = 0;
    char* end = (char *) str;
    *res = strtoull(str, &end, 10);
    if (errno == ERANGE || *end) return -1;
   
    return 0;
}

int uuid_init_random(uint8_t uuid[16]) {
    int fd = open("/dev/random", O_RDONLY);
    int n_read = read(fd, uuid, 16);
    close(fd);
    return n_read - 16;
}

void uuid_print(uint8_t uuid[16]) {
    printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
        uuid[0], uuid[1], uuid[2], uuid[3], 
        uuid[4], uuid[5], uuid[6], uuid[7], 
        uuid[8], uuid[9], uuid[10], uuid[11], 
        uuid[12], uuid[13], uuid[14], uuid[15]
    );
}
