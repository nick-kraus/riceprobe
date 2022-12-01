#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "../usb.h"
#include "io/io.h"
#include "io/usb.h"
#include "util.h"

LOG_MODULE_DECLARE(io, CONFIG_IO_LOG_LEVEL);

#define IO_USB_INTERFACE_STRING     "Rice I/O v1"
#define IO_USB_OUT_EP_IDX			0
#define IO_USB_IN_EP_IDX			1

static void io_usb_read_cb(uint8_t ep, int32_t size, void *priv);

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
	struct usb_cfg_data *cfg = config->usb_config;

	/* finishing the buffer read that was started in the read callback */
	int32_t ret = ring_buf_get_finish(config->response_buf, size);
	if (ret < 0) {
		LOG_ERR("buffer read finish failed with error %d", ret);
		ring_buf_reset(config->response_buf);
	}

	/* start the next request read */
	usb_transfer(
		cfg->endpoint[IO_USB_OUT_EP_IDX].ep_addr,
		config->ep_buf,
		IO_BULK_EP_MPS,
		USB_TRANS_READ,
		io_usb_read_cb,
		(void*) dev
	);
}

static void io_usb_read_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    const struct io_config *config = dev->config;
	struct usb_cfg_data *cfg = config->usb_config;

	if (size <= 0) {
		io_usb_write_cb(cfg->endpoint[IO_USB_IN_EP_IDX].ep_addr, 0, (void*) dev);
		return;
	}

	uint32_t wrote = ring_buf_put(config->request_buf, config->ep_buf, size);
	if (wrote < size) {
		LOG_ERR("request buffer full, write failed");
		ring_buf_reset(config->request_buf);
		io_usb_write_cb(cfg->endpoint[IO_USB_IN_EP_IDX].ep_addr, 0, (void*) dev);
		return;
	}

	/* by this point the previous response will have been received by the host and it should be safe to clear
	 * the response buffer */
	ring_buf_reset(config->response_buf);
	int response_size = io_handle_request(dev);
	if (response_size < 0) {
		/* response size is the two's-complement of an error code as defined in commands.h */
		uint8_t command_error = (uint8_t) -1 * response_size;
		LOG_ERR("handle request failed with error %d", command_error);

		/* unhandled commands or catastrophic errors will be returned with a zeroed id */
		ring_buf_reset(config->request_buf);
		ring_buf_reset(config->response_buf);
		uint8_t response[] = {0x00, (uint8_t) command_error};
		/* since just reset, this should only ever fail if the ring buffer is size 0 */
        FATAL_CHECK(ring_buf_put(config->response_buf, response, 2) == 2, "response buf size not 2");
		response_size = 2;
	}

	uint8_t *ptr;
	uint32_t claim_size = ring_buf_get_claim(config->response_buf, &ptr, response_size);
	if (claim_size < response_size) {
		LOG_ERR("only %d bytes available in buffer for response size %d", claim_size, response_size);
		ring_buf_reset(config->response_buf);
		io_usb_write_cb(cfg->endpoint[IO_USB_IN_EP_IDX].ep_addr, 0, (void*) dev);
		return;
	}

	usb_transfer(
		cfg->endpoint[IO_USB_IN_EP_IDX].ep_addr,
		ptr,
		response_size,
		USB_TRANS_WRITE,
		io_usb_write_cb,
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
		io_usb_read_cb(
			cfg->endpoint[IO_USB_OUT_EP_IDX].ep_addr,
			0,
			(void*) dev
		);
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
