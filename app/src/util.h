#ifndef __UTIL_H__
#define __UTIL_H__

#include <zephyr/kernel.h>

/* tests a condition, and triggers a kernel oops with a log message on failure */
#define FATAL_CHECK(_cond, _msg)    \
    do {                            \
        if ((_cond) == false) {     \
            LOG_ERR(_msg);          \
            k_oops();               \
        }                           \
    } while (0)

/* busy waits for a set amount of nanoseconds */
static inline void busy_wait_nanos(uint32_t nanos) {
    uint32_t start = k_cycle_get_32();
    uint32_t wait = (uint32_t) (
        (uint64_t) nanos *
        (uint64_t) sys_clock_hw_cycles_per_sec() /
        (uint64_t) NSEC_PER_SEC
    );

    while (true) {
        uint32_t current = k_cycle_get_32();
        /* the subtraction handles uint32 overflow */ 
        if ((current - start) >= wait) {
            break;
        }
    }
}

#endif /* __UTIL_H__ */
