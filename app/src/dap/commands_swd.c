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

static inline void swd_swclk_cycle(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
}

uint8_t swd_transfer(const struct device *dev, uint8_t request, uint32_t *transfer_data) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* 8-bit packet request */
    uint8_t ap_ndp = request >> TRANSFER_REQUEST_APnDP_SHIFT;
    uint8_t r_nw = request >> TRANSFER_REQUEST_RnW_SHIFT;
    uint8_t a2 = request >> TRANSFER_REQUEST_A2_SHIFT;
    uint8_t a3 = request >> TRANSFER_REQUEST_A3_SHIFT;
    uint32_t parity = ap_ndp + r_nw + a2 + a3;
    /* start bit */
    swd_write_cycle(dev, 1);
    swd_write_cycle(dev, ap_ndp);
    swd_write_cycle(dev, r_nw);
    swd_write_cycle(dev, a2);
    swd_write_cycle(dev, a3);
    swd_write_cycle(dev, parity);
    /* stop then park bits */
    swd_write_cycle(dev, 0);
    swd_write_cycle(dev, 1);

    /* turnaround bits */
    FATAL_CHECK(
        gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0,
        "tms swdio config failed"
    );
    for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
        swd_swclk_cycle(dev);
    }

    /* acknowledge bits */
    uint8_t ack = 0;
    ack |= swd_read_cycle(dev) << 0;
    ack |= swd_read_cycle(dev) << 1;
    ack |= swd_read_cycle(dev) << 2;

    if (ack == TRANSFER_RESPONSE_ACK_OK) {
        if ((request & TRANSFER_REQUEST_RnW) != 0) {
            /* read data */
            uint32_t read = 0;
            parity = 0;
            for (uint8_t i = 0; i < 32; i++) {
                uint8_t bit = swd_read_cycle(dev);
                read |= bit << i;
                parity += bit;
            }
            uint8_t parity_bit = swd_read_cycle(dev);
            if ((parity & 0x01) != parity_bit) {
                ack = TRANSFER_RESPONSE_ERROR;
            }
            *transfer_data = read;
            /* turnaround bits */
            for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
                swd_swclk_cycle(dev);
            }
            FATAL_CHECK(
                gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
        } else {
            /* turnaround bits */
            for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
                swd_swclk_cycle(dev);
            }
            FATAL_CHECK(
                gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
            /* write data */
            uint32_t write = *transfer_data;
            parity = 0;
            for (uint8_t i = 0; i < 32; i++) {
                swd_write_cycle(dev, (uint8_t) write);
                parity += write;
                write >>= 1;
            }
            swd_write_cycle(dev, (uint8_t) parity);
        }
        /* idle cycles */
        for (uint8_t i = 0; i < data->transfer.idle_cycles; i++) {
            swd_write_cycle(dev, 0);
        }
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        return ack;
    } else if (ack == TRANSFER_RESPONSE_ACK_WAIT || ack == TRANSFER_RESPONSE_FAULT) {
        if (data->swd.data_phase && (request & TRANSFER_REQUEST_RnW) != 0) {
            /* dummy read through 32 bits and parity */
            for (uint8_t i = 0; i < 33; i++) {
                swd_swclk_cycle(dev);
            }
        }
        /* turnaround bits */
        for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
            swd_swclk_cycle(dev);
        }
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
        if (data->swd.data_phase && (request & TRANSFER_REQUEST_RnW) == 0) {
            gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
            /* dummy write through 32 bits and parity */
            for (uint8_t i = 0; i < 33; i++) {
                swd_swclk_cycle(dev);
            }
        }
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        return ack;
    } else {
        /* dummy read through turnaround bits, 32 bits and parity */
        for (uint8_t i = 0; i < data->swd.turnaround_cycles + 33; i++) {
            swd_swclk_cycle(dev);
        }
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        return ack;
    }
}

int32_t dap_handle_command_swd_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* configuration bitmasks */
    const uint8_t turnaround_mask = 0x03;
    const uint8_t data_phase_mask = 0x04;

    uint8_t configuration = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &configuration, 1), 1, -EMSGSIZE);
    data->swd.turnaround_cycles = (configuration & turnaround_mask) + 1;
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
