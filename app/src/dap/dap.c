#include <logging/log.h>
#include <sys/ring_buffer.h>
#include <sys/slist.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "dap/usb.h"

LOG_MODULE_REGISTER(dap);

bool dap_is_configured(const struct device *dev) {
    struct dap_data *data = dev->data;
    return data->configured;
}

int32_t dap_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    if (data->configured) {
        return 0;
    }

    ring_buf_reset(config->request_buf);
    ring_buf_reset(config->response_buf);

    data->configured = true;
    return 0;
}

int32_t dap_reset(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    data->configured = false;

    ring_buf_reset(config->request_buf);
    ring_buf_reset(config->response_buf);

    return 0;
}

int32_t dap_handle_request(const struct device *dev) {
    const struct dap_config *config = dev->config;

    // this request should not be handled before the previous response was transmitted, so
    // we can assume that the response buffer is safe to reset
    ring_buf_reset(config->response_buf);
    // data should be available at the front of the ring buffer before calling this handler
    uint8_t command = 0xff;
    ring_buf_get(config->request_buf, &command, 1);
    
    switch (command) {
    case DAP_COMMAND_INFO:
        return dap_handle_command_info(dev);
    default:
        LOG_ERR("unsupported command 0x%x", command);
        return -ENOTSUP;
    }
}

sys_slist_t dap_devlist;

static int32_t dap_init(const struct device *dev) {
    struct dap_data *data = dev->data;

    data->dev = dev;
    sys_slist_append(&dap_devlist, &data->devlist_node);

    return dap_reset(dev);
}

#define DT_DRV_COMPAT rice_dap

#define DAP_DT_DEVICE_DEFINE(idx)                                   \
                                                                    \
    RING_BUF_DECLARE(dap_request_buf_##idx, DAP_RING_BUF_SIZE);     \
    RING_BUF_DECLARE(dap_respones_buf_##idx, DAP_BULK_EP_MPS);      \
                                                                    \
    DAP_USB_CONFIG_DEFINE(dap_usb_config_##idx, idx);               \
                                                                    \
    struct dap_data dap_data_##idx;                                 \
    const struct dap_config dap_config_##idx = {                    \
        .request_buf = &dap_request_buf_##idx,                      \
        .response_buf = &dap_respones_buf_##idx,                    \
        .usb_config = &dap_usb_config_##idx,                        \
    };                                                              \
                                                                    \
    DEVICE_DT_INST_DEFINE(                                          \
        idx,                                                        \
        dap_init,                                                   \
        NULL,                                                       \
        &dap_data_##idx,                                            \
        &dap_config_##idx,                                          \
        APPLICATION,                                                \
        40,                                                         \
        NULL,                                                       \
    );

DT_INST_FOREACH_STATUS_OKAY(DAP_DT_DEVICE_DEFINE);
