#include <device.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <sys/ring_buffer.h>
#include <sys/slist.h>
#include <zephyr.h>

#include "vcp/usb.h"
#include "vcp/vcp.h"

LOG_MODULE_REGISTER(vcp);

bool vcp_is_configured(const struct device *dev) {
    struct vcp_data *data = dev->data;
    return data->configured;
}

int32_t vcp_configure(const struct device *dev, k_work_handler_t handler) {
    struct vcp_data *data = dev->data;
    const struct vcp_config *config = dev->config;

    if (data->configured) {
        return 0;
    }

    ring_buf_reset(config->rx_rbuf);
    ring_buf_reset(config->tx_rbuf);

    k_work_init(&data->rx_work, handler);
    uart_irq_rx_enable(config->uart_dev);
    data->configured = true;
    return 0;
}

int32_t vcp_reset(const struct device *dev) {
    struct vcp_data *data = dev->data;
    const struct vcp_config *config = dev->config;

    data->configured = false;

    uart_irq_rx_disable(config->uart_dev);
    uart_irq_tx_disable(config->uart_dev);
    ring_buf_reset(config->rx_rbuf);
    ring_buf_reset(config->tx_rbuf);

    /* wait for the work handler to be idle, then cancel it, to allow a different
     * transport to re-configure it in the future */
    while (k_work_cancel(&data->rx_work)) {
        k_sleep(K_MSEC(1));
    }

    struct uart_config uart_config = {
        .baudrate = 115200,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    return uart_configure(config->uart_dev, &uart_config);
}

static void vcp_uart_isr(const struct device *dev, void *user_data) {
    const struct device *vcp_dev = user_data;
    struct vcp_data *data = vcp_dev->data;
    const struct vcp_config *config = vcp_dev->config;
    int32_t ret;

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (uart_irq_rx_ready(dev)) {
            uint8_t *ptr;
            uint32_t space = ring_buf_put_claim(config->rx_rbuf, &ptr, VCP_RING_BUF_SIZE);
            if (space == 0) {
                /* TODO: with hardware flow control, we just set the proper lines and disable
                 * the receive irq instead of flushing the uart stream */
                uint8_t drop;
                LOG_ERR("receive buffer full, flushing uart data");
                while (uart_fifo_read(dev, &drop, 1) > 0) {
                    continue;
                }
                break;
            }

            int32_t read = uart_fifo_read(dev, ptr, space);
            if (read <= 0) {
                LOG_ERR("uart fifo read failed with error %d", read);
                read = 0;
            }
            ring_buf_put_finish(config->rx_rbuf, read);
            ret = k_work_submit(&data->rx_work);
            if (ret < 0) {
                LOG_ERR("transport work queue submit failed with error %d", ret);
            }
        }

        if (uart_irq_tx_ready(dev)) {
            uint8_t *ptr;
            uint32_t avail = ring_buf_get_claim(config->tx_rbuf, &ptr, VCP_RING_BUF_SIZE);
            if (avail == 0) {
                /* no need to proceed further, we can disable the tx IRQ as it will be enabled
                 * the next time data is received from the host transport */
                uart_irq_tx_disable(dev);
                continue;
            }

            int32_t filled = uart_fifo_fill(dev, ptr, avail);
            if (filled <= 0) {
                LOG_ERR("uart fifo fill failed with error %d", filled);
                filled = 0;
            }
            ring_buf_get_finish(config->tx_rbuf, filled);
        }
    }
}

sys_slist_t vcp_devlist;

static int32_t vcp_init(const struct device *dev) {
    struct vcp_data *data = dev->data;
    const struct vcp_config *config = dev->config;

    if (!device_is_ready(config->uart_dev)) { return -ENODEV; }

    data->dev = dev;
    sys_slist_append(&vcp_devlist, &data->devlist_node);

    uart_irq_rx_disable(config->uart_dev);
    uart_irq_tx_disable(config->uart_dev);
    uart_irq_callback_user_data_set(config->uart_dev, vcp_uart_isr, (void*) dev);

    return vcp_reset(dev);
}

#define DT_DRV_COMPAT rice_vcp

#define VCP_DT_DEVICE_DEFINE(idx)                               \
                                                                \
    RING_BUF_DECLARE(vcp_rx_rb_##idx, VCP_RING_BUF_SIZE);       \
    RING_BUF_DECLARE(vcp_tx_rb_##idx, VCP_RING_BUF_SIZE);       \
                                                                \
    VCP_USB_CONFIG_DEFINE(vcp_usb_config_##idx, idx);           \
                                                                \
    struct vcp_data vcp_data_##idx;                             \
    const struct vcp_config vcp_config_##idx = {                \
        .uart_dev = DEVICE_DT_GET(DT_INST_PHANDLE(idx, uart)),  \
        .rx_rbuf = &vcp_rx_rb_##idx,                            \
        .tx_rbuf = &vcp_tx_rb_##idx,                            \
        .usb_config = &vcp_usb_config_##idx,                    \
    };                                                          \
                                                                \
    DEVICE_DT_INST_DEFINE(                                      \
        idx,                                                    \
        vcp_init,                                               \
        NULL,                                                   \
        &vcp_data_##idx,                                        \
        &vcp_config_##idx,                                      \
        APPLICATION,                                            \
        40,                                                     \
        NULL,                                                   \
    );

DT_INST_FOREACH_STATUS_OKAY(VCP_DT_DEVICE_DEFINE);
