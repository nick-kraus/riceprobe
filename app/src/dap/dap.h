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

/* default SWD/JTAG clock rate in Hz */
#define DAP_DEFAULT_SWJ_CLOCK_RATE    (1000000)

/* current configured state of the dap io port */
#define DAP_PORT_DISABLED   0
#define DAP_PORT_JTAG       1
#define DAP_PORT_SWD        2

struct dap_data {
    /* shared swd and jtag state */
    struct {
        /* current configuration state of the port */
        uint8_t port;
        /* nominal output clock rate in hz */
        uint32_t clock;
    } swj;

    struct {
        bool combined : 1;
        bool connected : 1;
        bool running : 1;
        struct k_timer timer;
    } led;

    const struct device *dev;
    sys_snode_t devlist_node;
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

int32_t dap_configure(const struct device *dev);
int32_t dap_reset(const struct device *dev);
int32_t dap_handle_request(const struct device *dev);

#endif /* __DAP_PRIV_H__ */
