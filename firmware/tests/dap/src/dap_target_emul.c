#include <zephyr/drivers/gpio.h>
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
} dap_target_emul;

static void dap_target_emul_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    /* only track sequences up to 1024 cycles, to keep buffers to a reasonable size. */
    if (dap_target_emul.clk_cycles >= 1024) return;

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

    dap_target_emul.clk_cycles++;
}

void dap_target_emul_init(void) {
    gpio_init_callback(&dap_target_emul.cb, dap_target_emul_handler, BIT(dap_io_tck_swclk->pin));
}

void dap_target_emul_reset(void) {
    dap_target_emul.clk_cycles = 0;
    dap_target_emul.last_clk_cycle = 0;
    dap_target_emul.clk_period_avg = 0;
}

void dap_target_emul_start(void) {
    dap_target_emul_reset();

    gpio_pin_interrupt_configure_dt(dap_io_tck_swclk, GPIO_INT_EDGE_RISING);
    gpio_add_callback_dt(dap_io_tck_swclk, &dap_target_emul.cb);
}

void dap_target_emul_end(void) {
    gpio_pin_interrupt_configure_dt(dap_io_tck_swclk, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(dap_io_tck_swclk, &dap_target_emul.cb);
}

uint64_t dap_target_emul_avg_clk_period(void) {
    return dap_target_emul.clk_period_avg;
}
