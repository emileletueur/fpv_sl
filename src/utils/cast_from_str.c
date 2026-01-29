#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

bool parse_bool(const char *str) {
    return (strcasecmp(str, "true") == 0 || strcasecmp(str, "1") == 0 || strcasecmp(str, "yes") == 0);
}

uint8_t parse_uint8(const char *str) {
    unsigned long val = strtoul(str, NULL, 0);
    return (val > UINT8_MAX) ? 0 : (uint8_t) val;
}

uint16_t parse_uint16(const char *str) {
    unsigned long val = strtoul(str, NULL, 0);
    return (val > UINT16_MAX) ? 0 : (uint16_t) val;
}

uint32_t parse_uint32(const char *str) {
    return (uint32_t) strtoul(str, NULL, 0);
}

uint64_t parse_uint64(const char *str) {
    return (uint64_t) strtoull(str, NULL, 0);
}
