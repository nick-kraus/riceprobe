#include <drivers/uart.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <usb/usb_device.h>
#include <usb/class/usb_cdc.h>
#include <usb_descriptor.h>
#include <zephyr.h>

#include "../usb.h"
#include "vcp/usb.h"
#include "vcp/vcp.h"

LOG_MODULE_DECLARE(vcp);

#define VCP_USB_INTERFACE_STRING    "Rice VCP"
#define VCP_USB_INT_EP_IDX			0
#define VCP_USB_OUT_EP_IDX			1
#define VCP_USB_IN_EP_IDX			2

USBD_STRING_DESCR_USER_DEFINE(primary) struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[USB_BSTRING_LENGTH(VCP_USB_INTERFACE_STRING)];
} __packed vcp_interface_string_descriptor = {
	.bLength = USB_STRING_DESCRIPTOR_LENGTH(VCP_USB_INTERFACE_STRING),
	.bDescriptorType = USB_DESC_STRING,
	.bString = VCP_USB_INTERFACE_STRING,
};

int32_t vcp_usb_class_handle_req(
    struct usb_setup_packet *setup,
    int32_t *len,
    uint8_t **data
) {
    int32_t ret;
    uint8_t intf_num = (uint8_t) (setup->wIndex & 0xFF);
    const struct device *dev = DRIVER_DEV_FROM_USB_INTF(struct vcp_data, struct vcp_config, intf_num, vcp_devlist);
    if (dev == NULL) {
        LOG_WRN("device data not found for interface %u", intf_num);
        return -ENODEV;
    }
    const struct vcp_config *config = dev->config;

    if (usb_reqtype_is_to_device(setup)) {
        if (setup->bRequest == SET_LINE_CODING) {
            struct cdc_acm_line_coding usb_line_coding;
            memcpy(&usb_line_coding, *data, sizeof(usb_line_coding));

            // cdc_acm_line_coding dwDTERate and bParityType have 1:1 mapping
            // with uart_config baudrate and parity, but the other values need
            // to be mapped.

            // uart stop bits supports one extra value before the bCharFormat
            // equivalent values are supported at a 1:1 mapping
            uint8_t uart_stop_bits = usb_line_coding.bCharFormat + 1;
            uint8_t uart_data_bits;
            switch (usb_line_coding.bDataBits) {
            case 5:
                uart_data_bits = UART_CFG_DATA_BITS_5;
                break;
            case 6:
                uart_data_bits = UART_CFG_DATA_BITS_6;
                break;
            case 7:
                uart_data_bits = UART_CFG_DATA_BITS_7;
                break;
            case 8:
                uart_data_bits = UART_CFG_DATA_BITS_8;
                break;
            case 16:
            default:
                LOG_ERR("bDataBits of %u is not supported", usb_line_coding.bDataBits);
                return -ENOTSUP;
            }

            struct uart_config uart_line_coding = {
                .baudrate = sys_le32_to_cpu(usb_line_coding.dwDTERate),
                .parity = usb_line_coding.bParityType,
                .stop_bits = uart_stop_bits,
                .data_bits = uart_data_bits,
                .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
            };

            ret = uart_configure(config->uart_dev, &uart_line_coding);
            if (ret < 0) {
                LOG_ERR("uart configure failed with error %d", ret);
            }
            return ret;
        } else if (setup->bRequest == SET_CONTROL_LINE_STATE) {
            // TODO: support setting the control line state
            LOG_DBG("setting control line state is not currently supported");
            return 0;
        }
    } else {
        if (setup->bRequest == GET_LINE_CODING) {
            // static so we can safely return the pointer to this structure after the
            // function has returned
            static struct cdc_acm_line_coding usb_line_coding;
            struct uart_config uart_line_coding;
            uart_config_get(config->uart_dev, &uart_line_coding);

            // 16 bit data isn't supported by most zephyr uart drivers, and
            // won't even be considered here
            uint8_t b_data_bits;
            switch (uart_line_coding.data_bits) {
            case UART_CFG_DATA_BITS_5:
                b_data_bits = 5;
                break;
            case UART_CFG_DATA_BITS_6:
                b_data_bits = 6;
                break;
            case UART_CFG_DATA_BITS_7:
                b_data_bits = 7;
                break;
            case UART_CFG_DATA_BITS_8:
                b_data_bits = 8;
                break;
            default:
                return -ENOTSUP;
            }

            usb_line_coding = (struct cdc_acm_line_coding) {
                .dwDTERate = sys_cpu_to_le32(uart_line_coding.baudrate),
                // here we must convert the bCharFormat value in the opposite way as above
                .bCharFormat = uart_line_coding.stop_bits - 1,
                .bParityType = uart_line_coding.parity,
                .bDataBits = b_data_bits,
            };

            *data = (uint8_t*) &usb_line_coding;
            *len = sizeof(usb_line_coding);
            return 0;
        }
    }

    LOG_DBG(
        "request type 0x%x and request 0x%x unsupported",
        setup->bmRequestType,
        setup->bRequest
    );
    return -ENOTSUP;
}

static void vcp_usb_write_cb(uint8_t ep, int32_t size, void *priv) {
    const struct device *dev = priv;
    struct vcp_data *data = dev->data;
    const struct vcp_config *config = dev->config;
    int32_t ret;

    LOG_DBG("write_cb, ep 0x%x, %d bytes", ep, size);

    // finishing the buffer read that was started in the work handler
    ret = ring_buf_get_finish(config->rx_rbuf, size);
    if (ret < 0) {
        LOG_ERR("rx buffer read finish failed with error %d", ret);
        return;
    }
    if (ring_buf_is_empty(config->rx_rbuf)) {
        LOG_DBG("transmit work complete");
        return;
    }

    ret = k_work_submit(&data->rx_work);
    if (ret < 0) {
        LOG_ERR("transport work queue submit failed with error %d", ret);
    }
}

static void usb_work_handler(struct k_work *work) {
    const struct device *dev = CONTAINER_OF(work, struct vcp_data, rx_work)->dev;
    const struct vcp_config *config = dev->config;
    struct usb_cfg_data *usb_config = config->usb_config;

    uint8_t in_ep = usb_config->endpoint[VCP_USB_IN_EP_IDX].ep_addr;
    if (usb_transfer_is_busy(in_ep)) {
        LOG_DBG("usb transfer ongoing");
        return;
    }

    uint8_t *ptr;
    uint32_t size = ring_buf_get_claim(config->rx_rbuf, &ptr, VCP_RING_BUF_SIZE);
    if (size == 0) {
        LOG_DBG("rx ring buffer empty, nothing to send");
        return;
    }

    /*
     * Prevent transferring data sizes that exactly match the packet size, causing a
     * zero-length packet which indicate to the host that no more data is to be
     * received over the transport stream.
     */
    if (size % VCP_BULK_EP_MPS == 0) {
        size--;
    }

    usb_transfer(
        in_ep,
        ptr,
        size,
        USB_TRANS_WRITE,
        vcp_usb_write_cb,
        (void*) dev
    );
}

static void vcp_usb_read_cb(uint8_t ep, int32_t size, void *priv) {
    const struct device *dev = priv;
    const struct vcp_config *config = dev->config;
    int32_t ret;

    LOG_DBG("read_cb, ep 0x%x, %d bytes", ep, size);
    if (size > 0) {
        // the data will already exist in the buffer from the previous read_cb call
        ret = ring_buf_put_finish(config->tx_rbuf, size);
        if (ret < 0) {
            LOG_ERR("tx buffer write finish failed with error %d", ret);
        } else {
            uart_irq_tx_enable(config->uart_dev);
        }
    }

    // write data into the largest continuous buffer space available within the ring bufer
    uint8_t *ptr;
    uint32_t space = ring_buf_put_claim(config->tx_rbuf, &ptr, VCP_RING_BUF_SIZE);
    usb_transfer(
        ep,
        ptr,
        space,
        USB_TRANS_READ,
        vcp_usb_read_cb,
        (void*) dev
    );
}
    
void vcp_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
    struct usb_if_descriptor *intf = (struct usb_if_descriptor*) head;
    struct vcp_usb_descriptor *desc = CONTAINER_OF(intf, struct vcp_usb_descriptor, if0);

    desc->iad_cdc.bFirstInterface = bInterfaceNumber;
    desc->if0.bInterfaceNumber = bInterfaceNumber;
    desc->if0.iInterface = usb_get_str_descriptor_idx(&vcp_interface_string_descriptor);
    desc->if0_union.bControlInterface = bInterfaceNumber;
    desc->if0_union.bSubordinateInterface0 = bInterfaceNumber + 1;
	desc->if1.bInterfaceNumber = bInterfaceNumber + 1;
    desc->if1.iInterface = usb_get_str_descriptor_idx(&vcp_interface_string_descriptor);
}

void vcp_usb_status_cb(
    struct usb_cfg_data *cfg,
    enum usb_dc_status_code status,
    const uint8_t *param
) {
    int32_t ret;

    const struct device *dev = DRIVER_DEV_FROM_USB_CFG(struct vcp_data, struct vcp_config, cfg, vcp_devlist);
    if (dev == NULL) {
        LOG_WRN("device data not found for cfg %p", cfg);
        return;
    }

    switch (status) {
    case USB_DC_ERROR:
		LOG_ERR("usb device error");
		break;
    case USB_DC_RESET:
        LOG_DBG("usb device reset");
        ret = vcp_reset(dev);
        if (ret < 0) {
            LOG_ERR("device reset failed with error %d", ret);
        }
        break;
    case USB_DC_CONFIGURED:
        LOG_INF("usb device configured");
        if (!vcp_is_configured(dev)) {
            ret = vcp_configure(dev, usb_work_handler);
            if (ret < 0) {
                LOG_ERR("device configuration failed with error %d", ret);
                return;
            }

            vcp_usb_read_cb(
                cfg->endpoint[VCP_USB_OUT_EP_IDX].ep_addr,
                0,
                (void*) dev
            );
        }
        break;
    case USB_DC_DISCONNECTED:
        LOG_INF("usb device disconnected");
        ret = vcp_reset(dev);
        if (ret < 0) {
            LOG_ERR("device reset failed with error %d", ret);
        }
        break;
    case USB_DC_SUSPEND:
		LOG_DBG("usb device suspended");
		break;
	case USB_DC_RESUME:
		LOG_DBG("usb device resumed");
		break;
    case USB_DC_CONNECTED:
    case USB_DC_INTERFACE:
    case USB_DC_SOF:
		break;
	case USB_DC_UNKNOWN:
	default:
		LOG_DBG("unknown usb device event");
		break;
    }
}
