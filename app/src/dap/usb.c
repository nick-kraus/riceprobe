#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "../usb.h"
#include "dap/commands.h"
#include "dap/dap.h"
#include "dap/usb.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

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

	/* finishing the buffer read that was started in the read callback */
    ret = ring_buf_get_finish(config->response_buf, size);
    if (ret < 0) {
        LOG_ERR("buffer read finish failed with error %d", ret);
        return;
    }
}

static void dap_usb_read_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    const struct dap_config *config = dev->config;
	struct usb_cfg_data *cfg = config->usb_config;

	LOG_DBG("read_cb, ep 0x%x, %d bytes", ep, size);
	if (size <= 0) {
		goto end;
	}

	uint32_t wrote = ring_buf_put(config->request_buf, config->ep_buf, size);
	if (wrote < size) {
		LOG_ERR("request buffer full, write failed");
		ring_buf_reset(config->request_buf);
		goto end;
	}

	int response_size = dap_handle_request(dev);
	if (response_size < 0) {
		LOG_ERR("dap handle request failed with error %d", response_size);

		/* commands that failed or aren't implemented get a simple 0xff reponse byte, and we also
		 * reset the request buffer since they are probably in a bad state */
		ring_buf_reset(config->request_buf);
		ring_buf_reset(config->response_buf);
		uint8_t response = DAP_COMMAND_RESPONSE_ERROR;
		/* since just reset, this should only ever fail if the ring buffer is size 0 */
        FATAL_CHECK(ring_buf_put(config->response_buf, &response, 1) == 1, "response buf size not 1");
		response_size = 1;
	}

	uint8_t *ptr;
	uint32_t claim_size = ring_buf_get_claim(config->response_buf, &ptr, response_size);
	if (claim_size < response_size) {
		LOG_ERR("only %d bytes available in buffer for response size %d", claim_size, response_size);
		ring_buf_reset(config->response_buf);
		goto end;
	}

	usb_transfer(
		cfg->endpoint[DAP_USB_IN_EP_IDX].ep_addr,
		ptr,
		response_size,
		USB_TRANS_WRITE,
		dap_usb_write_cb,
		(void*) dev
	);

end: ;
	usb_transfer(
		ep,
		config->ep_buf,
		DAP_BULK_EP_MPS,
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

    /* dap functionality occupies the 'first' function in the MS OS descriptors */
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
        LOG_DBG("usb device configured");
		dap_usb_read_cb(
			cfg->endpoint[DAP_USB_OUT_EP_IDX].ep_addr,
			0,
			(void*) dev
		);
        break;
    case USB_DC_DISCONNECTED:
        LOG_DBG("usb device disconnected");
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
