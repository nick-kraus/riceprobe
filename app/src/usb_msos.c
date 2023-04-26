#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <usb_descriptor.h>

#include "nvs.h"

LOG_MODULE_REGISTER(usb, LOG_LEVEL_DBG);

/* The format of the MS OS descriptor is based on the definition in the Microsoft document titled
 * 'Microsoft OS Descriptors Overview'.
 *
 * These descriptors are read on first device initialization by Windows to automatically install
 * WinUSB driver support for this device (and all devices with matching USB VID and PID), so that
 * manually driver installation is not required by the end user.
 *
 * For testing purposes, the registry entry named 'oscv' located at
 * 'HLKM/SYSTEM/CurrentControlSet/Control/UsbFlags/vvvvpppprrrr' can be deleted to force Windows
 * to re-query the device for MS OS descriptor support. For more details please see
 * https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-device-specific-registry-settings */

#define USB_MSOS_VENDOR_CODE    0x20

struct usb_msos_string_descr {
    uint8_t bLength;
	uint8_t bDescriptorType;
	uint8_t bString[14];
	uint16_t wVendor;
} __packed usb_msos_string_descr = {
	.bLength = sizeof(struct usb_msos_string_descr),
	.bDescriptorType = USB_DESC_STRING,
	.bString = {
		'M', 0, 'S', 0, 'F', 0, 'T', 0,
		'1', 0, '0', 0, '0', 0,
	},
	.wVendor = USB_MSOS_VENDOR_CODE,
};

struct usb_msos_compatid_func {
    uint8_t bFirstInterfaceNumber;
	uint8_t bReserved;
	uint8_t compatibleID[8];
	uint8_t subCompatibleID[8];
	uint8_t reserved[6];
} __packed;

struct usb_msos_compatid_descr {
    uint32_t dwLength;
	uint16_t bcdVersion;
	uint16_t wIndex;
	uint8_t bCount;
	uint8_t bReserved[7];
    struct usb_msos_compatid_func func0;
    struct usb_msos_compatid_func func1;
} __packed usb_msos_compatid_descr = {
    .dwLength = sizeof(struct usb_msos_compatid_descr),
	.bcdVersion = 0x100,
	.wIndex = 4,
	.bCount = 2,
	.bReserved = {0, 0, 0, 0, 0, 0, 0},
    .func0 = {
        .bFirstInterfaceNumber = 0,
        .bReserved = 1,
        .compatibleID = {'W', 'I', 'N', 'U', 'S', 'B', 0, 0},
        .subCompatibleID = {0, 0, 0, 0, 0, 0, 0, 0},
        .reserved = {0, 0, 0, 0, 0, 0},
    },
    .func1 = {
        .bFirstInterfaceNumber = 0,
        .bReserved = 1,
        .compatibleID = {'W', 'I', 'N', 'U', 'S', 'B', 0, 0},
        .subCompatibleID = {0, 0, 0, 0, 0, 0, 0, 0},
        .reserved = {0, 0, 0, 0, 0, 0},
    }
};

void usb_msos_set_func0_interface(uint8_t intf) {
    usb_msos_compatid_descr.func0.bFirstInterfaceNumber = intf;
}

void usb_msos_set_func1_interface(uint8_t intf) {
    usb_msos_compatid_descr.func1.bFirstInterfaceNumber = intf;
}

struct usb_msos_device_intf_guid {
    uint32_t dwSize;
    uint32_t dwPropertyDataType;
    uint16_t wPropertyNameLength;
    uint8_t bPropertyName[40];
    uint32_t dwPropertyDataLength;
    uint8_t bPropertyData[78];
} __packed;

struct usb_msos_extprop_descr {
    uint32_t dwLength;
	uint16_t bcdVersion;
	uint16_t wIndex;
	uint16_t wCount;
    struct usb_msos_device_intf_guid func;
} __packed usb_msos_extprop_descr = {
    .dwLength = sizeof(struct usb_msos_extprop_descr),
	.bcdVersion = 0x100,
	.wIndex = 5,
	.wCount = 1,
    .func = {
        .dwSize = sizeof(struct usb_msos_device_intf_guid),
        .dwPropertyDataType = 1,
        .wPropertyNameLength = 40,
        .bPropertyName = {
            'D', 0, 'e', 0, 'v', 0, 'i', 0, 'c', 0, 'e', 0, 'I', 0, 'n', 0,
            't', 0, 'e', 0, 'r', 0, 'f', 0, 'a', 0, 'c', 0, 'e', 0, 'G', 0,
            'U', 0, 'I', 0, 'D', 0, 0, 0
        },
        .dwPropertyDataLength = 78,
        .bPropertyData = {
            '{', 0, 'C', 0, 'D', 0, 'B', 0, '3', 0, 'B', 0, '5', 0, 'A', 0,
            'D', 0, '-', 0, '2', 0, '9', 0, '3', 0, 'B', 0, '-', 0, '4', 0,
            '6', 0, '6', 0, '3', 0, '-', 0, 'A', 0, 'A', 0, '3', 0, '6', 0,
            '-', 0, '1', 0, 'A', 0, 'A', 0, 'E', 0, '4', 0, '6', 0, '4', 0,
            '6', 0, '3', 0, '7', 0, '7', 0, '6', 0, '}', 0, 0, 0
        },
    }
};

int32_t usb_msos_custom_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data) {
	if (usb_reqtype_is_to_device(pSetup)) {
		return -ENOTSUP;
	}

	if (USB_GET_DESCRIPTOR_TYPE(pSetup->wValue) == USB_DESC_STRING &&
		USB_GET_DESCRIPTOR_INDEX(pSetup->wValue) == 0xEE) {

		LOG_DBG("retreiving MS OS v1 string descriptor");
		*data = (uint8_t*) &usb_msos_string_descr;
		*len = sizeof(usb_msos_string_descr);

		return 0;
	}

	return -EINVAL;
}

int32_t usb_msos_vendor_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data) {
	if (usb_reqtype_is_to_device(pSetup)) {
		return -ENOTSUP;
	}

	if (pSetup->bRequest == USB_MSOS_VENDOR_CODE && pSetup->wIndex == 0x04) {
		LOG_DBG("retreiving MS OS v1 compatible ID descriptor");
		*data = (uint8_t*) &usb_msos_compatid_descr;
		*len = sizeof(usb_msos_compatid_descr);

		return 0;
	} else if (pSetup->bRequest == USB_MSOS_VENDOR_CODE && pSetup->wIndex == 0x05) {
        LOG_DBG("retreiving MS OS v1 extended properties descriptor");
        *data = (uint8_t*) &usb_msos_extprop_descr;
        *len = sizeof(usb_msos_extprop_descr);

        return 0;
    }

	return -ENOTSUP;
}

uint8_t *usb_update_sn_string_descriptor(void) {
    static uint8_t serial[sizeof(CONFIG_USB_DEVICE_SN)];

    int32_t ret = nvs_get_serial_number(serial, sizeof(serial));
    if (ret < 0) {
        LOG_WRN("failed to update USB serial number with NVS value");
        serial[0] = '\0';
    }

    return serial;
}
