#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

static inline uint8_t swd_read_cycle(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    uint8_t swdio = gpio_pin_get_dt(&config->tms_swdio_gpio);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
    return swdio & 0x01;
}

static inline void swd_write_cycle(const struct device *dev, uint8_t swdio) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tms_swdio_gpio, swdio & 0x01);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
}

int32_t dap_handle_command_swd_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* configuration bitmasks */
    const uint8_t turnaround_mask = 0x03;
    const uint8_t data_phase_mask = 0x04;

    uint8_t configuration = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &configuration, 1), 1, -EMSGSIZE);
    data->swd.turnaround_cycles = configuration & turnaround_mask;
    data->swd.data_phase = (configuration & data_phase_mask) == 0 ? false : true;

    uint8_t response[] = {DAP_COMMAND_SWD_CONFIGURE, DAP_COMMAND_RESPONSE_OK};
    CHECK_EQ(ring_buf_put(config->response_buf, response, 2), 2, -ENOBUFS);
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swd_sequence(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* sequence info bitmasks */
    const uint8_t info_swclk_cycles_mask = 0x3f;
    const uint8_t info_mode_mask = 0x80;

    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_SWD_SEQUENCE}), 1), 1, -ENOBUFS);
    /* need a pointer to this item because we will write to it after trying the rest of the command */
    uint8_t *command_status = NULL;
    CHECK_EQ(ring_buf_put_claim(config->response_buf, &command_status, 1), 1, -ENOBUFS);
    *command_status = 0;
    CHECK_EQ(ring_buf_put_finish(config->response_buf, 1), 0, -ENOBUFS);

    uint8_t seq_count = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &seq_count, 1), 1, -EMSGSIZE);
    for (uint8_t i = 0; i < seq_count; i++) {
        uint8_t info = 0;
        CHECK_EQ(ring_buf_get(config->request_buf, &info, 1), 1, -EMSGSIZE);

        uint8_t swclk_cycles = info & info_swclk_cycles_mask;
        if (swclk_cycles == 0) {
            swclk_cycles = 64;
        }

        /* if the current port isn't SWD, just use this loop to process remaining bytes */
        if (data->swj.port != DAP_PORT_SWD) {
            status = DAP_COMMAND_RESPONSE_ERROR;
            uint8_t bytes = (swclk_cycles + 7) / 8;
            for (uint8_t j = 0; j < bytes; j++) {
                /* respond with the expected command length as if everything worked */
                if ((info & info_mode_mask) != 0) {
                    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {0}), 1), 1, -ENOBUFS);
                } else {
                    uint8_t temp = 0;
                    CHECK_EQ(ring_buf_get(config->request_buf, &temp, 1), 1, -EMSGSIZE);
                }
            }
            continue;
        }

        if ((info & info_mode_mask) != 0) {
            FATAL_CHECK(
                gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0,
                "tms swdio config failed"
            );
            while (swclk_cycles > 0) {
                uint8_t swdio = 0;
                uint8_t bits = 8;
                while (bits > 0 && swclk_cycles > 0) {
                    uint8_t swdio_bit = swd_read_cycle(dev);
                    swdio >>= 1;
                    swdio |= swdio_bit << 7;
                    bits--;
                    swclk_cycles--;
                }
                /* if we are on a byte boundary, no-op, otherwise move swdio to final bit position */
                swdio >>= bits;
                CHECK_EQ(ring_buf_put(config->response_buf, &swdio, 1), 1, -ENOBUFS);
            }
        } else {
            FATAL_CHECK(
                gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
            while (swclk_cycles > 0) {
                uint8_t swdio = 0;
                CHECK_EQ(ring_buf_get(config->request_buf, &swdio, 1), 1, -EMSGSIZE);
                uint8_t bits = 8;
                while (bits > 0 && swclk_cycles > 0) {
                    swd_write_cycle(dev, swdio);
                    swdio >>= 1;
                    bits--;
                    swclk_cycles--;
                }
            }
        }
    }

    if (status == DAP_COMMAND_RESPONSE_OK) {
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
    }

    *command_status = status;
    return ring_buf_size_get(config->response_buf);
}
