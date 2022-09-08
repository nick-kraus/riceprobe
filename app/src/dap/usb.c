#include <logging/log.h>
#include <sys/byteorder.h>
#include <usb/usb_device.h>
#include <usb_descriptor.h>
#include <zephyr.h>

#include "../usb.h"
#include "dap/dap.h"
#include "dap/usb.h"

LOG_MODULE_DECLARE(dap);

#define DAP_USB_INTERFACE_STRING	"Rice CMSIS-DAP v2"
#define DAP_USB_OUT_EP_IDX			0
#define DAP_USB_IN_EP_IDX			1

USBD_STRING_DESCR_USER_DEFINE(primary) struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[USB_BSTRING_LENGTH(DAP_USB_INTERFACE_STRING)];
} __packed dap_interface_string_descriptor = {
	.bLength = USB_STRING_DESCRIPTOR_LENGTH(DAP_USB_INTERFACE_STRING),
	.bDescriptorType = USB_DESC_STRING,
	.bString = DAP_USB_INTERFACE_STRING,
};

static void dap_usb_write_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    const struct dap_config *config = dev->config;
    int32_t ret;

	LOG_DBG("write_cb, ep 0x%x, %d bytes", ep, size);

	// finishing the buffer read that was started in the read callback
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

static void dap_usb_read_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    const struct dap_config *config = dev->config;
	struct usb_cfg_data *cfg = config->usb_config;
    int32_t ret;

	LOG_DBG("read_cb, ep 0x%x, %d bytes", ep, size);
    if (size > 0) {
        // the data will already exist in the buffer from the previous read_cb call
        ret = ring_buf_put_finish(config->rbuf, size);
        if (ret < 0) {
            LOG_ERR("buffer write finish failed with error %d", ret);
        } else {
			uint8_t *ptr;
			int32_t size = ring_buf_get_claim(config->rbuf, &ptr, DAP_RING_BUF_SIZE);
			if (size == 0) {
				LOG_DBG("ring buffer empty, nothing to send");
				return;
			}

            usb_transfer(
				cfg->endpoint[DAP_USB_IN_EP_IDX].ep_addr,
				ptr,
				size,
				USB_TRANS_WRITE,
				dap_usb_write_cb,
				(void*) dev
			);
        }
    }

	// write data into the largest continuous buffer space available within the ring bufer
    uint8_t *ptr;
    int32_t space = ring_buf_put_claim(config->rbuf, &ptr, DAP_RING_BUF_SIZE);
    usb_transfer(
        ep,
        ptr,
        space,
        USB_TRANS_READ,
        dap_usb_read_cb,
        (void*) dev
    );
}

void dap_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
	struct usb_if_descriptor *intf = (struct usb_if_descriptor*) head;
    struct dap_usb_descriptor *desc = CONTAINER_OF(intf, struct dap_usb_descriptor, if0);

    desc->if0.bInterfaceNumber = bInterfaceNumber;
    desc->if0.iInterface = usb_get_str_descriptor_idx(&dap_interface_string_descriptor);

    // dap functionality occupies the 'first' function in the MS OS descriptors
	usb_winusb_set_func0_interface(bInterfaceNumber);
}

void dap_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param) {
	int32_t ret;
	
	const struct device *dev = DRIVER_DEV_FROM_USB_CFG(struct dap_data, struct dap_config, cfg, dap_devlist);
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
		ret = dap_reset(dev);
		if (ret < 0) {
			LOG_ERR("device reset failed with error %d", ret);
		}
        break;
    case USB_DC_CONFIGURED:
        LOG_INF("usb device configured");
		if (!dap_is_configured(dev)) {
			ret = dap_configure(dev);
			if (ret < 0) {
				LOG_ERR("device configuration failed with error %d", ret);
				return;
			}

			dap_usb_read_cb(
				cfg->endpoint[DAP_USB_OUT_EP_IDX].ep_addr,
				0,
				(void*) dev
			);
		}
        break;
    case USB_DC_DISCONNECTED:
        LOG_INF("usb device disconnected");
		ret = dap_reset(dev);
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
