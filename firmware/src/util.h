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

/* tests if two expressions are equal, and return an error code if not */
#define CHECK_EQ(_expr_a, _expr_b, _code)   \
    do {                                    \
        if ((_expr_a) != (_expr_b)) {       \
            return _code;                   \
        }                                   \
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
        /* native posix platforms don't progress time unless sleep functions are called */
        IF_ENABLED(CONFIG_ARCH_POSIX, (k_busy_wait(1);));
        uint32_t current = k_cycle_get_32();
        /* the subtraction handles uint32 overflow */ 
        if ((current - start) >= wait) {
            break;
        }
    }
}

#endif /* __UTIL_H__ */
