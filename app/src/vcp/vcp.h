#ifndef __VCP_PRIV_H__
#define __VCP_PRIV_H__

#include <drivers/uart.h>
#include <sys/ring_buffer.h>
#include <usb/usb_device.h>
#include <zephyr.h>

/* size of the internal buffers in bytes */
#define VCP_RING_BUF_SIZE (1024)

struct vcp_data {
    bool configured;

    /* writes any received uart data to the host transport */
    struct k_work rx_work;

    /* the device self reference and device list node are used by the transport to get a
     * handle to the correct driver instance, to retreive the *data and *config structs */
    const struct device *dev;
    sys_snode_t devlist_node;
};

struct vcp_config {
    const struct device *uart_dev;

    /* tx_rbuf will receive data from the transport and transmit through the uart
     * device, and vice-versa for rx_buf */
    struct ring_buf *rx_rbuf;
    struct ring_buf *tx_rbuf;

    struct usb_cfg_data *usb_config;
};

extern sys_slist_t vcp_devlist;

bool vcp_is_configured(const struct device *dev);
int32_t vcp_configure(const struct device *dev, k_work_handler_t handler);
int32_t vcp_reset(const struct device *dev);

#endif /* __VCP_PRIV_H__ */
