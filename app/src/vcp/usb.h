#ifndef __USB_PRIV_H__
#define __USB_PRIV_H__

#include <sys/byteorder.h>
#include <usb/usb_device.h>
#include <usb/class/usb_cdc.h>
#include <usb_descriptor.h>
#include <zephyr.h>

/* usb descriptor max packet size */
#if CONFIG_USB_DC_HAS_HS_SUPPORT
#define VCP_BULK_EP_MPS (512)
#else
#define VCP_BULK_EP_MPS (64)
#endif

struct vcp_usb_descriptor {
    struct usb_association_descriptor iad_cdc;
    struct usb_if_descriptor if0;
    struct cdc_header_descriptor if0_header;
    struct cdc_cm_descriptor if0_cm;
    struct cdc_acm_descriptor if0_acm;
    struct cdc_union_descriptor if0_union;
    struct usb_ep_descriptor if0_int_ep;
    struct usb_if_descriptor if1;
    struct usb_ep_descriptor if1_in_ep;
    struct usb_ep_descriptor if1_out_ep;
} __packed;

void vcp_usb_int_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status);
int vcp_usb_class_handle_req(struct usb_setup_packet *setup, int32_t *len, uint8_t **data);
void vcp_usb_interface_config(struct usb_desc_header *head, uint8_t bInterfaceNumber);
void vcp_usb_status_cb(struct usb_cfg_data *cfg, enum usb_dc_status_code status, const uint8_t *param);

#define VCP_USB_CONFIG_DEFINE(config_name, idx)                                                     \
    USBD_CLASS_DESCR_DEFINE(primary, idx) struct vcp_usb_descriptor vcp_usb_descriptor_##idx = {    \
        .iad_cdc = {                                                                                \
            .bLength = sizeof(struct usb_association_descriptor),                                   \
            .bDescriptorType = USB_DESC_INTERFACE_ASSOC,                                            \
            .bFirstInterface = 0,                                                                   \
            .bInterfaceCount = 2,                                                                   \
            .bFunctionClass = USB_BCC_CDC_CONTROL,                                                  \
            .bFunctionSubClass = ACM_SUBCLASS,                                                      \
            .bFunctionProtocol = 0,                                                                 \
            .iFunction = 0,                                                                         \
        },                                                                                          \
        .if0 = {                                                                                    \
            .bLength = sizeof(struct usb_if_descriptor),                                            \
            .bDescriptorType = USB_DESC_INTERFACE,                                                  \
            .bInterfaceNumber = 0,                                                                  \
            .bAlternateSetting = 0,                                                                 \
            .bNumEndpoints = 1,                                                                     \
            .bInterfaceClass = USB_BCC_CDC_CONTROL,                                                 \
            .bInterfaceSubClass = ACM_SUBCLASS,                                                     \
            .bInterfaceProtocol = 0,                                                                \
            .iInterface = 0,                                                                        \
        },                                                                                          \
        .if0_header = {                                                                             \
            .bFunctionLength = sizeof(struct cdc_header_descriptor),                                \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                               \
            .bDescriptorSubtype = HEADER_FUNC_DESC,                                                 \
            .bcdCDC = sys_cpu_to_le16(USB_SRN_1_1),                                                 \
        },                                                                                          \
        .if0_cm = {                                                                                 \
            .bFunctionLength = sizeof(struct cdc_cm_descriptor),                                    \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                               \
            .bDescriptorSubtype = CALL_MANAGEMENT_FUNC_DESC,                                        \
            .bmCapabilities = 2,                                                                    \
            .bDataInterface = 1,                                                                    \
        },                                                                                          \
        .if0_acm = {                                                                                \
            .bFunctionLength = sizeof(struct cdc_acm_descriptor),                                   \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                               \
            .bDescriptorSubtype = ACM_FUNC_DESC,                                                    \
            .bmCapabilities = 2,                                                                    \
        },                                                                                          \
        .if0_union = {                                                                              \
            .bFunctionLength = sizeof(struct cdc_union_descriptor),                                 \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                               \
            .bDescriptorSubtype = UNION_FUNC_DESC,                                                  \
            .bControlInterface = 0,                                                                 \
            .bSubordinateInterface0 = 1,                                                            \
        },                                                                                          \
        .if0_int_ep = {                                                                             \
            .bLength = sizeof(struct usb_ep_descriptor),                                            \
            .bDescriptorType = USB_DESC_ENDPOINT,                                                   \
            .bEndpointAddress = AUTO_EP_IN,                                                         \
            .bmAttributes = USB_DC_EP_INTERRUPT,                                                    \
            .wMaxPacketSize = sys_cpu_to_le16(USB_MAX_FS_INT_MPS),                                  \
            .bInterval = 10,                                                                        \
        },                                                                                          \
        .if1 = {                                                                                    \
            .bLength = sizeof(struct usb_if_descriptor),                                            \
            .bDescriptorType = USB_DESC_INTERFACE,                                                  \
            .bInterfaceNumber = 1,                                                                  \
            .bAlternateSetting = 0,                                                                 \
            .bNumEndpoints = 2,                                                                     \
            .bInterfaceClass = USB_BCC_CDC_DATA,                                                    \
            .bInterfaceSubClass = ACM_SUBCLASS,                                                     \
            .bInterfaceProtocol = 0,                                                                \
            .iInterface = 0,                                                                        \
        },                                                                                          \
        .if1_in_ep = {                                                                              \
            .bLength = sizeof(struct usb_ep_descriptor),                                            \
            .bDescriptorType = USB_DESC_ENDPOINT,                                                   \
            .bEndpointAddress = AUTO_EP_IN,                                                         \
            .bmAttributes = USB_DC_EP_BULK,                                                         \
            .wMaxPacketSize = sys_cpu_to_le16(VCP_BULK_EP_MPS),                                     \
            .bInterval = 0,                                                                         \
        },                                                                                          \
        .if1_out_ep = {                                                                             \
            .bLength = sizeof(struct usb_ep_descriptor),                                            \
            .bDescriptorType = USB_DESC_ENDPOINT,                                                   \
            .bEndpointAddress = AUTO_EP_OUT,                                                        \
            .bmAttributes = USB_DC_EP_BULK,                                                         \
            .wMaxPacketSize = sys_cpu_to_le16(VCP_BULK_EP_MPS),                                     \
            .bInterval = 0,                                                                         \
        },                                                                                          \
    };                                                                                              \
                                                                                                    \
    static struct usb_ep_cfg_data vcp_usb_ep_data_##idx[] = {                                       \
        {                                                                                           \
            /* TODO: when we add support for manipulating the control lines we will                 \
             * need to use this callback to ensure the host received the interrupt                  \
             * notification as requested. */                                                        \
            .ep_cb = NULL,                                                                          \
            .ep_addr = AUTO_EP_IN,                                                                  \
        }, {                                                                                        \
            .ep_cb = usb_transfer_ep_callback,                                                      \
            .ep_addr = AUTO_EP_OUT,                                                                 \
        }, {                                                                                        \
            .ep_cb = usb_transfer_ep_callback,                                                      \
            .ep_addr = AUTO_EP_IN,                                                                  \
        },                                                                                          \
    };                                                                                              \
                                                                                                    \
    USBD_DEFINE_CFG_DATA(config_name) = {                                                           \
        .usb_device_description = NULL,                                                             \
        .interface_config = vcp_usb_interface_config,                                               \
        .interface_descriptor = &vcp_usb_descriptor_##idx.if0,                                      \
        .cb_usb_status = vcp_usb_status_cb,                                                         \
        .interface = {                                                                              \
            .class_handler = vcp_usb_class_handle_req,                                              \
            .custom_handler = NULL,                                                                 \
        },                                                                                          \
        .num_endpoints = ARRAY_SIZE(vcp_usb_ep_data_##idx),                                         \
        .endpoint = vcp_usb_ep_data_##idx,                                                          \
    };

#endif /* __USB_PRIV_H__ */
