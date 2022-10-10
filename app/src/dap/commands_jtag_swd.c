#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

/* statement statements may not work in non gcc or clang compilers */
#define JTAG_TDIO_CYCLE(_tdi) ({                        \
    gpio_pin_set_dt(&config->tdi_gpio, _tdi & 0x01);    \
    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);        \
    busy_wait_nanos(data->swj.delay_ns);                \
    uint8_t _tdo = gpio_pin_get_dt(&config->tdo_gpio);  \
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);        \
    busy_wait_nanos(data->swj.delay_ns);                \
    _tdo;                                               \
})

int32_t dap_handle_command_swj_pins(const struct device *dev) {
    const struct dap_config *config = dev->config;

    /* command pin bitfields */
    const uint8_t pin_swclk_tck_shift = 0;
    const uint8_t pin_swdio_tms_shift = 1;
    const uint8_t pin_tdi_shift = 2;
    const uint8_t pin_tdo_shift = 3;
    const uint8_t pin_nreset_shift = 7;

    if (ring_buf_size_get(config->request_buf) < 6) { return -EMSGSIZE; }

    uint8_t pin_output = 0;
    uint8_t pin_mask = 0;
    uint32_t delay_us = 0;
    ring_buf_get(config->request_buf, &pin_output, 1);
    ring_buf_get(config->request_buf, &pin_mask, 1);
    ring_buf_get(config->request_buf, (uint8_t*) &delay_us, 4);
    delay_us = sys_le32_to_cpu(delay_us);

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
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swj_clock(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    if (ring_buf_size_get(config->request_buf) < 4) { return -EMSGSIZE; }

    uint32_t clock = DAP_DEFAULT_SWJ_CLOCK_RATE;
    ring_buf_get(config->request_buf, (uint8_t*) &clock, 4);
    if (clock != 0) {
        data->swj.clock = sys_le32_to_cpu(clock);
        data->swj.delay_ns = 1000000000 / clock / 2;
    }

    uint8_t status = clock == 0 ? DAP_COMMAND_RESPONSE_ERROR : DAP_COMMAND_RESPONSE_OK;
    uint8_t response[] = {DAP_COMMAND_SWJ_CLOCK, status};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swj_sequence(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_jtag_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* minimum possible size for command, but actual length is variable */
    if (ring_buf_size_get(config->request_buf) < 2) { return -EMSGSIZE; }

    uint8_t count = 0;
    ring_buf_get(config->request_buf, &count, 1);
    if (count > DAP_JTAG_MAX_DEVICE_COUNT ||
        ring_buf_size_get(config->request_buf) < count) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    }

    data->jtag.count = count;
    for (int i = 0; i < data->jtag.count; i++) {
        uint8_t len = 0;
        ring_buf_get(config->request_buf, &len, 1);
        data->jtag.ir_length[i] = len;
    }

end: ;
    uint8_t response[] = {DAP_COMMAND_JTAG_CONFIGURE, status};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_jtag_sequence(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* command info bitfield */
    const uint8_t info_tck_cycles_mask = 0x3f;
    const uint8_t info_tdo_capture_mask = 0x80;
    const uint8_t info_tms_value_shift = 6;

    /* minimum possible size for command, but actual length is variable */
    if (ring_buf_size_get(config->request_buf) < 3) { return -EMSGSIZE; }

    ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_JTAG_SEQUENCE}), 1);
    /* need a pointer to this item because we will write to it after trying the rest of the command */
    uint8_t *command_status = NULL;
    ring_buf_put_claim(config->response_buf, &command_status, 1);
    *command_status = 0;
    ring_buf_put_finish(config->response_buf, 1);

    if (data->swj.port != DAP_PORT_JTAG) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    }

    uint8_t seq_count = 0;
    ring_buf_get(config->request_buf, &seq_count, 1);
    for (int i = 0; i < seq_count; i++) {
        uint8_t info = 0;
        ring_buf_get(config->request_buf, &info, 1);

        uint8_t tck_cycles = info & info_tck_cycles_mask;
        if (tck_cycles == 0) {
            tck_cycles = 64;
        }

        uint8_t tms_val = (info & BIT(info_tms_value_shift)) >> info_tms_value_shift;
        gpio_pin_set_dt(&config->tms_swdio_gpio, tms_val);

        while (tck_cycles > 0) {
            uint8_t tdi = 0;
            ring_buf_get(config->request_buf, &tdi, 1);
            uint8_t tdo = 0;

            uint8_t bits = 8;
            while (bits > 0 && tck_cycles > 0) {
                uint8_t tdo_bit = JTAG_TDIO_CYCLE(tdi);
                tdi >>= 1;
                tdo >>= 1;
                tdo |= tdo_bit << 7;
                bits--;
                tck_cycles--;
            }
            /* if we are on byte boundary, no-op, otherwise move tdo to final bit position */
            tdo >>= bits;

            if ((info & info_tdo_capture_mask) != 0) {
                ring_buf_put(config->response_buf, &tdo, 1);
            }
        }
    }

end: ;
    *command_status = status;
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_jtag_idcode(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_swd_configure(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_swd_sequence(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}
