#ifndef __DAP_PRIV_H__
#define __DAP_PRIV_H__

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>

/* supported version of the DAP protocol */
#define DAP_PROTOCOL_VERSION    "2.1.1"

/* size of the internal buffers in bytes */
#define DAP_RING_BUF_SIZE       (512)
/* size of the swo uart buffer in bytes */
#define DAP_SWO_RING_BUF_SIZE   (4096)

/* default SWD/JTAG clock rate in Hz */
#define DAP_DEFAULT_SWJ_CLOCK_RATE    (1000000)

/* current configured state of the dap io port */
#define DAP_PORT_DISABLED   0
#define DAP_PORT_JTAG       1
#define DAP_PORT_SWD        2

/* maximum number of devices supported on the JTAG chain */
#define DAP_JTAG_MAX_DEVICE_COUNT   4

/* pinctrl state for tdo/swo pin as gpio */
#define PINCTRL_STATE_TDO       ((uint8_t) 0)
/* pinctrl state for tdo/swo pin as uart rx */
#define PINCTRL_STATE_SWO       ((uint8_t) 1)

struct dap_data {
    /* shared swd and jtag state */
    struct {
        /* current configuration state of the port */
        uint8_t port;
        /* nominal output clock rate in hz */
        uint32_t clock;
        /* nanosecond pin delay based off above clock rate */
        uint32_t delay_ns;
    } swj;
    struct {
        /* number of devices in chain */
        uint8_t count;
        /* current transaction index in chain */
        uint8_t index;
        /* ir length of each device in the chain */
        uint8_t ir_length[DAP_JTAG_MAX_DEVICE_COUNT];
        /* ir length before each device in the chain */
        uint16_t ir_before[DAP_JTAG_MAX_DEVICE_COUNT];
        /* ir length after each device in the chain */
        uint16_t ir_after[DAP_JTAG_MAX_DEVICE_COUNT];
    } jtag;
    struct {
        /* turnaround clock period of the SWD bus */
        uint8_t turnaround_cycles;
        /* whether or not to generate a data phase */
        bool data_phase;
    } swd;
    struct {
        /* transport for swo data to host */
        uint8_t transport;
        /* interface mode for swo data to target */
        uint8_t mode;
        /* uart mode baudrate */
        uint32_t baudrate;
        /* if true then swo data is actively being captured */
        bool capture;
        /* true if a uart error has occurred, clears on swo disable */
        bool error;
        /* true if the swo buffer has overrun, clears on swo enable */
        bool overrun;
    } swo;
    struct {
        /* number of extra idle cycles after each transfer */
        uint8_t idle_cycles;
        /* number of transfer retries after WAIT response */
        uint16_t wait_retries;
        /* number of retries on reads with value match */
        uint16_t match_retries;
        /* read match mask */
        uint32_t match_mask;
    } transfer;

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
    uint8_t *ep_buf;
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

    const struct device *swo_uart_dev;
    struct ring_buf *swo_buf;

    const struct pinctrl_dev_config *pinctrl_config;
};

extern sys_slist_t dap_devlist;

int32_t dap_configure(const struct device *dev);
int32_t dap_reset(const struct device *dev);
int32_t dap_handle_request(const struct device *dev);

#endif /* __DAP_PRIV_H__ */
