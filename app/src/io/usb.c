#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "../usb.h"
#include "io/io.h"
#include "io/usb.h"

LOG_MODULE_DECLARE(io, CONFIG_IO_LOG_LEVEL);

#define IO_USB_INTERFACE_STRING     "Rice I/O v1"
#define IO_USB_OUT_EP_IDX			0
#define IO_USB_IN_EP_IDX			1

USBD_STRING_DESCR_USER_DEFINE(primary) struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[USB_BSTRING_LENGTH(IO_USB_INTERFACE_STRING)];
} __packed io_interface_string_descriptor = {
	.bLength = USB_STRING_DESCRIPTOR_LENGTH(IO_USB_INTERFACE_STRING),
	.bDescriptorType = USB_DESC_STRING,
	.bString = IO_USB_INTERFACE_STRING,
};

static void io_usb_write_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    const struct io_config *config = dev->config;
    int32_t ret;

	LOG_DBG("write_cb, ep 0x%x, %d bytes", ep, size);

	/* finishing the buffer read that was started in the read callback */
    ret = ring_buf_get_finish(config->rbuf, size);
    if (ret < 0) {
        LOG_ERR("buffer read finish failed with error %d", ret);
        return;
    }
    if (ring_buf_is_empty(config->rbuf)) {
        LOG_DBG("transmit work complete");
        return;
    }
}

static void io_usb_read_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    const struct io_config *config = dev->config;
	struct usb_cfg_data *cfg = config->usb_config;
    int32_t ret;

	LOG_DBG("read_cb, ep 0x%x, %d bytes", ep, size);
    if (size > 0) {
        /* the data will already exist in the buffer from the previous read_cb call */
        ret = ring_buf_put_finish(config->rbuf, size);
        if (ret < 0) {
            LOG_ERR("buffer write finish failed with error %d", ret);
        } else {
			uint8_t *ptr;
			uint32_t size = ring_buf_get_claim(config->rbuf, &ptr, IO_RING_BUF_SIZE);
			if (size == 0) {
				LOG_DBG("ring buffer empty, nothing to send");
				return;
			}

            usb_transfer(
				cfg->endpoint[IO_USB_IN_EP_IDX].ep_addr,
				ptr,
				size,
				USB_TRANS_WRITE,
				io_usb_write_cb,
				(void*) dev
			);
        }
    }

	/* write data into the largest continuous buffer space available within the ring bufer */
    uint8_t *ptr;
    uint32_t space = ring_buf_put_claim(config->rbuf, &ptr, IO_RING_BUF_SIZE);
    usb_transfer(
        ep,
        ptr,
        space,
        USB_TRANS_READ,
        io_usb_read_cb,
        (void*) dev
    );
}

void io_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
	struct usb_if_descriptor *intf = (struct usb_if_descriptor*) head;
    struct io_usb_descriptor *desc = CONTAINER_OF(intf, struct io_usb_descriptor, if0);

    desc->if0.bInterfaceNumber = bInterfaceNumber;
    desc->if0.iInterface = usb_get_str_descriptor_idx(&io_interface_string_descriptor);

    /* io functionality occupies the 'second' function in the MS OS descriptors */
	usb_winusb_set_func1_interface(bInterfaceNumber);
}

void io_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param) {
	int32_t ret;
	
	const struct device *dev = DRIVER_DEV_FROM_USB_CFG(struct io_data, struct io_config, cfg, io_devlist);
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
		ret = io_reset(dev);
		if (ret < 0) {
			LOG_ERR("device reset failed with error %d", ret);
		}
        break;
    case USB_DC_CONFIGURED:
        LOG_DBG("usb device configured");
		if (!io_is_configured(dev)) {
			ret = io_configure(dev);
			if (ret < 0) {
				LOG_ERR("device configuration failed with error %d", ret);
				return;
			}

			io_usb_read_cb(
				cfg->endpoint[IO_USB_OUT_EP_IDX].ep_addr,
				0,
				(void*) dev
			);
		}
        break;
    case USB_DC_DISCONNECTED:
        LOG_DBG("usb device disconnected");
		ret = io_reset(dev);
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
