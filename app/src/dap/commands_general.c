#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "nvs.h"
#include "util.h"
#include "vcp/vcp.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

/* supported version of the DAP protocol */
#define DAP_PROTOCOL_VERSION    "2.1.1"

void swo_capture_control(const struct device *dev, bool enable);

int32_t dap_handle_command_info(const struct device *dev) {
    struct dap_data *data = dev->data;

    /* info subcommands */
    const uint8_t info_vendor_name = 0x01;
    const uint8_t info_product_name = 0x02;
    const uint8_t info_serial_number = 0x03;
    const uint8_t info_dap_protocol_version = 0x04;
    const uint8_t info_target_device_vendor = 0x05;
    const uint8_t info_target_device_name = 0x06;
    const uint8_t info_target_board_vendor = 0x07;
    const uint8_t info_target_board_name = 0x08;
    const uint8_t info_product_firmware_version = 0x09;
    const uint8_t info_capabilities = 0xf0;
    const uint8_t info_test_domain_timer = 0xf1;
    const uint8_t info_uart_rx_buffer_size = 0xfb;
    const uint8_t info_uart_tx_buffer_size = 0xfc;
    const uint8_t info_swo_buffer_size = 0xfd;
    const uint8_t info_max_packet_count = 0xfe;
    const uint8_t info_max_packet_size = 0xff;
    /* capabilities byte 0 */
    const uint8_t caps_support_swd = 0x01;
    const uint8_t caps_support_jtag = 0x02;
    const uint8_t caps_no_swo_uart_support = 0x00;
    const uint8_t caps_no_swo_manchester_support = 0x00;
    const uint8_t caps_support_atomic_cmds = 0x10;
    const uint8_t caps_no_test_domain_timer_support = 0x00;
    const uint8_t caps_no_swo_trace_support = 0x00;
    const uint8_t caps_no_uart_dap_port_support = 0x00;
    /* we don't support the second capabilities byte, as certain debug software
     * doesn't properly support it and causes probe initialization failures */

    uint8_t id = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &id, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_put(&data->buf.response, &((uint8_t) {DAP_COMMAND_INFO}), 1), 1, -ENOBUFS);

    uint8_t *ptr;
    if (id == info_vendor_name) {
        const char *vendor_name = CONFIG_USB_DEVICE_MANUFACTURER;
        const uint8_t vendor_name_len = sizeof(CONFIG_USB_DEVICE_MANUFACTURER);
        CHECK_EQ(ring_buf_put(&data->buf.response, &vendor_name_len, 1), 1, -ENOBUFS);
        uint32_t space = ring_buf_put_claim(&data->buf.response, &ptr, vendor_name_len);
        CHECK_EQ(space, vendor_name_len, -ENOBUFS);
        strncpy(ptr, vendor_name, space);
        CHECK_EQ(ring_buf_put_finish(&data->buf.response, space), 0, -ENOBUFS);
    } else if (id == info_product_name) {
        const char *product_name = CONFIG_USB_DEVICE_PRODUCT;
        const uint8_t product_name_len = sizeof(CONFIG_USB_DEVICE_PRODUCT);
        CHECK_EQ(ring_buf_put(&data->buf.response, &product_name_len, 1), 1, -ENOBUFS);
        uint32_t space = ring_buf_put_claim(&data->buf.response, &ptr, product_name_len);
        CHECK_EQ(space, product_name_len, -ENOBUFS);
        strncpy(ptr, product_name, space);
        CHECK_EQ(ring_buf_put_finish(&data->buf.response, space), 0, -ENOBUFS);
    } else if (id == info_serial_number) {
        char serial[sizeof(CONFIG_USB_DEVICE_SN)];
        CHECK_EQ(nvs_get_serial_number(serial, sizeof(serial)), 0, -EINVAL);
        const uint8_t serial_len = sizeof(CONFIG_USB_DEVICE_SN);
        CHECK_EQ(ring_buf_put(&data->buf.response, &serial_len, 1), 1, -ENOBUFS);
        uint32_t space = ring_buf_put_claim(&data->buf.response, &ptr, serial_len);
        CHECK_EQ(space, serial_len, -ENOBUFS);
        strncpy(ptr, serial, space);
        CHECK_EQ(ring_buf_put_finish(&data->buf.response, space), 0, -ENOBUFS);
    } else if (id == info_dap_protocol_version) {
        const char *protocol_version = DAP_PROTOCOL_VERSION;
        const uint8_t protocol_version_len = sizeof(DAP_PROTOCOL_VERSION);
        CHECK_EQ(ring_buf_put(&data->buf.response, &protocol_version_len, 1), 1, -ENOBUFS);
        uint32_t space = ring_buf_put_claim(&data->buf.response, &ptr, protocol_version_len);
        CHECK_EQ(space, protocol_version_len, -ENOBUFS);
        strncpy(ptr, protocol_version, space);
        CHECK_EQ(ring_buf_put_finish(&data->buf.response, space), 0, -ENOBUFS);
    } else if (id == info_target_device_vendor ||
               id == info_target_device_name ||
               id == info_target_board_vendor ||
               id == info_target_board_name) {
        /* not an on-board debug unit, just return no string */
        const uint8_t response = 0;
        CHECK_EQ(ring_buf_put(&data->buf.response, &response, 1), 1, -ENOBUFS);
    } else if (id == info_product_firmware_version) {
        const char *firmware_version = CONFIG_REPO_VERSION_STRING;
        const uint8_t firmware_version_len = sizeof(CONFIG_REPO_VERSION_STRING);
        CHECK_EQ(ring_buf_put(&data->buf.response, &firmware_version_len, 1), 1, -ENOBUFS);
        uint32_t space = ring_buf_put_claim(&data->buf.response, &ptr, firmware_version_len);
        CHECK_EQ(space, firmware_version_len, -ENOBUFS);
        strncpy(ptr, firmware_version, space);
        CHECK_EQ(ring_buf_put_finish(&data->buf.response, space), 0, -ENOBUFS);
    } else if (id == info_capabilities) {
        const uint8_t capabilities_len = 1;
        const uint8_t capabilities_info0 = caps_support_swd |
                                           caps_support_jtag |
                                           caps_no_swo_uart_support |
                                           caps_no_swo_manchester_support |
                                           caps_support_atomic_cmds |
                                           caps_no_test_domain_timer_support |
                                           caps_no_swo_trace_support |
                                           caps_no_uart_dap_port_support;
        const uint8_t response[2] = { capabilities_len, capabilities_info0};
        CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    } else if (id == info_test_domain_timer) {
        /* not supported by this debug unit, just return a reasonable default */
        const uint8_t response[5] = { 0x08, 0x00, 0x00, 0x00, 0x00 };
        CHECK_EQ(ring_buf_put(&data->buf.response, response, 5), 5, -ENOBUFS);
    } else if (id == info_uart_rx_buffer_size ||
               id == info_uart_tx_buffer_size) {
        const uint32_t rx_buf_size = VCP_RING_BUF_SIZE;
        uint8_t response[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        bytecpy(response + 1, &rx_buf_size, 4);
        CHECK_EQ(ring_buf_put(&data->buf.response, response, 5), 5, -ENOBUFS);
    } else if (id == info_swo_buffer_size) {
        const uint32_t swo_buffer_size = DAP_SWO_RING_BUF_SIZE;
        uint8_t response[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        bytecpy(response + 1, &swo_buffer_size, 4);
        CHECK_EQ(ring_buf_put(&data->buf.response, response, 5), 5, -ENOBUFS);
    } else if (id == info_max_packet_count) {
        const uint8_t max_packets = (uint8_t) (DAP_RING_BUF_SIZE / DAP_MAX_PACKET_SIZE);
        uint8_t response[2] = { 0x01, max_packets };
        CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    } else if (id == info_max_packet_size) {
        const uint16_t packet_size = DAP_MAX_PACKET_SIZE;
        uint8_t response[3] = { 0x02, 0x00, 0x00 };
        bytecpy(response + 1, &packet_size, 2);
        CHECK_EQ(ring_buf_put(&data->buf.response, response, 3), 3, -ENOBUFS);
    } else {
        /* unsupported info responses just have a length of 0 */
        CHECK_EQ(ring_buf_put(&data->buf.response, &((uint8_t) {0}), 1), 1, -ENOBUFS);
    }

    return 0;
}

int32_t dap_handle_command_host_status(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint8_t type = 0, status = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &type, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&data->buf.request, &status, 1), 1, -EMSGSIZE);

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
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_connect(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint8_t port = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &port, 1), 1, -EMSGSIZE);

    /* signifies a failed port initialization */
    uint8_t response_port = 0;
    if (gpio_pin_get_dt(&config->vtref_gpio) != 1) {
        LOG_ERR("cannot configure dap port with no target voltage");
        goto end;
    }

    if (port == 1) {
        /* tdo is configured as uart but capture not enabled */
        swo_capture_control(dev, false);
        if (pinctrl_apply_state(config->pinctrl_config, PINCTRL_STATE_SWO) != 0) { goto end; }
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
        /* tdi unused in swd mode */
        FATAL_CHECK(gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT) >= 0, "tdi config failed");
        data->swj.port = DAP_PORT_SWD;
        response_port = 1;
    } else {
        /* tdo pinctrl must be configured as gpio, and uart capture disabled */
        swo_capture_control(dev, false);
        if (pinctrl_apply_state(config->pinctrl_config, PINCTRL_STATE_TDO) != 0) { goto end; }
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
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_disconnect(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* disable SWO if it is currently enabled */
    swo_capture_control(dev, false);
    if (pinctrl_apply_state(config->pinctrl_config, PINCTRL_STATE_TDO) != 0) {
        status = DAP_COMMAND_RESPONSE_ERROR;
    }

    data->swj.port = DAP_PORT_DISABLED;
    FATAL_CHECK(gpio_pin_configure_dt(&config->tck_swclk_gpio, GPIO_INPUT) >= 0, "tck swclk config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0, "tms swdio config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdo_gpio, GPIO_INPUT) >= 0, "tdo config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT) >= 0, "tdi config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->nreset_gpio, GPIO_INPUT) >= 0, "nreset config failed");
    LOG_INF("configured port io as HiZ");

    uint8_t response[] = {DAP_COMMAND_DISCONNECT, status};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_delay(const struct device *dev) {
    struct dap_data *data = dev->data;

    uint16_t delay_us = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &delay_us, 2), 2, -EMSGSIZE);
    k_busy_wait(delay_us);

    uint8_t response[] = {DAP_COMMAND_DELAY, DAP_COMMAND_RESPONSE_OK};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_reset_target(const struct device *dev) {
    struct dap_data *data = dev->data;

    /* device specific target reset sequence is not implemented for this debug unit */
    uint8_t response[] = {DAP_COMMAND_RESET_TARGET, DAP_COMMAND_RESPONSE_OK, 0x00};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 3), 3, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_swj_pins(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* command pin bitfields */
    const uint8_t pin_swclk_tck_shift = 0;
    const uint8_t pin_swdio_tms_shift = 1;
    const uint8_t pin_tdi_shift = 2;
    const uint8_t pin_tdo_shift = 3;
    const uint8_t pin_nreset_shift = 7;

    uint8_t pin_output = 0;
    uint8_t pin_mask = 0;
    uint32_t delay_us = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &pin_output, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&data->buf.request, &pin_mask, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &delay_us, 4), 4, -EMSGSIZE);

    if ((pin_mask & BIT(pin_swclk_tck_shift)) != 0) {
        gpio_pin_set_dt(&config->tck_swclk_gpio, (pin_output & BIT(pin_swclk_tck_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_swdio_tms_shift)) != 0) {
        gpio_pin_set_dt(&config->tms_swdio_gpio, (pin_output & BIT(pin_swdio_tms_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_tdi_shift)) != 0) {
        gpio_pin_set_dt(&config->tdi_gpio, (pin_output & BIT(pin_tdi_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_tdo_shift)) != 0) {
        gpio_pin_set_dt(&config->tdo_gpio, (pin_output & BIT(pin_tdo_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_nreset_shift)) != 0) {
        gpio_pin_set_dt(&config->nreset_gpio, (pin_output & BIT(pin_nreset_shift)) == 0 ? 0 : 1);
    }
    /* ignore nTRST, this debug probe doesn't support it */

    /* maximum wait time allowed by command */
    if (delay_us > 3000000) {
        delay_us = 3000000;
    }
    /* all pins expect nreset are push-pull, don't wait on those */
    if ((delay_us > 0) && (pin_mask & BIT(pin_nreset_shift))) {
        uint64_t delay_end = sys_clock_timeout_end_calc(K_USEC(delay_us));
        do {
            uint32_t nreset_val = gpio_pin_get_dt(&config->nreset_gpio);
            if ((pin_output & BIT(pin_nreset_shift)) ^ (nreset_val << pin_nreset_shift)) {
                k_busy_wait(1);
            } else {
                break;
            }
        } while (delay_end > k_uptime_ticks());
    }

    uint8_t pin_input =
        (gpio_pin_get_dt(&config->tck_swclk_gpio) << pin_swclk_tck_shift) |
        (gpio_pin_get_dt(&config->tms_swdio_gpio) << pin_swdio_tms_shift) |
        (gpio_pin_get_dt(&config->tdi_gpio) << pin_tdi_shift) |
        (gpio_pin_get_dt(&config->tdo_gpio) << pin_tdo_shift) |
        (gpio_pin_get_dt(&config->nreset_gpio) << pin_nreset_shift);
    uint8_t response[] = {DAP_COMMAND_SWJ_PINS, pin_input};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_swj_clock(const struct device *dev) {
    struct dap_data *data = dev->data;

    uint32_t clock = DAP_DEFAULT_SWJ_CLOCK_RATE;
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &clock, 4), 4, -EMSGSIZE);
    if (clock != 0) {
        data->swj.clock = clock;
        data->swj.delay_ns = 1000000000 / clock / 2;
    }

    uint8_t status = clock == 0 ? DAP_COMMAND_RESPONSE_ERROR : DAP_COMMAND_RESPONSE_OK;
    uint8_t response[] = {DAP_COMMAND_SWJ_CLOCK, status};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

int32_t dap_handle_command_swj_sequence(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint16_t count = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &count, 1), 1, -EMSGSIZE);
    if (count == 0) {
        count = 256;
    }

    uint8_t tms_swdio_bits = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (i % 8 == 0) {
            CHECK_EQ(ring_buf_get(&data->buf.request, &tms_swdio_bits, 1), 1, -EMSGSIZE);
        }
        gpio_pin_set_dt(&config->tms_swdio_gpio, tms_swdio_bits & 0x01);
        gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
        busy_wait_nanos(data->swj.delay_ns);
        gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
        busy_wait_nanos(data->swj.delay_ns);
        tms_swdio_bits >>= 1;
    }

    uint8_t response[] = {DAP_COMMAND_SWJ_SEQUENCE, DAP_COMMAND_RESPONSE_OK};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}
