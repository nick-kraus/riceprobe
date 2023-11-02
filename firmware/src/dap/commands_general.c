#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "nvs.h"
#include "util.h"
#include "vcp/vcp.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

int32_t dap_handle_cmd_info(struct dap_driver *dap) {
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
    const uint8_t caps_support_swo_uart = 0x04;
    const uint8_t caps_no_swo_manchester_support = 0x00;
    const uint8_t caps_support_atomic_cmds = 0x10;
    const uint8_t caps_no_test_domain_timer_support = 0x00;
    const uint8_t caps_support_swo_trace = 0x40;
    const uint8_t caps_no_uart_dap_port_support = 0x00;
    /* we don't support the second capabilities byte, as certain debug software
     * doesn't properly support it and causes probe initialization failures */

    /* supported version of the DAP protocol */
    const char dap_protocol_version[] = "2.1.1";

    uint8_t id = 0;
    if (ring_buf_get(&dap->buf.request, &id, 1) != 1) return -EMSGSIZE;
    if (ring_buf_put(&dap->buf.response, &dap_cmd_info, 1) != 1) return -ENOBUFS;

    uint8_t *ptr;
    uint32_t space;
    if (id == info_vendor_name) {
        const char *vendor_name = CONFIG_PRODUCT_MANUFACTURER;
        const uint8_t str_len = sizeof(CONFIG_PRODUCT_MANUFACTURER);
        if (ring_buf_put(&dap->buf.response, &str_len, 1) != 1) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&dap->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, vendor_name, space);
        if (ring_buf_put_finish(&dap->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_product_name) {
        const char *product_name = CONFIG_PRODUCT_DESCRIPTOR;
        const uint8_t str_len = sizeof(CONFIG_PRODUCT_DESCRIPTOR);
        if (ring_buf_put(&dap->buf.response, &str_len, 1) != 1) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&dap->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, product_name, space);
        if (ring_buf_put_finish(&dap->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_serial_number) {
        char serial[sizeof(CONFIG_PRODUCT_SERIAL_FORMAT)];
        if (nvs_get_serial_number(serial, sizeof(serial)) < 0) return -EINVAL;
        const uint8_t str_len = sizeof(CONFIG_PRODUCT_SERIAL_FORMAT);
        if (ring_buf_put(&dap->buf.response, &str_len, 1) != 1) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&dap->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, serial, space);
        if (ring_buf_put_finish(&dap->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_dap_protocol_version) {
        const uint8_t str_len = sizeof(dap_protocol_version);
        if (ring_buf_put(&dap->buf.response, &str_len, 1) != 1) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&dap->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, dap_protocol_version, space);
        if (ring_buf_put_finish(&dap->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_target_device_vendor ||
               id == info_target_device_name ||
               id == info_target_board_vendor ||
               id == info_target_board_name) {
        /* not an on-board debug unit, just return no string */
        const uint8_t response = 0;
        if (ring_buf_put(&dap->buf.response, &response, 1) != 1) return -ENOBUFS;
    } else if (id == info_product_firmware_version) {
        const char *firmware_version = CONFIG_REPO_VERSION_STRING;
        const uint8_t str_len = sizeof(CONFIG_REPO_VERSION_STRING);
        if (ring_buf_put(&dap->buf.response, &str_len, 1) != 1) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&dap->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, firmware_version, space);
        if (ring_buf_put_finish(&dap->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_capabilities) {
        const uint8_t capabilities_len = 1;
        const uint8_t capabilities_info0 = caps_support_swd |
                                           caps_support_jtag |
                                           caps_support_swo_uart |
                                           caps_no_swo_manchester_support |
                                           caps_support_atomic_cmds |
                                           caps_no_test_domain_timer_support |
                                           caps_support_swo_trace |
                                           caps_no_uart_dap_port_support;
        const uint8_t response[2] = { capabilities_len, capabilities_info0};
        if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    } else if (id == info_test_domain_timer) {
        /* not supported by this debug unit, just return a reasonable default */
        const uint8_t response[5] = { 0x08, 0x00, 0x00, 0x00, 0x00 };
        if (ring_buf_put(&dap->buf.response, response, 5) != 5) return -ENOBUFS;
    } else if (id == info_uart_rx_buffer_size ||
               id == info_uart_tx_buffer_size) {
        uint8_t response[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        sys_put_le32((VCP_RING_BUF_SIZE), &response[1]);
        if (ring_buf_put(&dap->buf.response, response, 5) != 5) return -ENOBUFS;
    } else if (id == info_swo_buffer_size) {
        uint8_t response[5] = { 0x04, 0x00, 0x00, 0x00, 0x00 };
        sys_put_le32((DAP_SWO_RING_BUF_SIZE), &response[1]);
        if (ring_buf_put(&dap->buf.response, response, 5) != 5) return -ENOBUFS;
    } else if (id == info_max_packet_count) {
        uint8_t response[2] = { 0x01, (uint8_t) (DAP_RING_BUF_SIZE / DAP_MAX_PACKET_SIZE) };
        if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    } else if (id == info_max_packet_size) {
        uint8_t response[3] = { 0x02, 0x00, 0x00 };
        sys_put_le16((DAP_MAX_PACKET_SIZE), &response[1]);
        if (ring_buf_put(&dap->buf.response, response, 3) != 3) return -ENOBUFS;
    } else {
        /* unsupported info responses just have a length of 0 */
        const uint8_t response = 0;
        if (ring_buf_put(&dap->buf.response, &response, 1) != 1) return -ENOBUFS;
    }

    return 0;
}

int32_t dap_handle_cmd_host_status(struct dap_driver *dap) {
    uint8_t type = 0, status = 0;
    if (ring_buf_get(&dap->buf.request, &type, 1) != 1) return -EMSGSIZE;
    if (ring_buf_get(&dap->buf.request, &status, 1) != 1) return -EMSGSIZE;

    uint8_t response_status = dap_cmd_response_ok;
    if (type > 1 || status > 1) {
        response_status = dap_cmd_response_error;
    } else if (type == 0) {
        /* 'connect' status, but use the running led if combined */
        const struct gpio_dt_spec *led = dap->led.combined ?
            &dap->io.led_running : &dap->io.led_connect;
        if (status == 0) {
            dap->led.connected = false;
            gpio_pin_set_dt(led, 0);
        } else {
            dap->led.connected = true;
            gpio_pin_set_dt(led, 1);
        }
    } else {
        /* 'running' status */
        if (status == 0) {
            dap->led.running = false;
            k_timer_stop(&dap->led.timer);
            /* if combined and still connected, make sure to leave led enabled */
            if (dap->led.combined && dap->led.connected) {
                gpio_pin_set_dt(&dap->io.led_running, 1);
            } else {
                gpio_pin_set_dt(&dap->io.led_running, 0);
            }
        } else {
            dap->led.running = true;
            k_timer_start(&dap->led.timer, K_NO_WAIT, K_MSEC(500));
        }
    }

    uint8_t response[] = {dap_cmd_host_status, response_status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_connect(struct dap_driver *dap) {
    uint8_t port = 0;
    if (ring_buf_get(&dap->buf.request, &port, 1) != 1) return -EMSGSIZE;

    /* signifies a failed port initialization */
    uint8_t response_port = 0;
    if (gpio_pin_get_dt(&dap->io.vtref) != 1) {
        LOG_ERR("cannot configure dap port with no target voltage");
        goto end;
    }

    if (port == 0 || port == 1) {
        /* tdo is configured as uart but capture not enabled */
        swo_capture_control(dap, false);
        if (dap_configure_pin(&dap->pinctrl.swd_state_pins) != 0) { goto end; }
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tck_swclk, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "tck swclk config failed"
        );
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "tms swdio config failed"
        );
        /* need to be able to read nreset while output to ensure pin stabilizes */
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.nreset, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "nreset config failed"
        );
        /* tdi unused in swd mode */
        FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tdi, GPIO_INPUT) >= 0, "tdi config failed");
        dap->swj.port = dap_port_swd;
        response_port = 1;
    } else if (port == 2) {
        /* tdo pinctrl must be configured as gpio, and uart capture disabled */
        swo_capture_control(dap, false);
        if (dap_configure_pin(&dap->pinctrl.jtag_state_pins) != 0) { goto end; }
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tck_swclk, GPIO_INPUT | GPIO_OUTPUT_ACTIVE | GPIO_PULL_DOWN) >= 0,
            "tck swclk config failed"
        );
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE | GPIO_PULL_UP) >= 0,
            "tms swdio config failed"
        );
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tdi, GPIO_INPUT | GPIO_OUTPUT_ACTIVE | GPIO_PULL_UP) >= 0,
            "tdi config failed"
        );
        /* need to be able to read nreset while output to ensure pin stabilizes */
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.nreset, GPIO_INPUT | GPIO_OUTPUT_ACTIVE) >= 0,
            "nreset config failed"
        );
        FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tdo, GPIO_INPUT | GPIO_PULL_UP) >= 0, "tdo config failed");
        dap->swj.port = dap_port_jtag;
        response_port = 2;
    } else {
        /* unsupported port, respond with failed initialization */
        goto end;
    }
    LOG_INF("configured port io as %s", port == 1 ? "SWD" : "JTAG");

end: ;
    uint8_t response[] = {dap_cmd_connect, response_port};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_disconnect(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    /* disable SWO if it is currently enabled */
    swo_capture_control(dap, false);
    if (dap_configure_pin(&dap->pinctrl.jtag_state_pins) != 0) { status = dap_cmd_response_error; }

    dap->swj.port = dap_port_disabled;
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tck_swclk, GPIO_INPUT) >= 0, "tck swclk config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT) >= 0, "tms swdio config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tdo, GPIO_INPUT) >= 0, "tdo config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tdi, GPIO_INPUT) >= 0, "tdi config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.nreset, GPIO_INPUT) >= 0, "nreset config failed");
    LOG_INF("configured port io as HiZ");

    uint8_t response[] = {dap_cmd_disconnect, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_delay(struct dap_driver *dap) {
    uint16_t delay_us = 0;
    if (ring_buf_get_le16(&dap->buf.request, &delay_us) < 0) return -EMSGSIZE;
    k_sleep(K_USEC(delay_us));

    uint8_t response[] = {dap_cmd_delay, dap_cmd_response_ok};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_reset_target(struct dap_driver *dap) {
    /* device specific target reset sequence is not implemented for this debug unit */
    uint8_t response[] = {dap_cmd_reset_target, dap_cmd_response_ok, 0x00};
    if (ring_buf_put(&dap->buf.response, response, 3) != 3) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_swj_pins(struct dap_driver *dap) {
    /* command pin bitfields */
    const uint8_t pin_swclk_tck_shift = 0;
    const uint8_t pin_swdio_tms_shift = 1;
    const uint8_t pin_tdi_shift = 2;
    const uint8_t pin_tdo_shift = 3;
    const uint8_t pin_nreset_shift = 7;

    uint8_t pin_output = 0;
    if (ring_buf_get(&dap->buf.request, &pin_output, 1) != 1) return -EMSGSIZE;
    uint8_t pin_mask = 0;
    if (ring_buf_get(&dap->buf.request, &pin_mask, 1) != 1) return -EMSGSIZE;
    uint32_t delay_us = 0;
    if (ring_buf_get_le32(&dap->buf.request, &delay_us) < 0) return -EMSGSIZE;

    if ((pin_mask & BIT(pin_swclk_tck_shift)) != 0) {
        gpio_pin_set_dt(&dap->io.tck_swclk, (pin_output & BIT(pin_swclk_tck_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_swdio_tms_shift)) != 0) {
        gpio_pin_set_dt(&dap->io.tms_swdio, (pin_output & BIT(pin_swdio_tms_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_tdi_shift)) != 0) {
        gpio_pin_set_dt(&dap->io.tdi, (pin_output & BIT(pin_tdi_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_tdo_shift)) != 0) {
        gpio_pin_set_dt(&dap->io.tdo, (pin_output & BIT(pin_tdo_shift)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(pin_nreset_shift)) != 0) {
        gpio_pin_set_dt(&dap->io.nreset, (pin_output & BIT(pin_nreset_shift)) == 0 ? 0 : 1);
    }
    /* ignore nTRST, this debug probe doesn't support it */

    /* maximum wait time allowed by command */
    if (delay_us > 3000000) {
        delay_us = 3000000;
    }
    /* all pins expect nreset are push-pull, don't wait on those */
    if ((delay_us > 0) && (pin_mask & BIT(pin_nreset_shift))) {
        k_timepoint_t delay_end_time = sys_timepoint_calc(K_USEC(delay_us));
        do {
            uint32_t nreset_val = gpio_pin_get_dt(&dap->io.nreset);
            if ((pin_output & BIT(pin_nreset_shift)) ^ (nreset_val << pin_nreset_shift)) {
                k_busy_wait(1);
            } else {
                break;
            }
        } while (!sys_timepoint_expired(delay_end_time));
    }

    uint8_t pin_input =
        (gpio_pin_get_dt(&dap->io.tck_swclk) << pin_swclk_tck_shift) |
        (gpio_pin_get_dt(&dap->io.tms_swdio) << pin_swdio_tms_shift) |
        (gpio_pin_get_dt(&dap->io.tdi) << pin_tdi_shift) |
        (gpio_pin_get_dt(&dap->io.tdo) << pin_tdo_shift) |
        (gpio_pin_get_dt(&dap->io.nreset) << pin_nreset_shift);
    uint8_t response[] = {dap_cmd_swj_pins, pin_input};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_swj_clock(struct dap_driver *dap) {
    uint32_t clock = dap_default_swj_clock_rate;
    if (ring_buf_get_le32(&dap->buf.request, &clock) < 0) return -EMSGSIZE;

    if (clock != 0) {
        dap->swj.clock = clock;
        dap->swj.delay_ns = 1000000000 / clock / 2;
    }

    uint8_t status = clock == 0 ? dap_cmd_response_error : dap_cmd_response_ok;
    uint8_t response[] = {dap_cmd_swj_clock, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_swj_sequence(struct dap_driver *dap) {
    uint16_t count = 0;
    if (ring_buf_get(&dap->buf.request, (uint8_t*) &count, 1) != 1) return -EMSGSIZE;
    if (count == 0) {
        count = 256;
    }

    uint8_t tms_swdio_bits = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (i % 8 == 0) {
            if (ring_buf_get(&dap->buf.request, &tms_swdio_bits, 1) != 1) return -EMSGSIZE;
        }
        gpio_pin_set_dt(&dap->io.tms_swdio, tms_swdio_bits & 0x01);
        gpio_pin_set_dt(&dap->io.tck_swclk, 0);
        busy_wait_nanos(dap->swj.delay_ns);
        gpio_pin_set_dt(&dap->io.tck_swclk, 1);
        busy_wait_nanos(dap->swj.delay_ns);
        tms_swdio_bits >>= 1;
    }

    uint8_t response[] = {dap_cmd_swj_sequence, dap_cmd_response_ok};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}
