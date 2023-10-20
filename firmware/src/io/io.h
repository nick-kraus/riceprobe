#ifndef __IO_PRIV_H__
#define __IO_PRIV_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "io/transport.h"

/* size of the internal buffers in bytes */
#define IO_RING_BUF_SIZE        (2048)
/* maximum size for any single transport transfer */
#define IO_MAX_PACKET_SIZE      (512)

/* possible status responses to commands */
static const uint8_t io_cmd_response_ok = 0x00;
static const uint8_t io_cmd_response_enotsup = 0xff;

struct io_driver {
    struct {
        uint8_t request_bytes[IO_RING_BUF_SIZE];
        struct ring_buf request;
        uint8_t response_bytes[IO_RING_BUF_SIZE];
        struct ring_buf response;
    } buf;

    struct io_transport *transport;
};

/* command ids */
static const uint32_t io_cmd_info = 0x01;
static const uint32_t io_cmd_multi = 0x02;
static const uint32_t io_cmd_queue = 0x03;
static const uint32_t io_cmd_delay = 0x04;

/* command handlers */
int32_t io_handle_cmd_info(struct io_driver *io);
int32_t io_handle_cmd_delay(struct io_driver *io);

#endif /* __IO_PRIV_H__ */
