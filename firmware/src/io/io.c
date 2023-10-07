#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/slist.h>

#include "io/io.h"
#include "io/usb.h"

LOG_MODULE_REGISTER(io, CONFIG_IO_LOG_LEVEL);

bool io_is_configured(const struct device *dev) {
    struct io_data *data = dev->data;
    return data->configured;
}

int32_t io_configure(const struct device *dev) {
    struct io_data *data = dev->data;
    const struct io_config *config = dev->config;

    if (data->configured) {
        return 0;
    }
    LOG_INF("configuring driver");

    ring_buf_reset(config->rbuf);
    data->configured = true;
    return 0;
}

int32_t io_reset(const struct device *dev) {
    struct io_data *data = dev->data;
    const struct io_config *config = dev->config;

    LOG_INF("resetting driver state");
    data->configured = false;

    ring_buf_reset(config->rbuf);
    return 0;
}

sys_slist_t io_devlist;

static int32_t io_init(const struct device *dev) {
    struct io_data *data = dev->data;

    data->dev = dev;
    sys_slist_append(&io_devlist, &data->devlist_node);

    return io_reset(dev);
}

#define DT_DRV_COMPAT rice_io

#define IO_DT_DEVICE_DEFINE(idx)                        \
                                                        \
    RING_BUF_DECLARE(io_rb_##idx, IO_RING_BUF_SIZE);    \
                                                        \
    IO_USB_CONFIG_DEFINE(io_usb_config_##idx, idx);     \
                                                        \
    struct io_data io_data_##idx;                       \
    const struct io_config io_config_##idx = {          \
        .rbuf = &io_rb_##idx,                           \
        .usb_config = &io_usb_config_##idx,             \
    };                                                  \
                                                        \
    DEVICE_DT_INST_DEFINE(                              \
        idx,                                            \
        io_init,                                        \
        NULL,                                           \
        &io_data_##idx,                                 \
        &io_config_##idx,                               \
        POST_KERNEL,                                    \
        99,                                             \
        NULL,                                           \
    );

DT_INST_FOREACH_STATUS_OKAY(IO_DT_DEVICE_DEFINE);
