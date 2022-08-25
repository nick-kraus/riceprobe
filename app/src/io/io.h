#ifndef __IO_PRIV_H__
#define __IO_PRIV_H__

#include <sys/ring_buffer.h>
#include <usb/usb_device.h>
#include <zephyr.h>

/* size of the internal buffers in bytes */
#define IO_RING_BUF_SIZE (1024)

struct io_data {
    bool configured;

    const struct device *dev;
    sys_snode_t devlist_node;
};

struct io_config {
    struct ring_buf *rbuf;
    struct usb_cfg_data *usb_config;
};

extern sys_slist_t io_devlist;

bool io_is_configured(const struct device *dev);
int io_configure(const struct device *dev);
int io_reset(const struct device *dev);

#endif /* __IO_PRIV_H__ */
