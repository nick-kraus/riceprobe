#include <zephyr/sys/ring_buffer.h>
#include <zephyr/ztest.h>

#include "utf8.h"

ZTEST(utf8, test_encode) {
    uint8_t buf[4];

    /* null buffer */
    zassert_equal(utf8_encode(0x10FFFF, NULL, 4), -ENOBUFS);
    /* undersized buffer */
    zassert_equal(utf8_encode(0x10FFFF, buf, 3), -ENOBUFS);

    /* value larger than utf8 max codepoint */
    zassert_equal(utf8_encode(0x110000, buf, 4), -EINVAL);

    /* 1 byte codepoints */
    zassert_equal(utf8_encode(0x0, buf, 4), 1);
    zassert_mem_equal(buf, "\x00", 1);
    zassert_equal(utf8_encode(0x7f, buf, 4), 1);
    zassert_mem_equal(buf, "\x7f", 1);
    /* 2 byte codepoints */
    zassert_equal(utf8_encode(0x80, buf, 4), 2);
    zassert_mem_equal(buf, "\xc2\x80", 2);
    zassert_equal(utf8_encode(0x7ff, buf, 4), 2);
    zassert_mem_equal(buf, "\xdf\xbf", 2);
    /* 3 byte codepoints */
    zassert_equal(utf8_encode(0x800, buf, 4), 3);
    zassert_mem_equal(buf, "\xe0\xa0\x80", 3);
    zassert_equal(utf8_encode(0xffff, buf, 4), 3);
    zassert_mem_equal(buf, "\xef\xbf\xbf", 3);
    /* 4 byte codepoints */
    zassert_equal(utf8_encode(0x10000, buf, 4), 4);
    zassert_mem_equal(buf, "\xf0\x90\x80\x80", 4);
    zassert_equal(utf8_encode(0x10ffff, buf, 4), 4);
    zassert_mem_equal(buf, "\xf4\x8f\xbf\xbf", 4);
}

ZTEST(utf8, test_decode) {
    uint32_t result;

    /* null and zero sized buffer / result */
    zassert_equal(utf8_decode(NULL, 1, &result), -ENODATA);
    zassert_equal(utf8_decode(&(uint8_t) { 0 }, 0, &result), -ENODATA);
    zassert_equal(utf8_decode(&(uint8_t) { 0 }, 1, NULL), -ENODATA);
    /* not enough bytes */
    zassert_equal(utf8_decode("\xf4\x8f\xbf", 3, &result), -ENODATA);

    /* invalid first byte */
    zassert_equal(utf8_decode("\xf8", 1, &result), -EINVAL);
    /* invalid continuation byte */
    zassert_equal(utf8_decode("\xef\xbf\xc0", 3, &result), -EINVAL);

    /* 1 byte codepoints */
    zassert_equal(utf8_decode("\x00", 1, &result), 1);
    zassert_equal(result, 0x00);
    zassert_equal(utf8_decode("\x7f", 1, &result), 1);
    zassert_equal(result, 0x7f);
    /* 2 byte codepoints */
    zassert_equal(utf8_decode("\xc2\x80", 2, &result), 2);
    zassert_equal(result, 0x80);
    zassert_equal(utf8_decode("\xdf\xbf", 2, &result), 2);
    zassert_equal(result, 0x7ff);
    /* 3 byte codepoints */
    zassert_equal(utf8_decode("\xe0\xa0\x80", 3, &result), 3);
    zassert_equal(result, 0x800);
    zassert_equal(utf8_decode("\xef\xbf\xbf", 3, &result), 3);
    zassert_equal(result, 0xffff);
    /* 4 byte codepoints */
    zassert_equal(utf8_decode("\xf0\x90\x80\x80", 4, &result), 4);
    zassert_equal(result, 0x10000);
    zassert_equal(utf8_decode("\xf4\x8f\xbf\xbf", 4, &result), 4);
    zassert_equal(result, 0x10ffff);
}

ZTEST(utf8, test_rbuf_put) {
    RING_BUF_DECLARE(rbuf, 4);

    /* only put the required length of bytes for a give utf8 value */
    zassert_equal(utf8_rbuf_put(&rbuf, 0xffff), 3);
    zassert_equal(ring_buf_size_get(&rbuf), 3);
    uint8_t *rbuf_get;
    ring_buf_get_claim(&rbuf, &rbuf_get, 3);
    zassert_mem_equal(rbuf_get, "\xef\xbf\xbf", 3);
    ring_buf_get_finish(&rbuf, 0);
    
    /* produce an error if the utf8 value cannot fit, ring buf size shouldn't change */
    zassert_equal(utf8_rbuf_put(&rbuf, 0xffff), -ENOBUFS);
    zassert_equal(ring_buf_size_get(&rbuf), 3);
    /* size also shouldn't change on invalid utf8 codepoint */
    ring_buf_reset(&rbuf);
    zassert_equal(utf8_rbuf_put(&rbuf, 0xffffffff), -EINVAL);
    zassert_equal(ring_buf_size_get(&rbuf), 0);
}

ZTEST(utf8, test_rbuf_get) {
    RING_BUF_DECLARE(rbuf, 4);
    uint32_t result;

    /* only get the required length of bytes for a give utf8 value */
    ring_buf_put(&rbuf, "\xc2\x80\x7f\x00", 4);
    zassert_equal(ring_buf_size_get(&rbuf), 4);
    zassert_equal(utf8_rbuf_get(&rbuf, &result), 2);
    zassert_equal(result, 0x80);
    zassert_equal(ring_buf_size_get(&rbuf), 2);
    
    /* don't remove bytes on invalid utf8 */
    ring_buf_reset(&rbuf);
    ring_buf_put(&rbuf, "\xef\xbf\xc0\x08", 4);
    zassert_equal(ring_buf_size_get(&rbuf), 4);
    zassert_equal(utf8_rbuf_get(&rbuf, &result), -EINVAL);
    zassert_equal(ring_buf_size_get(&rbuf), 4);
    /* also don't remove on lack of buffer space (incomplete utf8) */
    ring_buf_reset(&rbuf);
    ring_buf_put(&rbuf, "\xf4\x8f\xbf", 3);
    zassert_equal(ring_buf_size_get(&rbuf), 3);
    zassert_equal(utf8_rbuf_get(&rbuf, &result), -ENODATA);
    zassert_equal(ring_buf_size_get(&rbuf), 3);
}

ZTEST_SUITE(utf8, NULL, NULL, NULL, NULL, NULL);
