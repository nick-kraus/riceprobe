#ifndef __DAP_PRIV_H__
#define __DAP_PRIV_H__

#include <drivers/gpio.h>
#include <sys/ring_buffer.h>
#include <usb/usb_device.h>
#include <zephyr.h>

/* supported version of the DAP protocol */
#define DAP_PROTOCOL_VERSION    "2.1.1"

/* size of the internal buffers in bytes */
#define DAP_RING_BUF_SIZE       (1024)

/* set if connected led should be enabled */
#define DAP_STATUS_LED_CONNECTED    BIT(0)
/* set if running led should be enabled */
#define DAP_STATUS_LED_RUNNING      BIT(1)
/* set if both connected and running status share an led */
#define DAP_STATUS_LEDS_COMBINED    BIT(7)

/* current configured state of the dap io port */
#define DAP_PORT_DISABLED   0
#define DAP_PORT_JTAG       1
#define DAP_PORT_SWD        2

struct dap_data {
    bool configured;
    uint8_t port_state;

    const struct device *dev;
    sys_snode_t devlist_node;

    uint8_t led_state;
    struct k_timer running_led_timer;
};

struct dap_config {
    struct ring_buf *request_buf;
    struct ring_buf *response_buf;

    struct usb_cfg_data *usb_config;

    struct gpio_dt_spec tck_swclk_gpio;
    struct gpio_dt_spec tms_swdio_gpio;
    struct gpio_dt_spec tdo_gpio;
    struct gpio_dt_spec tdi_gpio;
    struct gpio_dt_spec nreset_gpio;
    struct gpio_dt_spec vtref_gpio;
    struct gpio_dt_spec led_connect_gpio;
    struct gpio_dt_spec led_running_gpio;
};

extern sys_slist_t dap_devlist;

bool dap_is_configured(const struct device *dev);
int32_t dap_configure(const struct device *dev);
int32_t dap_reset(const struct device *dev);
int32_t dap_handle_request(const struct device *dev);

#endif /* __DAP_PRIV_H__ */
