#ifndef __DAP_IO_H__
#define __DAP_IO_H__

#include <zephyr/drivers/gpio.h>

extern struct gpio_dt_spec *dap_io_tck_swclk;
extern struct gpio_dt_spec *dap_io_tms_swdio;
extern struct gpio_dt_spec *dap_io_tdo;
extern struct gpio_dt_spec *dap_io_tdi;
extern struct gpio_dt_spec *dap_io_nreset;
extern struct gpio_dt_spec *dap_io_vtref;
extern struct gpio_dt_spec *dap_io_led_connect;
extern struct gpio_dt_spec *dap_io_led_running;

extern const struct device *dap_swo_uart;

#endif /* __DAP_IO_H__ */
