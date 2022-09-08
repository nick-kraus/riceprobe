#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

/**
 * @brief Returns the associated const struct device *dev from a given usb config.
 * 
 * @note In order for this to work, the driver device data must contain a copy of its own
 * device handle named 'dev' (data->dev) and the driver device config must contain a
 * reference to the usb config named 'usb_config' (config->usb_config);
 */
#define DRIVER_DEV_FROM_USB_CFG(driver_data, driver_config, usb_cfg, usb_devlist)       \
    ({                                                                                  \
        const struct device *return_dev = NULL;                                         \
        driver_data *data;                                                              \
        SYS_SLIST_FOR_EACH_CONTAINER(&usb_devlist, data, devlist_node) {                \
            const struct device *dev = data->dev;                                       \
            const driver_config *config = dev->config;                                  \
            const struct usb_cfg_data *cur_usb_cfg = config->usb_config;                \
                                                                                        \
            if (cur_usb_cfg == usb_cfg) {                                               \
                return_dev = dev;                                                       \
            }                                                                           \
        }                                                                               \
        return_dev;                                                                     \
    })

/**
 * @brief Returns the associated const struct device *dev from a given usb interface number.
 * 
 * @note In order for this to work, the driver device data must contain a copy of its own
 * device handle named 'dev' (data->dev) and the driver device config must contain a
 * reference to the usb config named 'usb_config' (config->usb_config);
 */
#define DRIVER_DEV_FROM_USB_INTF(driver_data, driver_config, usb_intf, usb_devlist)     \
    ({                                                                                  \
        const struct device *return_dev = NULL;                                         \
        driver_data *data;                                                              \
        SYS_SLIST_FOR_EACH_CONTAINER(&usb_devlist, data, devlist_node) {                \
            const struct device *dev = data->dev;                                       \
            const driver_config *config = dev->config;                                  \
            const struct usb_cfg_data *cfg = config->usb_config;                        \
            const struct usb_if_descriptor *intf = cfg->interface_descriptor;           \
                                                                                        \
            if (intf->bInterfaceNumber == usb_intf) {                                   \
                return_dev = dev;                                                       \
            }                                                                           \
        }                                                                               \
        return_dev;                                                                     \
    })

void usb_winusb_set_func0_interface(uint8_t intf);
void usb_winusb_set_func1_interface(uint8_t intf);

int32_t usb_winusb_custom_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data);
int32_t usb_winusb_vendor_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data);

#endif /* __USB_H__ */
