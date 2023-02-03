#ifndef __DAP_PRIV_H__
#define __DAP_PRIV_H__

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>

/* ensure we have one and exactly one dap driver in the devicetree */
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(rice_dap) == 1);
#define DAP_DT_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(rice_dap)

/* size of the internal buffers in bytes */
#define DAP_RING_BUF_SIZE       (2048)
/* size of the swo uart buffer in bytes */
#define DAP_SWO_RING_BUF_SIZE   (2048)
/* maximum size for any single transport transfer */
#define DAP_MAX_PACKET_SIZE     (512)

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

/* possible status responses to commands */
#define DAP_COMMAND_RESPONSE_OK     ((uint8_t) 0x00)
#define DAP_COMMAND_RESPONSE_ERROR  ((uint8_t) 0xff)

/* general command ids */
#define DAP_COMMAND_INFO                    ((uint8_t) 0x00)
#define DAP_COMMAND_HOST_STATUS             ((uint8_t) 0x01)
#define DAP_COMMAND_CONNECT                 ((uint8_t) 0x02)
#define DAP_COMMAND_DISCONNECT              ((uint8_t) 0x03)
#define DAP_COMMAND_TRANSFER_CONFIGURE      ((uint8_t) 0x04)
#define DAP_COMMAND_TRANSFER                ((uint8_t) 0x05)
#define DAP_COMMAND_TRANSFER_BLOCK          ((uint8_t) 0x06)
#define DAP_COMMAND_TRANSFER_ABORT          ((uint8_t) 0x07)
#define DAP_COMMAND_WRITE_ABORT             ((uint8_t) 0x08)
#define DAP_COMMAND_DELAY                   ((uint8_t) 0x09)
#define DAP_COMMAND_RESET_TARGET            ((uint8_t) 0x0a)
#define DAP_COMMAND_SWJ_PINS                ((uint8_t) 0x10)
#define DAP_COMMAND_SWJ_CLOCK               ((uint8_t) 0x11)
#define DAP_COMMAND_SWJ_SEQUENCE            ((uint8_t) 0x12)
#define DAP_COMMAND_SWD_CONFIGURE           ((uint8_t) 0x13)
#define DAP_COMMAND_JTAG_SEQUENCE           ((uint8_t) 0x14)
#define DAP_COMMAND_JTAG_CONFIGURE          ((uint8_t) 0x15)
#define DAP_COMMAND_JTAG_IDCODE             ((uint8_t) 0x16)
#define DAP_COMMAND_SWO_TRANSPORT           ((uint8_t) 0x17)
#define DAP_COMMAND_SWO_MODE                ((uint8_t) 0x18)
#define DAP_COMMAND_SWO_BAUDRATE            ((uint8_t) 0x19)
#define DAP_COMMAND_SWO_CONTROL             ((uint8_t) 0x1a)
#define DAP_COMMAND_SWO_STATUS              ((uint8_t) 0x1b)
#define DAP_COMMAND_SWO_DATA                ((uint8_t) 0x1c)
#define DAP_COMMAND_SWD_SEQUENCE            ((uint8_t) 0x1d)
#define DAP_COMMAND_SWO_EXTENDED_STATUS     ((uint8_t) 0x1e)
#define DAP_COMMAND_UART_TRANSPORT          ((uint8_t) 0x1f)
#define DAP_COMMAND_UART_CONFIGURE          ((uint8_t) 0x20)
#define DAP_COMMAND_UART_TRANSFER           ((uint8_t) 0x21)
#define DAP_COMMAND_UART_CONTROL            ((uint8_t) 0x22)
#define DAP_COMMAND_UART_STATUS             ((uint8_t) 0x23)
#define DAP_COMMAND_QUEUE_COMMANDS          ((uint8_t) 0x7e)
#define DAP_COMMAND_EXECUTE_COMMANDS        ((uint8_t) 0x7f)

/* thread events */
#define DAP_THREAD_EVENT_DISCONNECT         (BIT(0))
#define DAP_THREAD_EVENT_USB_CONNECT        (BIT(1))
#define DAP_THREAD_EVENT_READ_READY         (BIT(2))
#define DAP_THREAD_EVENT_WRITE_COMPLETE     (BIT(3))

/* all supported transports */
#define DAP_TRANSPORT_NONE                  ((uint8_t) 0)
#define DAP_TRANSPORT_USB                   ((uint8_t) 1)

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

    struct {
        /* points to the start of the command at the buffer tail, once a request has been received */
        uint8_t *request_tail;
        uint8_t request_bytes[DAP_RING_BUF_SIZE];
        struct ring_buf request;
        uint8_t response_bytes[DAP_RING_BUF_SIZE];
        struct ring_buf response;
        uint8_t swo_bytes[DAP_SWO_RING_BUF_SIZE];
        struct ring_buf swo;
    } buf;

    struct {
        /* primary driver thread, handles all dap I/O */
        struct k_thread driver;
        /* events for the main driver thread to wait on */
        struct k_event event;
        /* which transport is currently configured, if any */
        uint8_t transport;
    } thread;
};

struct dap_config {
    struct gpio_dt_spec tck_swclk_gpio;
    struct gpio_dt_spec tms_swdio_gpio;
    struct gpio_dt_spec tdo_gpio;
    struct gpio_dt_spec tdi_gpio;
    struct gpio_dt_spec nreset_gpio;
    struct gpio_dt_spec vtref_gpio;
    struct gpio_dt_spec led_connect_gpio;
    struct gpio_dt_spec led_running_gpio;

    const struct device *swo_uart_dev;

    const struct pinctrl_dev_config *pinctrl_config;
};

#endif /* __DAP_PRIV_H__ */
