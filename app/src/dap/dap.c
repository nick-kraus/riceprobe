#include <logging/log.h>
#include <sys/ring_buffer.h>
#include <sys/slist.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/usb.h"

LOG_MODULE_REGISTER(dap);

bool dap_is_configured(const struct device *dev) {
    struct dap_data *data = dev->data;
    return data->configured;
}

int dap_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    if (data->configured) {
        return 0;
    }

    ring_buf_reset(config->rbuf);
    data->configured = true;
    return 0;
}

int dap_reset(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    data->configured = false;

    ring_buf_reset(config->rbuf);
    return 0;
}

sys_slist_t dap_devlist;

static int dap_init(const struct device *dev) {
    struct dap_data *data = dev->data;

    data->dev = dev;
    sys_slist_append(&dap_devlist, &data->devlist_node);

    return dap_reset(dev);
}

#define DT_DRV_COMPAT rice_dap

#define DAP_DT_DEVICE_DEFINE(idx)                       \
                                                        \
    RING_BUF_DECLARE(dap_rb_##idx, DAP_RING_BUF_SIZE);  \
                                                        \
    DAP_USB_CONFIG_DEFINE(dap_usb_config_##idx, idx);   \
                                                        \
    struct dap_data dap_data_##idx;                     \
    const struct dap_config dap_config_##idx = {        \
        .rbuf = &dap_rb_##idx,                          \
        .usb_config = &dap_usb_config_##idx,            \
    };                                                  \
                                                        \
    DEVICE_DT_INST_DEFINE(                              \
        idx,                                            \
        dap_init,                                       \
        NULL,                                           \
        &dap_data_##idx,                                \
        &dap_config_##idx,                              \
        APPLICATION,                                    \
        40,                                             \
        NULL,                                           \
    );

DT_INST_FOREACH_STATUS_OKAY(DAP_DT_DEVICE_DEFINE);
