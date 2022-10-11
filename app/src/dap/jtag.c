#include <drivers/gpio.h>
#include <sys/byteorder.h>
#include <zephyr.h>

#include "dap/jtag.h"

void jtag_set_ir(const struct device *dev, uint8_t index, uint32_t ir) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* assumes we are starting in idle tap state, move to select-dr-scan then select-ir-scan */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* capture-ir, then shift-ir */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* bypass all tap bits before index */
    gpio_pin_set_dt(&config->tdi_gpio, 1);
    for (int i = 0; i < data->jtag.ir_before[index]; i++) {
        jtag_tck_cycle(dev);
    }
    /* set all ir bits except the last */
    for (int i = 0; i < data->jtag.ir_length[index] - 1; i++) {
        jtag_tdi_cycle(dev, ir);
        ir >>= 1;
    }
    /* set last ir bit and bypass all remaining ir bits */
    if (data->jtag.ir_after[index] == 0) {
        /* set last ir bit, then exit-1-ir */
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        jtag_tdi_cycle(dev, ir);
    } else {
        jtag_tdi_cycle(dev, ir);
        gpio_pin_set_dt(&config->tdi_gpio, 1);
        for (int i = 0; i < data->jtag.ir_after[index] - 1; i++) {
            jtag_tck_cycle(dev);
        }
        /* set last bypass bit, then exit-1-ir */
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        jtag_tck_cycle(dev);
    }

    /* update-ir then idle */
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tdi_gpio, 1);

    return;
}

uint32_t jtag_get_dr_le32(const struct device *dev, uint8_t index) {
    const struct dap_config *config = dev->config;

    /* assumes we are starting in idle tap state, move to select-dr-scan */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    jtag_tck_cycle(dev);

    /* capture-dr, then shift-dr */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* bypass for every tap before the current index */
    for (int i = 0; i < index; i++) {
        jtag_tck_cycle(dev);
    }

    /* tdo bits 0..30 */
    uint32_t word = 0;
    for (int i = 0; i < 31; i++) {
        word |= jtag_tdo_cycle(dev) << i;
    }
    /* last tdo bit and exit-1-dr*/
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    word |= jtag_tdo_cycle(dev) << 31;

    /* update-dr, then idle */
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);

    return sys_le32_to_cpu(word);
}
