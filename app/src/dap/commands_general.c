#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "dap/usb.h"
#include "nvs.h"
#include "util.h"
#include "vcp/vcp.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

#define INFO_COMMAND_VENDOR_NAME                ((uint8_t) 0x01)
#define INFO_COMMAND_PRODUCT_NAME               ((uint8_t) 0x02)
#define INFO_COMMAND_SERIAL_NUMBER              ((uint8_t) 0x03)
#define INFO_COMMAND_DAP_PROTOCOL_VERSION       ((uint8_t) 0x04)
#define INFO_COMMAND_TARGET_DEVICE_VENDOR       ((uint8_t) 0x05)
#define INFO_COMMAND_TARGET_DEVICE_NAME         ((uint8_t) 0x06)
#define INFO_COMMAND_TARGET_BOARD_VENDOR        ((uint8_t) 0x07)
#define INFO_COMMAND_TARGET_BOARD_NAME          ((uint8_t) 0x08)
#define INFO_COMMAND_PRODUCT_FIRMWARE_VERSION   ((uint8_t) 0x09)
#define INFO_COMMAND_CAPABILITIES               ((uint8_t) 0xf0)
#define INFO_COMMAND_TEST_DOMAIN_TIMER          ((uint8_t) 0xf1)
#define INFO_COMMAND_UART_RX_BUFFER_SIZE        ((uint8_t) 0xfb)
#define INFO_COMMAND_UART_TX_BUFFER_SIZE        ((uint8_t) 0xfc)
#define INFO_COMMAND_SWO_BUFFER_SIZE            ((uint8_t) 0xfd)
#define INFO_COMMAND_MAX_PACKET_COUNT           ((uint8_t) 0xfe)
#define INFO_COMMAND_MAX_PACKET_SIZE            ((uint8_t) 0xff)

/* capabilities byte 0 */
#define CAPS_NO_SWD_SUPPORT                     ((uint8_t) 0x00)
#define CAPS_NO_JTAG_SUPPORT                    ((uint8_t) 0x00)
#define CAPS_NO_SWO_UART_SUPPORT                ((uint8_t) 0x00)
#define CAPS_NO_SWO_MANCHESTER_SUPPORT          ((uint8_t) 0x00)
#define CAPS_NO_ATOMIC_CMDS_SUPPORT             ((uint8_t) 0x00)
#define CAPS_NO_TEST_DOMAIN_TIMER_SUPPORT       ((uint8_t) 0x00)
#define CAPS_NO_SWO_TRACE_SUPPORT               ((uint8_t) 0x00)
#define CAPS_NO_UART_DAP_PORT_SUPPORT           ((uint8_t) 0x00)
/* capabilities byte 1 */
#define CAPS_SUPPORT_UART_VCP                   ((uint8_t) 0x01)

int32_t dap_handle_command_info(const struct device *dev) {
    const struct dap_config *config = dev->config;

    if (ring_buf_size_get(config->request_buf) < 1) { return -EMSGSIZE; }

    uint8_t id = 0;
    ring_buf_get(config->request_buf, &id, 1);
    ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_INFO}), 1);

    uint8_t *ptr;
    if (id == INFO_COMMAND_VENDOR_NAME) {
        const char *vendor_name = CONFIG_USB_DEVICE_MANUFACTURER;
        const uint8_t vendor_name_len = sizeof(CONFIG_USB_DEVICE_MANUFACTURER);
        ring_buf_put(config->response_buf, &vendor_name_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, vendor_name_len);
        strncpy(ptr, vendor_name, MIN(vendor_name_len, space));
        if (space != vendor_name_len || ring_buf_put_finish(config->response_buf, vendor_name_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_PRODUCT_NAME) {
        const char *product_name = CONFIG_USB_DEVICE_PRODUCT;
        const uint8_t product_name_len = sizeof(CONFIG_USB_DEVICE_PRODUCT);
        ring_buf_put(config->response_buf, &product_name_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, product_name_len);
        strncpy(ptr, product_name, MIN(product_name_len, space));
        if (space != product_name_len || ring_buf_put_finish(config->response_buf, product_name_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_SERIAL_NUMBER) {
        char serial[sizeof(CONFIG_USB_DEVICE_SN)];
        if (nvs_get_serial_number(serial, sizeof(serial)) < 0) {
            return -ENOBUFS;
        }
        const uint8_t serial_len = sizeof(CONFIG_USB_DEVICE_SN);
        ring_buf_put(config->response_buf, &serial_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, serial_len);
        strncpy(ptr, serial, MIN(serial_len, space));
        if (space != serial_len || ring_buf_put_finish(config->response_buf, serial_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_DAP_PROTOCOL_VERSION) {
        const char *protocol_version = DAP_PROTOCOL_VERSION;
        const uint8_t protocol_version_len = sizeof(DAP_PROTOCOL_VERSION);
        ring_buf_put(config->response_buf, &protocol_version_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, protocol_version_len);
        strncpy(ptr, protocol_version, MIN(protocol_version_len, space));
        if (space != protocol_version_len || ring_buf_put_finish(config->response_buf, protocol_version_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_TARGET_DEVICE_VENDOR ||
               id == INFO_COMMAND_TARGET_DEVICE_NAME ||
               id == INFO_COMMAND_TARGET_BOARD_VENDOR ||
               id == INFO_COMMAND_TARGET_BOARD_NAME) {
        /* not an on-board debug unit, just return no string */
        const uint8_t response = 0;
        ring_buf_put(config->response_buf, &response, 1);
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_PRODUCT_FIRMWARE_VERSION) {
        const char *firmware_version = CONFIG_REPO_VERSION_STRING;
        const uint8_t firmware_version_len = sizeof(CONFIG_REPO_VERSION_STRING);
        ring_buf_put(config->response_buf, &firmware_version_len, 1);
        uint32_t space = ring_buf_put_claim(config->response_buf, &ptr, firmware_version_len);
        strncpy(ptr, firmware_version, MIN(firmware_version_len, space));
        if (space != firmware_version_len || ring_buf_put_finish(config->response_buf, firmware_version_len) < 0) {
            return -ENOBUFS;
        }
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_CAPABILITIES) {
        /* TODO: this all needs to be changed as we support new capabilities */
        const uint8_t capabilities_len = 2;
        const uint8_t capabilities_info0 = CAPS_NO_SWD_SUPPORT |
                                           CAPS_NO_JTAG_SUPPORT |
                                           CAPS_NO_SWO_UART_SUPPORT |
                                           CAPS_NO_SWO_MANCHESTER_SUPPORT |
                                           CAPS_NO_ATOMIC_CMDS_SUPPORT |
                                           CAPS_NO_TEST_DOMAIN_TIMER_SUPPORT |
                                           CAPS_NO_SWO_TRACE_SUPPORT |
                                           CAPS_NO_UART_DAP_PORT_SUPPORT;
        const uint8_t capabilities_info1 = CAPS_SUPPORT_UART_VCP;
        const uint8_t response[3] = { capabilities_len, capabilities_info0, capabilities_info1 };
        ring_buf_put(config->response_buf, response, 3);
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_TEST_DOMAIN_TIMER) {
        /* not supported by this debug unit, just return a reasonable default */
        const uint8_t response[5] = { 0x08, 0x00, 0x00, 0x00, 0x00 };
        ring_buf_put(config->response_buf, response, 5);
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_UART_RX_BUFFER_SIZE ||
               id == INFO_COMMAND_UART_TX_BUFFER_SIZE) {
        const uint32_t rx_buf_size = sys_cpu_to_le32(VCP_RING_BUF_SIZE);
        uint8_t response[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        bytecpy(response + 1, &rx_buf_size, sizeof(rx_buf_size));
        ring_buf_put(config->response_buf, response, 5);
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_SWO_BUFFER_SIZE) {
        /* TODO: change this when functionality is supported */
        const uint8_t response[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        ring_buf_put(config->response_buf, response, 5);
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_MAX_PACKET_COUNT) {
        const uint8_t max_packets = (uint8_t) (DAP_RING_BUF_SIZE / DAP_BULK_EP_MPS);
        uint8_t response[2] = { 0x01, max_packets };
        ring_buf_put(config->response_buf, response, 2);
        return ring_buf_size_get(config->response_buf);
    } else if (id == INFO_COMMAND_MAX_PACKET_SIZE) {
        const uint16_t packet_size = sys_cpu_to_le16(DAP_BULK_EP_MPS);
        uint8_t response[3] = { 0x02, 0x00, 0x00 };
        bytecpy(response + 1, &packet_size, sizeof(packet_size));
        ring_buf_put(config->response_buf, response, 3);
        return ring_buf_size_get(config->response_buf);
    } else {
        return -ENOTSUP;
    }
}

int32_t dap_handle_command_host_status(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    if (ring_buf_size_get(config->request_buf) < 2) { return -EMSGSIZE; }

    uint8_t type = 0, status = 0;
    ring_buf_get(config->request_buf, &type, 1);
    ring_buf_get(config->request_buf, &status, 1);

    uint8_t response_status = DAP_COMMAND_RESPONSE_OK;
    if (type > 1 || status > 1) {
        response_status = DAP_COMMAND_RESPONSE_ERROR;
    } else if (type == 0) {
        /* 'connect' status, but use the running led if combined */
        const struct gpio_dt_spec *led_gpio = data->led.combined ?
            &config->led_running_gpio : &config->led_connect_gpio;
        if (status == 0) {
            data->led.connected = false;
            gpio_pin_set_dt(led_gpio, 0);
        } else {
            data->led.connected = true;
            gpio_pin_set_dt(led_gpio, 1);
        }
    } else {
        /* 'running' status */
        if (status == 0) {
            data->led.running = false;
            k_timer_stop(&data->led.timer);
            /* if combined and still connected, make sure to leave led enabled */
            if (data->led.combined && data->led.connected) {
                gpio_pin_set_dt(&config->led_running_gpio, 1);
            } else {
                gpio_pin_set_dt(&config->led_running_gpio, 0);
            }
        } else {
            data->led.running = true;
            k_timer_start(&data->led.timer, K_NO_WAIT, K_MSEC(500));
        }
    }

    uint8_t response[] = {DAP_COMMAND_HOST_STATUS, response_status};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_connect(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    if (ring_buf_size_get(config->request_buf) < 1) { return -EMSGSIZE; }

    uint8_t port = 0;
    ring_buf_get(config->request_buf, &port, 1);

    /* signifies a failed port initialization */
    uint8_t response_port = 0;
    if (gpio_pin_get_dt(&config->vtref_gpio) != 1) {
        LOG_ERR("cannot configure dap port with no target voltage");
        goto end;
    }

    if (port == 1) {
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tck_swclk_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "tck swclk config failed"
        );
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "tms swdio config failed"
        );
        /* need to be able to read nreset while output to ensure pin stabilizes */
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->nreset_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "nreset config failed"
        );
        /* tdi and tdo unused in swd mode */
        FATAL_CHECK(gpio_pin_configure_dt(&config->tdo_gpio, GPIO_INPUT) >= 0, "tdo config failed");
        FATAL_CHECK(gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT) >= 0, "tdi config failed");
        data->swj.port = DAP_PORT_SWD;
        response_port = 1;
    } else {
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tck_swclk_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE | GPIO_PULL_DOWN) >= 0,
            "tck swclk config failed"
        );
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE | GPIO_PULL_UP) >= 0,
            "tms swdio config failed"
        );
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE | GPIO_PULL_UP) >= 0,
            "tdi config failed"
        );
        /* need to be able to read nreset while output to ensure pin stabilizes */
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->nreset_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "nreset config failed"
        );
        FATAL_CHECK(gpio_pin_configure_dt(&config->tdo_gpio, GPIO_INPUT | GPIO_PULL_UP) >= 0, "tdo config failed");
        data->swj.port = DAP_PORT_JTAG;
        response_port = 2;
    }
    LOG_INF("configured port io as %s", port == 1 ? "SWD" : "JTAG");

end: ;
    uint8_t response[] = {DAP_COMMAND_CONNECT, response_port};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_disconnect(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    data->swj.port = DAP_PORT_DISABLED;
    FATAL_CHECK(gpio_pin_configure_dt(&config->tck_swclk_gpio, GPIO_INPUT) >= 0, "tck swclk config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0, "tms swdio config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdo_gpio, GPIO_INPUT) >= 0, "tdo config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT) >= 0, "tdi config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->nreset_gpio, GPIO_INPUT) >= 0, "nreset config failed");
    LOG_INF("configured port io as HiZ");

    uint8_t response[] = {DAP_COMMAND_DISCONNECT, DAP_COMMAND_RESPONSE_OK};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_write_abort(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_delay(const struct device *dev) {
    const struct dap_config *config = dev->config;

    if (ring_buf_size_get(config->request_buf) < 1) { return -EMSGSIZE; }

    uint16_t delay_us = 0;
    ring_buf_get(config->request_buf, (uint8_t*) &delay_us, 2);
    k_busy_wait(sys_le16_to_cpu(delay_us));

    uint8_t response[] = {DAP_COMMAND_DELAY, DAP_COMMAND_RESPONSE_OK};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_reset_target(const struct device *dev) {
    const struct dap_config *config = dev->config;

    /* device specific target reset sequence is not implemented for this debug unit */
    uint8_t response[] = {DAP_COMMAND_RESET_TARGET, DAP_COMMAND_RESPONSE_OK, 0x00};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));
    return ring_buf_size_get(config->response_buf);
}
