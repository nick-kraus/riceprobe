#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "transport.h"
#include "usb_msos.h"

LOG_MODULE_REGISTER(dap_usb, CONFIG_DAP_LOG_LEVEL);

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

static void dap_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber) {
	struct usb_if_descriptor *intf = (struct usb_if_descriptor*) head;
	intf->bInterfaceNumber = bInterfaceNumber;
	intf->iInterface = usb_get_str_descriptor_idx(&dap_interface_string_descriptor);

    /* dap functionality occupies the 'first' function in the MS OS descriptors */
	usb_msos_set_func0_interface(bInterfaceNumber);
}

static K_SEM_DEFINE(dap_usb_thread_wake, 0, 1);

static volatile bool dap_usb_configured = false;
static void dap_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param) {
    if (status == USB_DC_CONFIGURED) {
        dap_usb_configured = true;
    } else if (status == USB_DC_SUSPEND ||
               status == USB_DC_RESET ||
               status == USB_DC_DISCONNECTED ||
               status == USB_DC_ERROR) {
        
        if (status == USB_DC_ERROR) LOG_ERR("usb device error");
        dap_usb_configured = false;
        /* wake the thread potentially waiting on a read or write */
        k_sem_give(&dap_usb_thread_wake);
    }
}

static void dap_usb_send_cb(uint8_t ep, int32_t size, void *priv) {
    ARG_UNUSED(ep);

    int32_t *size_ptr = priv;
    *size_ptr = size;

    k_sem_give(&dap_usb_thread_wake);
}

static void dap_usb_recv_cb(uint8_t ep, int32_t size, void *priv) {
    ARG_UNUSED(ep);

    int32_t *size_ptr = priv;
    *size_ptr = size;

    k_sem_give(&dap_usb_thread_wake);
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

const uint8_t dap_out_idx = 0;
const uint8_t dap_in_idx = 1;
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
        .custom_handler = usb_msos_custom_handle_req,
        .vendor_handler = usb_msos_vendor_handle_req,
    },
    .num_endpoints = ARRAY_SIZE(dap_usb_ep_data),
    .endpoint = dap_usb_ep_data,
};

int32_t dap_usb_transport_init(void) {
    /* all USB initialization is static, no need to do anything here. */
    return 0;
}

int32_t dap_usb_transport_configure(void) {
    return dap_usb_configured ? 0 : -EAGAIN;
}

int32_t dap_usb_transport_recv(uint8_t *read, size_t len) {    
    int32_t recv_size = 0;
    int32_t ret = usb_transfer(
        dap_usb_ep_data[dap_out_idx].ep_addr,
        read,
        len,
        USB_TRANS_READ,
        dap_usb_recv_cb,
        (void*) &recv_size
    );
    if (ret < 0) return ret;

    while (usb_transfer_is_busy(dap_usb_ep_data[dap_out_idx].ep_addr)) {
        k_sem_take(&dap_usb_thread_wake, K_MSEC(100));

        if (!dap_usb_configured) {
            usb_cancel_transfer(dap_usb_ep_data[dap_out_idx].ep_addr);
            return -ESHUTDOWN;
        }
    }

    return recv_size;
}

int32_t dap_usb_transport_send(uint8_t *send, size_t len) {
    int32_t send_size = 0;
    int32_t ret = usb_transfer(
        dap_usb_ep_data[dap_in_idx].ep_addr,
        send,
        len,
        USB_TRANS_WRITE,
        dap_usb_send_cb,
        (void*) &send_size
    );
    if (ret < 0) return ret;

    while (usb_transfer_is_busy(dap_usb_ep_data[dap_in_idx].ep_addr)) {
        k_sem_take(&dap_usb_thread_wake, K_MSEC(100));

        if (!dap_usb_configured) {
            usb_cancel_transfer(dap_usb_ep_data[dap_in_idx].ep_addr);
            return -ESHUTDOWN;
        }
    }

    return send_size;
}

DAP_TRANSPORT_DEFINE(
    dap_usb,
    dap_usb_transport_init,
    dap_usb_transport_configure,
    dap_usb_transport_recv,
    dap_usb_transport_send
);
