#ifndef __DAP_PRIV_H__
#define __DAP_PRIV_H__

#include <sys/ring_buffer.h>
#include <usb/usb_device.h>
#include <zephyr.h>

/* size of the internal buffers in bytes */
#define DAP_RING_BUF_SIZE (1024)

struct dap_data {
    bool configured;

    const struct device *dev;
    sys_snode_t devlist_node;
};

struct dap_config {
    struct ring_buf *request_buf;
    struct ring_buf *response_buf;

    struct usb_cfg_data *usb_config;
};

extern sys_slist_t dap_devlist;

bool dap_is_configured(const struct device *dev);
int32_t dap_configure(const struct device *dev);
int32_t dap_reset(const struct device *dev);
int32_t dap_handle_request(const struct device *dev);

#endif /* __DAP_PRIV_H__ */
