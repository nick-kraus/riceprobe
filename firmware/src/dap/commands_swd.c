#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

uint8_t swd_read_cycle(struct dap_driver *dap) {
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    uint8_t swdio = gpio_pin_get_dt(&dap->io.tms_swdio);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
    return swdio & 0x01;
}

void swd_write_cycle(struct dap_driver *dap, uint8_t swdio) {
    gpio_pin_set_dt(&dap->io.tms_swdio, swdio & 0x01);
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
}

void swd_swclk_cycle(struct dap_driver *dap) {
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
}

int32_t dap_handle_cmd_swd_configure(struct dap_driver *dap) {
    /* configuration bitmasks */
    const uint8_t turnaround_mask = 0x03;
    const uint8_t data_phase_mask = 0x04;

    uint8_t configuration = 0;
    if (ring_buf_get(&dap->buf.request, &configuration, 1) != 1) return -EMSGSIZE;
    dap->swd.turnaround_cycles = (configuration & turnaround_mask) + 1;
    dap->swd.data_phase = (configuration & data_phase_mask) == 0 ? false : true;

    uint8_t response[] = {dap_cmd_swd_configure, dap_cmd_response_ok};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_swd_sequence(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    /* sequence info bitmasks */
    const uint8_t info_swclk_cycles_mask = 0x3f;
    const uint8_t info_mode_mask = 0x80;

    if (ring_buf_put(&dap->buf.response, &dap_cmd_swd_sequence, 1) != 1) return -ENOBUFS;
    /* need a pointer to this item because we will write to it after trying the rest of the command */
    uint8_t *response_status = NULL;
    if (ring_buf_put_claim(&dap->buf.response, &response_status, 1) != 1) return -ENOBUFS;
    if (ring_buf_put_finish(&dap->buf.response, 1) < 0) return -ENOBUFS;

    uint8_t seq_count = 0;
    if (ring_buf_get(&dap->buf.request, &seq_count, 1) != 1) return -EMSGSIZE;
    for (uint8_t i = 0; i < seq_count; i++) {
        uint8_t info = 0;
        if (ring_buf_get(&dap->buf.request, &info, 1) != 1) return -EMSGSIZE;

        uint8_t swclk_cycles = info & info_swclk_cycles_mask;
        if (swclk_cycles == 0) {
            swclk_cycles = 64;
        }

        /* if the current port isn't SWD, just use this loop to process remaining bytes */
        if (dap->swj.port != dap_port_swd) {
            status = dap_cmd_response_error;
            uint8_t bytes = (swclk_cycles + 7) / 8;
            for (uint8_t j = 0; j < bytes; j++) {
                /* respond with the expected command length as if everything worked */
                if ((info & info_mode_mask) != 0) {
                    const uint8_t temp = 0;
                    if (ring_buf_put(&dap->buf.response, &temp, 1) != 1) return -ENOBUFS;
                } else {
                    uint8_t temp = 0;
                    if (ring_buf_get(&dap->buf.request, &temp, 1) != 1) return -EMSGSIZE;
                }
            }
            continue;
        }

        if ((info & info_mode_mask) != 0) {
            FATAL_CHECK(
                gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT) >= 0,
                "tms swdio config failed"
            );
            while (swclk_cycles > 0) {
                uint8_t swdio = 0;
                uint8_t bits = 8;
                while (bits > 0 && swclk_cycles > 0) {
                    uint8_t swdio_bit = swd_read_cycle(dap);
                    swdio >>= 1;
                    swdio |= swdio_bit << 7;
                    bits--;
                    swclk_cycles--;
                }
                /* if we are on a byte boundary, no-op, otherwise move swdio to final bit position */
                swdio >>= bits;
                if (ring_buf_put(&dap->buf.response, &swdio, 1) != 1) return -ENOBUFS;
            }
        } else {
            FATAL_CHECK(
                gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
            while (swclk_cycles > 0) {
                uint8_t swdio = 0;
                if (ring_buf_get(&dap->buf.request, &swdio, 1) != 1) return -EMSGSIZE;
                uint8_t bits = 8;
                while (bits > 0 && swclk_cycles > 0) {
                    swd_write_cycle(dap, swdio);
                    swdio >>= 1;
                    bits--;
                    swclk_cycles--;
                }
            }
        }
    }

    if (status == dap_cmd_response_ok) {
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
    }

    memcpy(response_status, &status, 1);
    return 0;
}
