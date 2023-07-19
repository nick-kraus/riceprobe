#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "dap_io.h"

static struct {
    struct gpio_callback cb;
    uint16_t clk_cycles;
    /* last clock rising edge measured in nanoseconds */
    uint64_t last_clk_cycle;
    /* average clock period measured in nanoseconds */
    uint64_t clk_period_avg;
    /* bitstream of tms/swdio input data from probe */
    uint8_t tms_swdio_in[128];
    /* bitstream of tdi values input data from probe */
    uint8_t tdi_in[128];
    // /* bitstream of tdo values output data to probe */
    uint8_t tdo_out[128];
} dap_target_emul;

static void dap_target_emul_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    /* only track sequences up to 1024 cycles, to keep buffers to a reasonable size. */
    if (dap_target_emul.clk_cycles >= 1024) return;

    uint16_t bitstream_byte_idx = dap_target_emul.clk_cycles / 8;
    uint8_t bitstream_bit_idx = dap_target_emul.clk_cycles % 8;

    /* in a standard JTAG device, TMS and TDI are sampled on the rising edge of TCK, and TDO changes on
     * the falling edge to TCK. */
    if (gpio_pin_get_dt(dap_io_tck_swclk) == 1) {
        /* rising edge */

        /* calculate new average clock period, but make sure to keep the average consistent if the period does not change,
        * so that compounding rounding errors don't cause problems */
        uint64_t new_clk = k_ticks_to_ns_floor64(k_uptime_ticks());
        if (dap_target_emul.last_clk_cycle != 0) {
            uint64_t this_clk_period = new_clk - dap_target_emul.last_clk_cycle;
            if (this_clk_period != dap_target_emul.clk_period_avg) {
                uint64_t prev_avg = dap_target_emul.clk_period_avg * (dap_target_emul.clk_cycles - 1);
                dap_target_emul.clk_period_avg = (prev_avg + this_clk_period) / (dap_target_emul.clk_cycles);
            }
        }
        dap_target_emul.last_clk_cycle = new_clk;
        
        /* record all input values set from probe */
        dap_target_emul.tms_swdio_in[bitstream_byte_idx] |= gpio_pin_get_dt(dap_io_tms_swdio) << bitstream_bit_idx;
        dap_target_emul.tdi_in[bitstream_byte_idx] |= gpio_pin_get_dt(dap_io_tdi) << bitstream_bit_idx;
        dap_target_emul.clk_cycles++;
    } else {
        /* falling edge */

        /* emulate all target output values */
        gpio_port_value_t tdo = (dap_target_emul.tdo_out[bitstream_byte_idx] >> bitstream_bit_idx) & 1;
        gpio_emul_input_set(dap_io_tdo->port, dap_io_tdo->pin, tdo);
    }
}

void dap_target_emul_init(void) {
    gpio_init_callback(&dap_target_emul.cb, dap_target_emul_handler, BIT(dap_io_tck_swclk->pin));
}

void dap_target_emul_reset(void) {
    dap_target_emul.clk_cycles = 0;
    dap_target_emul.last_clk_cycle = 0;
    dap_target_emul.clk_period_avg = 0;
    memset(dap_target_emul.tms_swdio_in, 0, sizeof(dap_target_emul.tms_swdio_in));
    memset(dap_target_emul.tdi_in, 0, sizeof(dap_target_emul.tdi_in));
    memset(dap_target_emul.tdo_out, 0, sizeof(dap_target_emul.tdo_out));
}

void dap_target_emul_start(void) {
    dap_target_emul_reset();

    gpio_pin_interrupt_configure_dt(dap_io_tck_swclk, GPIO_INT_EDGE_BOTH);
    gpio_add_callback_dt(dap_io_tck_swclk, &dap_target_emul.cb);
}

void dap_target_emul_end(void) {
    gpio_pin_interrupt_configure_dt(dap_io_tck_swclk, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(dap_io_tck_swclk, &dap_target_emul.cb);
}

void dap_target_emul_set_tdo_out(uint8_t *data, size_t len) {
    memcpy(dap_target_emul.tdo_out, (void *) data, MIN(len, sizeof(dap_target_emul.tdo_out)));
}

uint16_t dap_target_emul_clk_cycles(void) {
    return dap_target_emul.clk_cycles;
}

uint64_t dap_target_emul_avg_clk_period(void) {
    return dap_target_emul.clk_period_avg;
}

uint8_t* dap_target_emul_tms_swdio_in(void) {
    return dap_target_emul.tms_swdio_in;
}

uint8_t* dap_target_emul_tdi_in(void) {
    return dap_target_emul.tdi_in;
}
