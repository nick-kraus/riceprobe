#ifndef __UTF8_H__
#define __UTF8_H__

/* encodes 'val' as a utf8 codepoint, into the byte array 'buf' */
int32_t utf8_encode(uint32_t val, uint8_t *buf, size_t buf_len);

/* decodes a utf8 codepoint from byte array 'buf' into 'result' */
int32_t utf8_decode(uint8_t *buf, size_t buf_len, uint32_t *result);

/* encodes 'val' as a utf8 codepoint onto ring buffer 'buf' */
int32_t utf8_rbuf_put(struct ring_buf *buf, uint32_t val);

/* decodes a utf8 codepoint at the start of 'buf' and places it in the result */
int32_t utf8_rbuf_get(struct ring_buf *buf, uint32_t *result);

#endif /* __UTF8_H__ */
