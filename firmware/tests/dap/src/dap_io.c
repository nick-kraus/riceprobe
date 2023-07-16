#include <zephyr/drivers/gpio.h>

struct gpio_dt_spec *dap_io_tck_swclk = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tck_swclk_gpios);
struct gpio_dt_spec *dap_io_tms_swdio = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tms_swdio_gpios);
struct gpio_dt_spec *dap_io_tdo = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tdo_gpios);
struct gpio_dt_spec *dap_io_tdi = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tdi_gpios);
struct gpio_dt_spec *dap_io_nreset = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), nreset_gpios);
struct gpio_dt_spec *dap_io_vtref = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), vtref_gpios);
struct gpio_dt_spec *dap_io_led_connect = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), led_connect_gpios);
struct gpio_dt_spec *dap_io_led_running = &(struct gpio_dt_spec) GPIO_DT_SPEC_GET(DT_NODELABEL(dap), led_running_gpios);
