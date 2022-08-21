#include <logging/log.h>
#include <sys/byteorder.h>
#include <usb/usb_device.h>
#include <usb_descriptor.h>
#include <zephyr.h>

#include "winusb.h"

LOG_MODULE_REGISTER(io);

#define IO_USB_INTERFACE_STRING		"Rice I/O v1"
#define IO_OUT_EP_INDEX				0
#define IO_IN_EP_INDEX 				1

USBD_STRING_DESCR_USER_DEFINE(primary) struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[USB_BSTRING_LENGTH(IO_USB_INTERFACE_STRING)];
} __packed io_interface_string_descriptor = {
	.bLength = USB_STRING_DESCRIPTOR_LENGTH(IO_USB_INTERFACE_STRING),
	.bDescriptorType = USB_DESC_STRING,
	.bString = IO_USB_INTERFACE_STRING,
};

USBD_CLASS_DESCR_DEFINE(primary, 0) struct {
    struct usb_association_descriptor iad;
    struct usb_if_descriptor intf;
    struct usb_ep_descriptor out_ep;
	struct usb_ep_descriptor in_ep;
} __packed io_usb_descriptor = {
    .iad = {
        .bLength = sizeof(struct usb_association_descriptor),
        .bDescriptorType = USB_DESC_INTERFACE_ASSOC,
        .bFirstInterface = 0,
        .bInterfaceCount = 1,
        .bFunctionClass = USB_BCC_VENDOR,
        .bFunctionSubClass = 0,
        .bFunctionProtocol = 0,
        .iFunction = 0,
    },
    .intf = {
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
    .out_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = AUTO_EP_OUT,
		.bmAttributes = USB_DC_EP_BULK,
		.wMaxPacketSize = sys_cpu_to_le16(USB_MAX_FS_BULK_MPS),
		.bInterval = 0,
    },
    .in_ep = {
        .bLength = sizeof(struct usb_ep_descriptor),
        .bDescriptorType = USB_DESC_ENDPOINT,
		.bEndpointAddress = AUTO_EP_IN,
		.bmAttributes = USB_DC_EP_BULK,
		.wMaxPacketSize = sys_cpu_to_le16(USB_MAX_FS_BULK_MPS),
		.bInterval = 0,
    },
};

static void io_usb_if_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
    ARG_UNUSED(head);

    io_usb_descriptor.iad.bFirstInterface = bInterfaceNumber;
    io_usb_descriptor.intf.bInterfaceNumber = bInterfaceNumber;
    io_usb_descriptor.intf.iInterface = usb_get_str_descriptor_idx(&io_interface_string_descriptor);

    // io functionality occupies the 'second' function in the MS OS descriptors
    *winusb_func1_first_interface = bInterfaceNumber;
}

static uint8_t ep_buf[64];

static void io_write_complete_cb(uint8_t ep_addr, int size, void *priv) {
    LOG_DBG("wrote %d bytes to USB ep 0x%x", size, ep_addr);
}

static void io_transfer_cb(uint8_t ep_addr, int size, void *priv) {
    struct usb_cfg_data *cfg = priv;

    LOG_DBG("read %d bytes from USB ep 0x%x", size, ep_addr);

    if (size > 0) {
        usb_transfer(
            cfg->endpoint[IO_IN_EP_INDEX].ep_addr,
            ep_buf,
            size,
            USB_TRANS_WRITE,
            io_write_complete_cb,
            cfg
        );
    }

    usb_transfer(
        ep_addr,
        ep_buf,
        ARRAY_SIZE(ep_buf),
        USB_TRANS_READ,
        io_transfer_cb,
        cfg
    );
}

static void io_usb_status_cb(
	struct usb_cfg_data *cfg,
	enum usb_dc_status_code status,
	const uint8_t *param
) {
    ARG_UNUSED(cfg);
	ARG_UNUSED(param);

	switch (status) {
	case USB_DC_ERROR:
		LOG_ERR("usb device error");
		break;
	case USB_DC_CONFIGURED:
		LOG_INF("USB device configured");
        io_transfer_cb(cfg->endpoint[IO_OUT_EP_INDEX].ep_addr, 0, cfg);
		break;
	case USB_DC_SUSPEND:
		LOG_INF("USB device suspended");
        usb_cancel_transfer(cfg->endpoint[IO_OUT_EP_INDEX].ep_addr);
		break;
	case USB_DC_RESET:
	case USB_DC_DISCONNECTED:
		break;
	case USB_DC_UNKNOWN:
	default:
		LOG_DBG("unknown usb device event");
		break;
	}
}

static struct usb_ep_cfg_data io_usb_ep_data[] = {
	{
		.ep_cb = usb_transfer_ep_callback,
		.ep_addr = AUTO_EP_OUT,
	}, {
		.ep_cb = usb_transfer_ep_callback,
		.ep_addr = AUTO_EP_IN,
	},
};

USBD_DEFINE_CFG_DATA(io_usb_config) = {
	.usb_device_description = NULL,
	.interface_descriptor = &io_usb_descriptor.intf,
	.interface_config = io_usb_if_config,
	.cb_usb_status = io_usb_status_cb,
	.interface = {
		.class_handler = NULL,
		.custom_handler = winusb_custom_handle_req,
		.vendor_handler = winusb_vendor_handle_req,
	},
	.num_endpoints = ARRAY_SIZE(io_usb_ep_data),
	.endpoint = io_usb_ep_data,
};
