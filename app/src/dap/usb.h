#ifndef __DAP_USB_PRIV_H__
#define __DAP_USB_PRIV_H__

#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_cdc.h>
#include <usb_descriptor.h>

#include "../usb.h"

/* usb descriptor max packet size */
#if CONFIG_USB_DC_HAS_HS_SUPPORT
#define DAP_BULK_EP_MPS (512)
#else
#define DAP_BULK_EP_MPS (64)
#endif

struct dap_usb_descriptor {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

void dap_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber);
void dap_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param);

#define DAP_USB_CONFIG_DEFINE(config_name, idx)                                                     \
    USBD_CLASS_DESCR_DEFINE(primary, idx) struct dap_usb_descriptor dap_usb_descriptor_##idx = {    \
        .if0 = {                                                                                    \
            .bLength = sizeof(struct usb_if_descriptor),                                            \
            .bDescriptorType = USB_DESC_INTERFACE,                                                  \
            .bInterfaceNumber = 0,                                                                  \
            .bAlternateSetting = 0,                                                                 \
            .bNumEndpoints = 2,                                                                     \
            .bInterfaceClass = USB_BCC_VENDOR,                                                      \
            .bInterfaceSubClass = 0,                                                                \
            .bInterfaceProtocol = 0,                                                                \
            .iInterface = 0,                                                                        \
        },                                                                                          \
        .if0_out_ep = {                                                                             \
            .bLength = sizeof(struct usb_ep_descriptor),                                            \
            .bDescriptorType = USB_DESC_ENDPOINT,                                                   \
            .bEndpointAddress = AUTO_EP_OUT,                                                        \
            .bmAttributes = USB_DC_EP_BULK,                                                         \
            .wMaxPacketSize = DAP_BULK_EP_MPS,                                                      \
            .bInterval = 0,                                                                         \
        },                                                                                          \
        .if0_in_ep = {                                                                              \
            .bLength = sizeof(struct usb_ep_descriptor),                                            \
            .bDescriptorType = USB_DESC_ENDPOINT,                                                   \
            .bEndpointAddress = AUTO_EP_IN,                                                         \
            .bmAttributes = USB_DC_EP_BULK,                                                         \
            .wMaxPacketSize = DAP_BULK_EP_MPS,                                                      \
            .bInterval = 0,                                                                         \
        },                                                                                          \
    };                                                                                              \
                                                                                                    \
    static struct usb_ep_cfg_data dap_usb_ep_data_##idx[] = {                                       \
        { .ep_cb = usb_transfer_ep_callback, .ep_addr = AUTO_EP_OUT },                              \
        { .ep_cb = usb_transfer_ep_callback, .ep_addr = AUTO_EP_IN },                               \
    };                                                                                              \
                                                                                                    \
    USBD_DEFINE_CFG_DATA(config_name) = {                                                           \
        .usb_device_description = NULL,                                                             \
        .interface_config = dap_usb_interface_config,                                               \
        .interface_descriptor = &dap_usb_descriptor_##idx.if0,                                      \
        .cb_usb_status = dap_usb_status_cb,                                                         \
        .interface = {                                                                              \
            .class_handler = NULL,                                                                  \
            .custom_handler = usb_winusb_custom_handle_req,                                         \
            .vendor_handler = usb_winusb_vendor_handle_req,                                         \
        },                                                                                          \
        .num_endpoints = ARRAY_SIZE(dap_usb_ep_data_##idx),                                         \
        .endpoint = dap_usb_ep_data_##idx,                                                          \
    };

#endif /* __DAP_USB_PRIV_H__ */
