#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

int32_t utf8_encode(uint32_t val, uint8_t *buf, size_t buf_len) {
    uint8_t nbytes;
    if (val <= 0x7f) {
        nbytes = 1;
    } else if (val <= 0x7FF) {
        nbytes = 2;
    } else if (val <= 0xFFFF) {
        nbytes = 3;
    } else if (val <= 0x10FFFF) {
        nbytes = 4;
    } else {
        return -EINVAL;
    }

    if (buf == NULL || buf_len < nbytes) return -ENOBUFS;

    if (nbytes == 1) {
        buf[0] = (uint8_t) val;
    } else {
        uint8_t shift = (nbytes - 1) * 6;
        buf[0] = (uint8_t) (0xff << (8 - nbytes) | (val >> shift));

        for (uint8_t i = 1; i < nbytes; i++) {
            buf[i] = (uint8_t) (0x80 | ((val >> (shift - 6 * i)) & 0x3f));
        }
    }

    return nbytes;
}

int32_t utf8_decode(uint8_t *buf, size_t buf_len, uint32_t *result) {
    if (result == NULL || buf == NULL || buf_len < 1) {
        return -ENODATA;
    }

    uint32_t start;
    uint8_t nbytes;
    if (buf[0] <= 0x7f) {
        nbytes = 1;
        start = buf[0];
    } else if ((buf[0] & 0xe0) == 0xc0) {
        nbytes = 2;
        start = buf[0] & 0x1f;
    } else if ((buf[0] & 0xf0) == 0xe0) {
        nbytes = 3;
        start = buf[0] & 0x0f;
    } else if ((buf[0] & 0xf8) == 0xf0) {
        nbytes = 4;
        start = buf[0] & 0x07;
    } else {
        return -EINVAL;
    }

    if (buf_len < nbytes) return -ENODATA;

    *result = start;
    for (uint8_t i = 1; i < nbytes; i++) {
        if ((buf[i] & 0xc0) != 0x80) {
            /* invalid continuation byte */
            return -EINVAL;
        }

        *result = (*result << 6) | (buf[i] & 0x3f);
    }

    return nbytes;
}

int32_t utf8_rbuf_put(struct ring_buf *buf, uint32_t val) {
    uint8_t *put;
    uint32_t put_len = ring_buf_put_claim(buf, &put, 4);

    int32_t utf8_len = utf8_encode(val, put, put_len);
    if (utf8_len < 0) {
        ring_buf_put_finish(buf, 0);
    } else {
        ring_buf_put_finish(buf, utf8_len);
    }

    return utf8_len;
}

int32_t utf8_rbuf_get(struct ring_buf *buf, uint32_t *result) {
    uint8_t *get;
    uint32_t get_len = ring_buf_get_claim(buf, &get, 4);

    int32_t utf8_len = utf8_decode(get, get_len, result);
    if (utf8_len < 0) {
        ring_buf_get_finish(buf, 0);
    } else {
        ring_buf_get_finish(buf, utf8_len);
    }

    return utf8_len;
}
