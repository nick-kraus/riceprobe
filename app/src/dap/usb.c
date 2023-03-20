#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "../usb.h"
#include "dap/dap.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

/* make sure the underlying usb stack supports high-speed */
#ifndef CONFIG_USB_DC_HAS_HS_SUPPORT
	#error "dap driver requires high-speed usb support for 512 byte packet size"
#endif

#define DAP_USB_INTERFACE_STRING "Rice CMSIS-DAP v2"
USBD_STRING_DESCR_USER_DEFINE(primary) struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[USB_BSTRING_LENGTH(DAP_USB_INTERFACE_STRING)];
} __packed dap_interface_string_descriptor = {
	.bLength = USB_STRING_DESCRIPTOR_LENGTH(DAP_USB_INTERFACE_STRING),
	.bDescriptorType = USB_DESC_STRING,
	.bString = DAP_USB_INTERFACE_STRING,
};

static struct usb_cfg_data dap_usb_cfg;
#define DAP_USB_OUT_EP_ADDR		(dap_usb_cfg.endpoint[0].ep_addr)
#define DAP_USB_IN_EP_ADDR		(dap_usb_cfg.endpoint[1].ep_addr)

static void dap_usb_write_cb(uint8_t ep, int32_t size, void *priv) {
    const struct device *dev = priv;
    struct dap_data *data = dev->data;

    int32_t ret = ring_buf_get_finish(&data->buf.response, size);
    if (ret < 0) {
        LOG_ERR("response buffer read finish failed with error %d", ret);
    }

    k_event_post(&data->thread.event, DAP_THREAD_EVENT_WRITE_COMPLETE);
}

int32_t dap_usb_recv_begin(const struct device *dev);
static void dap_usb_read_cb(uint8_t ep, int32_t size, void *priv) {
	const struct device *dev = priv;
    struct dap_data *data = dev->data;

    int32_t ret = ring_buf_put_finish(&data->buf.request, size);
    if (ret < 0) {
        LOG_ERR("not enough space in request buffer, dropped bytes");
        return;
    }

    if (size > 0) {
        /* signal that data is ready to process, except if we receive a DAP Queue Commands request,
         * in which case we don't immediately process the data */
        if (data->buf.request_tail[0] != DAP_COMMAND_QUEUE_COMMANDS) {
            k_event_post(&data->thread.event, DAP_THREAD_EVENT_READ_READY);
        } else {
            /* continue to read data from the transport until we have a full request to process */
            if ((ret = dap_usb_recv_begin(dev)) < 0) LOG_ERR("usb receive begin failed with error %d", ret);
        }
    }
}

int32_t dap_usb_send(const struct device *dev) {
    struct dap_data *data = dev->data;

    /* because the buffer is reset before population, the full length should be available
     * from a single buffer pointer */
    uint8_t *ptr;
    uint32_t size = ring_buf_size_get(&data->buf.response);
    uint32_t claim_size = ring_buf_get_claim(&data->buf.response, &ptr, size);
    if (claim_size < size) {
        LOG_ERR("only %d bytes available in buffer for response size %d", claim_size, size);
        return -ENOBUFS;
    }

	return usb_transfer(
		DAP_USB_IN_EP_ADDR,
		ptr,
		size,
		USB_TRANS_WRITE,
		dap_usb_write_cb,
		(void*) dev
	);
}

int32_t dap_usb_recv_begin(const struct device *dev) {
    struct dap_data *data = dev->data;

    uint32_t request_space = ring_buf_put_claim(
        &data->buf.request,
        &data->buf.request_tail,
        DAP_MAX_PACKET_SIZE
    );

    if (request_space == 0) {
        return -ENOBUFS;
    } else {
        return usb_transfer(
            DAP_USB_OUT_EP_ADDR,
            data->buf.request_tail,
            request_space,
            USB_TRANS_READ,
            dap_usb_read_cb,
            (void*) dev
        );
    }
}

void dap_usb_stop(const struct device *dev) {
    ARG_UNUSED(dev);

    usb_cancel_transfer(DAP_USB_OUT_EP_ADDR);
    usb_cancel_transfer(DAP_USB_IN_EP_ADDR);
}

static void dap_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
	struct usb_if_descriptor *intf = (struct usb_if_descriptor*) head;
	intf->bInterfaceNumber = bInterfaceNumber;
	intf->iInterface = usb_get_str_descriptor_idx(&dap_interface_string_descriptor);

    /* dap functionality occupies the 'first' function in the MS OS descriptors */
	usb_winusb_set_func0_interface(bInterfaceNumber);
}

static void dap_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param) {
	const struct device *dev = DEVICE_DT_GET(DAP_DT_NODE);
	if (!device_is_ready(dev)) {
		LOG_ERR("dap device not ready");
		return;
	}
	struct dap_data *data = dev->data;

    switch (status) {
    case USB_DC_ERROR:
        LOG_ERR("usb device error");
        break;
    case USB_DC_CONFIGURED:
        LOG_DBG("usb device configured");
        /* signal opening a new connection, if not already connected */
        if (data->thread.transport == DAP_TRANSPORT_NONE) {
            k_event_post(&data->thread.event, DAP_THREAD_EVENT_USB_CONNECT);
        }
        break;
    case USB_DC_SUSPEND:
        LOG_DBG("usb device suspended");
        /* signal closing the current connection, only if connected */
        if (data->thread.transport == DAP_TRANSPORT_USB) {
            k_event_post(&data->thread.event, DAP_THREAD_EVENT_DISCONNECT);
        }
        break;
    case USB_DC_RESET:
	case USB_DC_RESUME:
    case USB_DC_CONNECTED:
    case USB_DC_DISCONNECTED:
    case USB_DC_INTERFACE:
    case USB_DC_SOF:
        break;
    case USB_DC_UNKNOWN:
	default:
		LOG_DBG("unknown usb device event");
		break;
    }
}

struct dap_usb_descriptor {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} USBD_CLASS_DESCR_DEFINE(primary, 0) dap_usb_descriptor = {
    .if0 = {
        .bLength = sizeof(struct usb_if_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_BCC_VENDOR,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,
    },
    .if0_out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = AUTO_EP_OUT,
        .bmAttributes = USB_DC_EP_BULK,
        .wMaxPacketSize = 512,
        .bInterval = 0,
    },
    .if0_in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = AUTO_EP_IN,
        .bmAttributes = USB_DC_EP_BULK,
        .wMaxPacketSize = 512,
        .bInterval = 0,
    },
};

static struct usb_ep_cfg_data dap_usb_ep_data[] = {
    { .ep_cb = usb_transfer_ep_callback, .ep_addr = AUTO_EP_OUT },
    { .ep_cb = usb_transfer_ep_callback, .ep_addr = AUTO_EP_IN },
};

USBD_DEFINE_CFG_DATA(dap_usb_cfg) = {
    .usb_device_description = NULL,
    .interface_config = dap_usb_interface_config,
    .interface_descriptor = &dap_usb_descriptor.if0,
    .cb_usb_status = dap_usb_status_cb,
    .interface = {
        .class_handler = NULL,
        .custom_handler = usb_winusb_custom_handle_req,
        .vendor_handler = usb_winusb_vendor_handle_req,
    },
    .num_endpoints = ARRAY_SIZE(dap_usb_ep_data),
    .endpoint = dap_usb_ep_data,
};
