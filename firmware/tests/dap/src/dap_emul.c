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
    /* bitstream of tms/swdio direction (0 = output, 1 = input) */
    uint8_t tms_swdio_dir[128];
    /* bitstream of data input to tms/swdio probe io */
    uint8_t tms_swdio_in[128];
    /* bitstream of captured output data from tms/swdio probe io */
    uint8_t tms_swdio_out[128];
    /* bitstream of captured output data from tdi probe io */
    uint8_t tdi_out[128];
    /* bitstream of data input to tdo probe io */
    uint8_t tdo_in[128];
} dap_emul;

static void dap_emul_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    /* only track sequences up to 1024 cycles, to keep buffers to a reasonable size. */
    if (dap_emul.clk_cycles >= 1024) return;

    uint16_t bitstream_byte_idx = dap_emul.clk_cycles / 8;
    uint8_t bitstream_bit_idx = dap_emul.clk_cycles % 8;

    gpio_flags_t flags;
    gpio_emul_flags_get(dap_io_tms_swdio->port, dap_io_tms_swdio->pin, &flags);
    bool swdio_output = (flags & GPIO_OUTPUT) == GPIO_OUTPUT ? true : false;

    /* in a standard JTAG device, TMS and TDI are sampled on the rising edge of TCK, and TDO changes on
     * the falling edge to TCK. */
    if (gpio_pin_get_dt(dap_io_tck_swclk) == 1) {
        /* rising edge */

        /* calculate new average clock period, but make sure to keep the average consistent if the period does not change,
        * so that compounding rounding errors don't cause problems */
        uint64_t new_clk = k_ticks_to_ns_floor64(k_uptime_ticks());
        if (dap_emul.last_clk_cycle != 0) {
            uint64_t this_clk_period = new_clk - dap_emul.last_clk_cycle;
            if (this_clk_period != dap_emul.clk_period_avg) {
                uint64_t prev_avg = dap_emul.clk_period_avg * (dap_emul.clk_cycles - 1);
                dap_emul.clk_period_avg = (prev_avg + this_clk_period) / (dap_emul.clk_cycles);
            }
        }
        dap_emul.last_clk_cycle = new_clk;

        /* record the values of all probe outputs */
        dap_emul.tms_swdio_dir[bitstream_byte_idx] |= (swdio_output ? 0 : 1) << bitstream_bit_idx;
        dap_emul.tms_swdio_out[bitstream_byte_idx] |= gpio_pin_get_dt(dap_io_tms_swdio) << bitstream_bit_idx;
        dap_emul.tdi_out[bitstream_byte_idx] |= gpio_pin_get_dt(dap_io_tdi) << bitstream_bit_idx;
        dap_emul.clk_cycles++;
    } else {
        /* falling edge */

        /* set all probe input values */
        gpio_port_value_t tdo = (dap_emul.tdo_in[bitstream_byte_idx] >> bitstream_bit_idx) & 1;
        gpio_emul_input_set(dap_io_tdo->port, dap_io_tdo->pin, tdo);

        if (!swdio_output) {
            gpio_port_value_t swdio = (dap_emul.tms_swdio_in[bitstream_byte_idx] >> bitstream_bit_idx) & 1;
            gpio_emul_input_set(dap_io_tms_swdio->port, dap_io_tms_swdio->pin, swdio);
        }
    }
}

void dap_emul_init(void) {
    gpio_init_callback(&dap_emul.cb, dap_emul_handler, BIT(dap_io_tck_swclk->pin));
}

void dap_emul_reset(void) {
    dap_emul.clk_cycles = 0;
    dap_emul.last_clk_cycle = 0;
    dap_emul.clk_period_avg = 0;
    memset(dap_emul.tms_swdio_dir, 0, sizeof(dap_emul.tms_swdio_dir));
    memset(dap_emul.tms_swdio_in, 0, sizeof(dap_emul.tms_swdio_in));
    memset(dap_emul.tms_swdio_out, 0, sizeof(dap_emul.tms_swdio_out));
    memset(dap_emul.tdi_out, 0, sizeof(dap_emul.tdi_out));
    memset(dap_emul.tdo_in, 0, sizeof(dap_emul.tdo_in));
}

void dap_emul_start(void) {
    dap_emul_reset();

    gpio_pin_interrupt_configure_dt(dap_io_tck_swclk, GPIO_INT_EDGE_BOTH);
    gpio_add_callback_dt(dap_io_tck_swclk, &dap_emul.cb);
}

void dap_emul_end(void) {
    gpio_pin_interrupt_configure_dt(dap_io_tck_swclk, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(dap_io_tck_swclk, &dap_emul.cb);
}

void dap_emul_set_tdo_in(uint8_t *data, size_t len) {
    memcpy(dap_emul.tdo_in, (void *) data, MIN(len, sizeof(dap_emul.tdo_in)));
}

void dap_emul_set_tms_swdio_in(uint8_t *data, size_t len) {
    memcpy(dap_emul.tms_swdio_in, (void *) data, MIN(len, sizeof(dap_emul.tms_swdio_in)));
}

uint16_t dap_emul_get_clk_cycles(void) {
    return dap_emul.clk_cycles;
}

uint64_t dap_emul_get_avg_clk_period(void) {
    return dap_emul.clk_period_avg;
}

uint8_t* dap_emul_get_tms_swdio_dir(void) {
    return dap_emul.tms_swdio_dir;
}

uint8_t* dap_emul_get_tms_swdio_out(void) {
    return dap_emul.tms_swdio_out;
}

uint8_t* dap_emul_get_tdi_out(void) {
    return dap_emul.tdi_out;
}
