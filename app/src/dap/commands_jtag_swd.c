#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

#define PIN_SWCLK_TCK_SHIFT  (0)
#define PIN_SWDIO_TMS_SHIFT  (1)
#define PIN_TDI_SHIFT        (2)
#define PIN_TDO_SHIFT        (3)
#define PIN_NRESET_SHIFT     (7)

int32_t dap_handle_command_swj_pins(const struct device *dev) {
    const struct dap_config *config = dev->config;

    uint8_t pin_output = 0;
    uint8_t pin_mask = 0;
    uint32_t delay_us = 0;
    ring_buf_get(config->request_buf, &pin_output, 1);
    ring_buf_get(config->request_buf, &pin_mask, 1);
    ring_buf_get(config->request_buf, (uint8_t*) &delay_us, 4);
    delay_us = sys_le32_to_cpu(delay_us);

    if ((pin_mask & BIT(PIN_SWCLK_TCK_SHIFT)) != 0) {
        gpio_pin_set_dt(&config->tck_swclk_gpio, (pin_output & BIT(PIN_SWCLK_TCK_SHIFT)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(PIN_SWDIO_TMS_SHIFT)) != 0) {
        gpio_pin_set_dt(&config->tms_swdio_gpio, (pin_output & BIT(PIN_SWDIO_TMS_SHIFT)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(PIN_TDI_SHIFT)) != 0) {
        gpio_pin_set_dt(&config->tdi_gpio, (pin_output & BIT(PIN_TDI_SHIFT)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(PIN_TDO_SHIFT)) != 0) {
        gpio_pin_set_dt(&config->tdo_gpio, (pin_output & BIT(PIN_TDO_SHIFT)) == 0 ? 0 : 1);
    }
    if ((pin_mask & BIT(PIN_NRESET_SHIFT)) != 0) {
        gpio_pin_set_dt(&config->nreset_gpio, (pin_output & BIT(PIN_NRESET_SHIFT)) == 0 ? 0 : 1);
    }
    /* ignore nTRST, this debug probe doesn't support it */

    /* maximum wait time allowed by command */
    if (delay_us > 3000000) {
        delay_us = 3000000;
    }
    /* all pins expect nreset are push-pull, don't wait on those */
    if ((delay_us > 0) && (pin_mask & BIT(PIN_NRESET_SHIFT))) {
        uint64_t delay_end = sys_clock_timeout_end_calc(K_USEC(delay_us));
        do {
            uint32_t nreset_val = gpio_pin_get_dt(&config->nreset_gpio);
            if ((pin_output & BIT(PIN_NRESET_SHIFT)) ^ (nreset_val << PIN_NRESET_SHIFT)) {
                k_busy_wait(1);
            } else {
                break;
            }
        } while (delay_end > k_uptime_ticks());
    }

    uint8_t pin_input =
        (gpio_pin_get_dt(&config->tck_swclk_gpio) << PIN_SWCLK_TCK_SHIFT) |
        (gpio_pin_get_dt(&config->tms_swdio_gpio) << PIN_SWDIO_TMS_SHIFT) |
        (gpio_pin_get_dt(&config->tdi_gpio) << PIN_TDI_SHIFT) |
        (gpio_pin_get_dt(&config->tdo_gpio) << PIN_TDO_SHIFT) |
        (gpio_pin_get_dt(&config->nreset_gpio) << PIN_NRESET_SHIFT);
    uint8_t response[] = {DAP_COMMAND_SWJ_PINS, pin_input};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swj_clock(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint32_t clock = DAP_DEFAULT_SWJ_CLOCK_RATE;
    ring_buf_get(config->request_buf, (uint8_t*) &clock, 4);
    if (clock != 0) {
        data->swj.clock = sys_le32_to_cpu(clock);
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
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_jtag_sequence(const struct device *dev) {
    return -ENOTSUP; /* TODO */
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
