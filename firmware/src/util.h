#ifndef __UTIL_H__
#define __UTIL_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>

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
        /* native posix platforms don't progress time unless sleep functions are called */
        IF_ENABLED(CONFIG_ARCH_POSIX, (k_busy_wait(1);));
        uint32_t current = k_cycle_get_32();
        /* the subtraction handles uint32 overflow */ 
        if ((current - start) >= wait) {
            break;
        }
    }
}

/*
 * convenience functions for ring buffers
 */

static inline int32_t ring_buf_get_skip(struct ring_buf *buf, size_t len) {
    uint8_t *ptr = NULL;
    if (ring_buf_get_claim(buf, &ptr, len) != len) return -EMSGSIZE;
    return ring_buf_get_finish(buf, len);
}

static inline int32_t ring_buf_get_le16(struct ring_buf *buf, uint16_t *value) {
    uint8_t *ptr = NULL;
    if (ring_buf_get_claim(buf, &ptr, 2) != 2) return -EMSGSIZE;
    *value = sys_get_le16(ptr);
    return ring_buf_get_finish(buf, 2);
}

static inline int32_t ring_buf_get_le32(struct ring_buf *buf, uint32_t *value) {
    uint8_t *ptr = NULL;
    if (ring_buf_get_claim(buf, &ptr, 4) != 4) return -EMSGSIZE;
    *value = sys_get_le32(ptr);
    return ring_buf_get_finish(buf, 4);
}

static inline int32_t ring_buf_put_le32(struct ring_buf *buf, uint32_t value) {
    uint8_t *ptr = NULL;
    if (ring_buf_put_claim(buf, &ptr, 4) != 4) return -ENOBUFS;
    sys_put_le32(value, ptr);
    return ring_buf_put_finish(buf, 4);
}

#endif /* __UTIL_H__ */
