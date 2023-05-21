#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

int parse_u32(const char *str, uint32_t *res);

int parse_u64(const char *str, uint64_t *res);

int uuid_init_random(uint8_t uuid[16]);

void uuid_print(uint8_t uuid[16]);

#endif
