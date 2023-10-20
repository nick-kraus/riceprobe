#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "transport.h"
#include "usb_msos.h"

LOG_MODULE_REGISTER(io_usb, CONFIG_IO_LOG_LEVEL);

#define IO_USB_INTERFACE_STRING "Rice I/O v1"
USBD_STRING_DESCR_USER_DEFINE(primary) struct {
	uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[USB_BSTRING_LENGTH(IO_USB_INTERFACE_STRING)];
} __packed io_interface_string_descriptor = {
	.bLength = USB_STRING_DESCRIPTOR_LENGTH(IO_USB_INTERFACE_STRING),
	.bDescriptorType = USB_DESC_STRING,
	.bString = IO_USB_INTERFACE_STRING,
};

static void io_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
	struct usb_if_descriptor *intf = (struct usb_if_descriptor*) head;
	intf->bInterfaceNumber = bInterfaceNumber;
	intf->iInterface = usb_get_str_descriptor_idx(&io_interface_string_descriptor);

    /* io functionality occupies the 'second' function in the MS OS descriptors */
	usb_msos_set_func1_interface(bInterfaceNumber);
}

static K_SEM_DEFINE(io_usb_thread_wake, 0, 1);

static volatile bool io_usb_configured = false;
static void io_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param) {
    if (status == USB_DC_CONFIGURED) {
        io_usb_configured = true;
    } else if (status == USB_DC_SUSPEND ||
               status == USB_DC_RESET ||
               status == USB_DC_DISCONNECTED ||
               status == USB_DC_ERROR) {
        
        if (status == USB_DC_ERROR) LOG_ERR("usb device error");
        io_usb_configured = false;
        /* wake the thread potentially waiting on a read or write */
        k_sem_give(&io_usb_thread_wake);
    }
}

static void io_usb_send_cb(uint8_t ep, int32_t size, void *priv) {
    ARG_UNUSED(ep);

    int32_t *size_ptr = priv;
    *size_ptr = size;

    k_sem_give(&io_usb_thread_wake);
}

static void io_usb_recv_cb(uint8_t ep, int32_t size, void *priv) {
    ARG_UNUSED(ep);

    int32_t *size_ptr = priv;
    *size_ptr = size;

    k_sem_give(&io_usb_thread_wake);
}

struct io_usb_descriptor {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} USBD_CLASS_DESCR_DEFINE(primary, 0) io_usb_descriptor = {
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

const uint8_t io_out_idx = 0;
const uint8_t io_in_idx = 1;
static struct usb_ep_cfg_data io_usb_ep_data[] = {
    { .ep_cb = usb_transfer_ep_callback, .ep_addr = AUTO_EP_OUT },
    { .ep_cb = usb_transfer_ep_callback, .ep_addr = AUTO_EP_IN },
};

USBD_DEFINE_CFG_DATA(io_usb_cfg) = {
    .usb_device_description = NULL,
    .interface_config = io_usb_interface_config,
    .interface_descriptor = &io_usb_descriptor.if0,
    .cb_usb_status = io_usb_status_cb,
    .interface = {
        .class_handler = NULL,
        .custom_handler = usb_msos_custom_handle_req,
        .vendor_handler = usb_msos_vendor_handle_req,
    },
    .num_endpoints = ARRAY_SIZE(io_usb_ep_data),
    .endpoint = io_usb_ep_data,
};

int32_t io_usb_transport_init(void) {
    /* all USB initialization is static, no need to do anything here. */
    return 0;
}

int32_t io_usb_transport_configure(void) {
    return io_usb_configured ? 0 : -EAGAIN;
}

int32_t io_usb_transport_recv(uint8_t *read, size_t len) {    
    int32_t recv_size = 0;
    int32_t ret = usb_transfer(
        io_usb_ep_data[io_out_idx].ep_addr,
        read,
        len,
        USB_TRANS_READ,
        io_usb_recv_cb,
        (void*) &recv_size
    );
    if (ret < 0) return ret;

    while (usb_transfer_is_busy(io_usb_ep_data[io_out_idx].ep_addr)) {
        k_sem_take(&io_usb_thread_wake, K_MSEC(100));

        if (!io_usb_configured) {
            usb_cancel_transfer(io_usb_ep_data[io_out_idx].ep_addr);
            return -ESHUTDOWN;
        }
    }

    return recv_size;
}

int32_t io_usb_transport_send(uint8_t *send, size_t len) {
    int32_t send_size = 0;
    int32_t ret = usb_transfer(
        io_usb_ep_data[io_in_idx].ep_addr,
        send,
        len,
        USB_TRANS_WRITE,
        io_usb_send_cb,
        (void*) &send_size
    );
    if (ret < 0) return ret;

    while (usb_transfer_is_busy(io_usb_ep_data[io_in_idx].ep_addr)) {
        k_sem_take(&io_usb_thread_wake, K_MSEC(100));

        if (!io_usb_configured) {
            usb_cancel_transfer(io_usb_ep_data[io_in_idx].ep_addr);
            return -ESHUTDOWN;
        }
    }

    return send_size;
}

IO_TRANSPORT_DEFINE(
    io_usb,
    io_usb_transport_init,
    io_usb_transport_configure,
    io_usb_transport_recv,
    io_usb_transport_send
);
