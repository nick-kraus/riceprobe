#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/slist.h>

#include "io/commands.h"
#include "io/io.h"
#include "io/usb.h"
#include "util.h"

LOG_MODULE_REGISTER(io, CONFIG_IO_LOG_LEVEL);

int32_t io_reset(const struct device *dev) {
    const struct io_config *config = dev->config;

    LOG_INF("resetting driver state");

    ring_buf_reset(config->request_buf);
    ring_buf_reset(config->response_buf);

    return 0;
}

int32_t io_handle_request(const struct device *dev) {
    const struct io_config *config = dev->config;

    int32_t command = ring_buf_uleb128_get(config->request_buf);
    if (command < 0) {
        LOG_ERR("uleb128 get failed with error %d", command);
        return -IO_ERROR_INVALID;
    }

    switch (command) {
    default:
        LOG_ERR("unsupported command 0x%x", command);
        return -IO_ERROR_UNSUPPORTED;
    }
}

sys_slist_t io_devlist;

static int32_t io_init(const struct device *dev) {
    struct io_data *data = dev->data;

    data->dev = dev;
    sys_slist_append(&io_devlist, &data->devlist_node);

    return io_reset(dev);
}

#define DT_DRV_COMPAT rice_io

#define IO_DT_DEVICE_DEFINE(idx)                                \
                                                                \
    static uint8_t io_ep_buffer_##idx[IO_BULK_EP_MPS];          \
    RING_BUF_DECLARE(io_request_buf_##idx, IO_RING_BUF_SIZE);   \
    RING_BUF_DECLARE(io_response_buf_##idx, IO_RING_BUF_SIZE);  \
                                                                \
    IO_USB_CONFIG_DEFINE(io_usb_config_##idx, idx);             \
                                                                \
    struct io_data io_data_##idx;                               \
    const struct io_config io_config_##idx = {                  \
        .ep_buf = io_ep_buffer_##idx,                           \
        .request_buf = &io_request_buf_##idx,                   \
        .response_buf = &io_response_buf_##idx,                 \
        .usb_config = &io_usb_config_##idx,                     \
    };                                                          \
                                                                \
    DEVICE_DT_INST_DEFINE(                                      \
        idx,                                                    \
        io_init,                                                \
        NULL,                                                   \
        &io_data_##idx,                                         \
        &io_config_##idx,                                       \
        APPLICATION,                                            \
        40,                                                     \
        NULL,                                                   \
    );

DT_INST_FOREACH_STATUS_OKAY(IO_DT_DEVICE_DEFINE);
