#include <drivers/hwinfo.h>
#include <logging/log.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"

LOG_MODULE_DECLARE(dap);

enum info_command_id {
    INFO_COMMAND_VENDOR_NAME = 0x01,
    INFO_COMMAND_PRODUCT_NAME = 0x02,
    INFO_COMMAND_SERIAL_NUMBER = 0x03,
    INFO_COMMAND_DAP_PROTOCOL_VERSION = 0x04,
    INFO_COMMAND_TARGET_DEVICE_VENDOR = 0x05,
    INFO_COMMAND_TARGET_DEVICE_NAME = 0x06,
    INFO_COMMAND_TARGET_BOARD_VENDOR = 0x07,
    INFO_COMMAND_TARGET_BOARD_NAME = 0x08,
    INFO_COMMAND_PRODUCT_FIRMWARE_VERSION = 0x09,
    INFO_COMMAND_CAPABILITIES = 0xf0,
    INFO_COMMAND_TEST_DOMAIN_TIMER = 0xf1,
    INFO_COMMAND_UART_RX_BUFFER_SIZE = 0xfb,
    INFO_COMMAND_UART_TX_BUFFER_SIZE = 0xfc,
    INFO_COMMAND_SWO_BUFFER_SIZE = 0xfd,
    INFO_COMMAND_MAX_PACKET_COUNT = 0xfe,
    INFO_COMMAND_MAX_PACKET_SIZE = 0xff,
};

int32_t dap_handle_command_info(const struct device *dev) {
    const struct dap_config *config = dev->config;

    uint8_t id = 0;
    ring_buf_get(config->request_buf, &id, 1);
    ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_INFO}), 1);

    uint8_t *ptr;
    if (id == INFO_COMMAND_VENDOR_NAME) {
        const char *vendor_name = CONFIG_USB_DEVICE_MANUFACTURER;
        const uint8_t vendor_name_len = sizeof(CONFIG_USB_DEVICE_MANUFACTURER);
        ring_buf_put(config->response_buf, &vendor_name_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, vendor_name_len);
        if (space != vendor_name_len) {
            return -ENOBUFS;
        }
        strncpy(ptr, vendor_name, vendor_name_len);
        if (ring_buf_put_finish(config->response_buf, vendor_name_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_PRODUCT_NAME) {
        const char *product_name = CONFIG_USB_DEVICE_PRODUCT;
        const uint8_t product_name_len = sizeof(CONFIG_USB_DEVICE_PRODUCT);
        ring_buf_put(config->response_buf, &product_name_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, product_name_len);
        if (space != product_name_len) {
            return -ENOBUFS;
        }
        strncpy(ptr, product_name, product_name_len);
        if (ring_buf_put_finish(config->response_buf, product_name_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_SERIAL_NUMBER) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_DAP_PROTOCOL_VERSION) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_TARGET_DEVICE_VENDOR) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_TARGET_DEVICE_NAME) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_TARGET_BOARD_VENDOR) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_TARGET_BOARD_NAME) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_PRODUCT_FIRMWARE_VERSION) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_CAPABILITIES) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_TEST_DOMAIN_TIMER) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_UART_RX_BUFFER_SIZE) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_UART_TX_BUFFER_SIZE) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_SWO_BUFFER_SIZE) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_MAX_PACKET_COUNT) {
        return -ENOTSUP;
    } else if (id == INFO_COMMAND_MAX_PACKET_SIZE) {
        return -ENOTSUP;
    } else {
        return -ENOTSUP;
    }
}