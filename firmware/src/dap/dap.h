#ifndef __DAP_PRIV_H__
#define __DAP_PRIV_H__

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/transport.h"

/* size of the internal buffers in bytes */
#define DAP_RING_BUF_SIZE       (2048)
/* size of the swo uart buffer in bytes */
#define DAP_SWO_RING_BUF_SIZE   (2048)
/* maximum size for any single transport transfer */
#define DAP_MAX_PACKET_SIZE     (512)

/* maximum number of devices supported on the JTAG chain */
#define DAP_JTAG_MAX_DEVICE_COUNT   4

/* default SWD/JTAG clock rate in Hz */
static const uint32_t dap_default_swj_clock_rate = 1000000;

/* current configured state of the dap io port */
static const uint8_t dap_port_disabled = 0;
static const uint8_t dap_port_jtag = 1;
static const uint8_t dap_port_swd = 2;

/* possible status responses to commands */
static const uint8_t dap_cmd_response_ok = 0x00;
static const uint8_t dap_cmd_response_error = 0xff;

struct dap_driver {
    struct {
        struct gpio_dt_spec tck_swclk;
        struct gpio_dt_spec tms_swdio;
        struct gpio_dt_spec tdo;
        struct gpio_dt_spec tdi;
        struct gpio_dt_spec nreset;
        struct gpio_dt_spec vtref;
        struct gpio_dt_spec led_connect;
        struct gpio_dt_spec led_running;

        const struct device *swo_uart;
    } io;
    struct {
        pinctrl_soc_pin_t jtag_state_pins;
        pinctrl_soc_pin_t swd_state_pins;
    } pinctrl;

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
        uint8_t request_bytes[DAP_RING_BUF_SIZE];
        struct ring_buf request;
        uint8_t response_bytes[DAP_RING_BUF_SIZE];
        struct ring_buf response;
        uint8_t swo_bytes[DAP_SWO_RING_BUF_SIZE];
        struct ring_buf swo;
    } buf;

    struct dap_transport *transport;
};

/* command ids */
static const uint8_t dap_cmd_info = 0x00;
static const uint8_t dap_cmd_host_status = 0x01;
static const uint8_t dap_cmd_connect = 0x02;
static const uint8_t dap_cmd_disconnect = 0x03;
static const uint8_t dap_cmd_transfer_configure = 0x04;
static const uint8_t dap_cmd_transfer = 0x05;
static const uint8_t dap_cmd_transfer_block = 0x06;
static const uint8_t dap_cmd_transfer_abort = 0x07;
static const uint8_t dap_cmd_write_abort = 0x08;
static const uint8_t dap_cmd_delay = 0x09;
static const uint8_t dap_cmd_reset_target = 0x0a;
static const uint8_t dap_cmd_swj_pins = 0x10;
static const uint8_t dap_cmd_swj_clock = 0x11;
static const uint8_t dap_cmd_swj_sequence = 0x12;
static const uint8_t dap_cmd_swd_configure = 0x13;
static const uint8_t dap_cmd_jtag_sequence = 0x14;
static const uint8_t dap_cmd_jtag_configure = 0x15;
static const uint8_t dap_cmd_jtag_idcode = 0x16;
static const uint8_t dap_cmd_swo_transport = 0x17;
static const uint8_t dap_cmd_swo_mode = 0x18;
static const uint8_t dap_cmd_swo_baudrate = 0x19;
static const uint8_t dap_cmd_swo_control = 0x1a;
static const uint8_t dap_cmd_swo_status = 0x1b;
static const uint8_t dap_cmd_swo_data = 0x1c;
static const uint8_t dap_cmd_swd_sequence = 0x1d;
static const uint8_t dap_cmd_swo_extended_status = 0x1e;
static const uint8_t dap_cmd_queue_commands = 0x7e;
static const uint8_t dap_cmd_execute_commands = 0x7f;

/* command handlers */
int32_t dap_handle_cmd_info(struct dap_driver *dap);
int32_t dap_handle_cmd_host_status(struct dap_driver *dap);
int32_t dap_handle_cmd_connect(struct dap_driver *dap);
int32_t dap_handle_cmd_disconnect(struct dap_driver *dap);
int32_t dap_handle_cmd_transfer_configure(struct dap_driver *dap);
int32_t dap_handle_cmd_transfer(struct dap_driver *dap);
int32_t dap_handle_cmd_transfer_block(struct dap_driver *dap);
int32_t dap_handle_cmd_transfer_abort(struct dap_driver *dap);
int32_t dap_handle_cmd_write_abort(struct dap_driver *dap);
int32_t dap_handle_cmd_delay(struct dap_driver *dap);
int32_t dap_handle_cmd_reset_target(struct dap_driver *dap);
int32_t dap_handle_cmd_swj_pins(struct dap_driver *dap);
int32_t dap_handle_cmd_swj_clock(struct dap_driver *dap);
int32_t dap_handle_cmd_swj_sequence(struct dap_driver *dap);
int32_t dap_handle_cmd_swd_configure(struct dap_driver *dap);
int32_t dap_handle_cmd_jtag_sequence(struct dap_driver *dap);
int32_t dap_handle_cmd_jtag_configure(struct dap_driver *dap);
int32_t dap_handle_cmd_jtag_idcode(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_transport(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_mode(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_baudrate(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_control(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_status(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_data(struct dap_driver *dap);
int32_t dap_handle_cmd_swd_sequence(struct dap_driver *dap);
int32_t dap_handle_cmd_swo_extended_status(struct dap_driver *dap);

/** @brief performs single tck clock cycle */
void jtag_tck_cycle(struct dap_driver *dap);
/** @brief performs clock cycle with tdi data output */
void jtag_tdi_cycle(struct dap_driver *dap, uint8_t tdi);
/** @brief performs clock cycle with tdo data retreival */
uint8_t jtag_tdo_cycle(struct dap_driver *dap);
/** @brief performs clock cycle with tdi data output and tdo data retreival */
uint8_t jtag_tdio_cycle(struct dap_driver *dap, uint8_t tdi);
/** @brief sets JTAG IR instruction */
void jtag_set_ir(struct dap_driver *dap, uint32_t ir);

/** @brief reads single SWD bit */
uint8_t swd_read_cycle(struct dap_driver *dap);
/** @brief writes single SWD bit */
void swd_write_cycle(struct dap_driver *dap, uint8_t swdio);
/** @brief performs single swclk clock cycle */
void swd_swclk_cycle(struct dap_driver *dap);

/** @brief enables SWO uart capture */
void swo_capture_control(struct dap_driver *dap, bool enable);

/** @brief configure a dap_driver pinctrl state */
static inline int32_t dap_configure_pin(const pinctrl_soc_pin_t *pinctrl_state) {
    return pinctrl_configure_pins(pinctrl_state, 1, PINCTRL_REG_NONE);
}

#endif /* __DAP_PRIV_H__ */
