#include "mph_time.h"

#include <sys/time.h>

uint64_t mph_time_now_unix_ms(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((uint64_t)now.tv_sec * 1000ULL) + ((uint64_t)now.tv_usec / 1000ULL);
}
