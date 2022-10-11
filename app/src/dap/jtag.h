#ifndef __DAP_JTAG_PRIV_H__
#define __DAP_JTAG_PRIV_H__

#include <stdint.h>

#include "dap/dap.h"
#include "util.h"

static inline void jtag_tck_cycle(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
}

static inline void jtag_tdi_cycle(const struct device *dev, uint8_t tdi) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tdi_gpio, tdi & 0x01);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
}

static inline uint8_t jtag_tdo_cycle(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    uint8_t tdo = gpio_pin_get_dt(&config->tdo_gpio);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
    return tdo;
}

static inline uint8_t jtag_tdio_cycle(const struct device *dev, uint8_t tdi) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tdi_gpio, tdi & 0x01);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    uint8_t tdo = gpio_pin_get_dt(&config->tdo_gpio);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
    return tdo;
}

void jtag_set_ir(const struct device *dev, uint8_t index, uint32_t ir);
uint32_t jtag_get_dr_le32(const struct device *dev, uint8_t index);

#endif /* __DAP_JTAG_PRIV_H__ */
