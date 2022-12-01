#ifndef __IO_PRIV_H__
#define __IO_PRIV_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>

/* supported version of the IO protocol */
#define IO_PROTOCOL_VERSION     "0.0.1"

/* size of the internal buffers in bytes */
#define IO_RING_BUF_SIZE        (2048)

struct io_data {
    const struct device *dev;
    sys_snode_t devlist_node;
};

struct io_config {
    uint8_t *ep_buf;
    struct ring_buf *request_buf;
    struct ring_buf *response_buf;

    struct usb_cfg_data *usb_config;
};

extern sys_slist_t io_devlist;

int32_t io_reset(const struct device *dev);
int32_t io_handle_request(const struct device *dev);

#endif /* __IO_PRIV_H__ */
