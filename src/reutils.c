#include <string.h>
#include "reutils.h"

uint32_t to_uint32_t(char const * str, unsigned char str_len) {
    uint32_t out = 0;
    unsigned char buf[sizeof(uint32_t)] = {'\0'};
    memcpy(buf + (sizeof(uint32_t) - str_len), str, str_len);
    memcpy(&out, buf, sizeof(uint32_t));
    return out;
}