#ifndef __UTIL_H__
#define __UTIL_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

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
        uint32_t current = k_cycle_get_32();
        /* the subtraction handles uint32 overflow */ 
        if ((current - start) >= wait) {
            break;
        }
    }
}

/* gets a ULEB128 encoded varint (up to 4-bytes long) from a ring buffer, negative value returned on error */
static inline int32_t ring_buf_uleb128_get(struct ring_buf *buf) {
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t byte;
        if (ring_buf_get(buf, &byte, 1) != 1) {
            return -EMSGSIZE;
        } else if (i == 3 && (byte & 0x80) != 0) {
            /* only support up to 4-byte varints, set top bit not allowed on last byte */
            return -EINVAL;
        }
        result |= (byte & 0x7f) << 7 * i;
        if ((byte & 0x80) == 0) { break; }
    }

    return result;
}

/* gets a ULEB128 encoded varint (up to 4-bytes long) onto a ring buffer, negative value returned on error */
static inline int32_t ring_buf_uleb128_put(struct ring_buf *buf, uint32_t val) {
    if (val > (1 << 28) - 1) {
        /* only support up to 4-byte varints, anything bigger than this can't be encoded */
        return -EINVAL;
    }
    for (int i = 0; i < 4; i++) {
        uint8_t byte = (uint8_t) (val & 0x7f);
        val >>= 7;
        if (val != 0) { byte |= 0x80; }
        if (ring_buf_put(buf, &byte, 1) != 1) { return -ENOBUFS; }
    }

    return 0;
}

#endif /* __UTIL_H__ */
