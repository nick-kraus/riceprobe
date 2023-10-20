#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "dap/transport.h"
#include "util.h"

LOG_MODULE_REGISTER(dap, CONFIG_DAP_LOG_LEVEL);

/* ensure we have one and exactly one dap driver in the devicetree */
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(rice_dap) == 1);
#define DAP_DT_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(rice_dap)

static struct dap_driver dap = {
    .io = {
        .tck_swclk = GPIO_DT_SPEC_GET(DAP_DT_NODE, tck_swclk_gpios),
        .tms_swdio = GPIO_DT_SPEC_GET(DAP_DT_NODE, tms_swdio_gpios),
        .tdo = GPIO_DT_SPEC_GET(DAP_DT_NODE, tdo_gpios),
        .tdi = GPIO_DT_SPEC_GET(DAP_DT_NODE, tdi_gpios),
        .nreset = GPIO_DT_SPEC_GET(DAP_DT_NODE, nreset_gpios),
        .vtref = GPIO_DT_SPEC_GET(DAP_DT_NODE, vtref_gpios),
        .led_connect = GPIO_DT_SPEC_GET(DAP_DT_NODE, led_connect_gpios),
        .led_running = GPIO_DT_SPEC_GET(DAP_DT_NODE, led_running_gpios),
        .swo_uart = DEVICE_DT_GET(DT_PHANDLE(DAP_DT_NODE, swo_uart)),
    },
    .pinctrl = {
        .jtag_state_pins = Z_PINCTRL_STATE_PINS_INIT(DAP_DT_NODE, pinctrl_jtag),
        .swd_state_pins = Z_PINCTRL_STATE_PINS_INIT(DAP_DT_NODE, pinctrl_swd),
    },
};

static void handle_running_led_timer(struct k_timer *timer) {
    struct dap_driver *dap = timer->user_data;

    /* we choose to manually control the running led if the leds are combined,
     * so we don't need to check here, and can just do a simple toggle */
    gpio_pin_toggle_dt(&dap->io.led_running);
}

static void swo_uart_isr(const struct device *dev, void *user_data) {
    struct dap_driver *dap = user_data;

    if (uart_err_check(dev) > 0) {
        dap->swo.error = true;
    }

    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t *ptr;
        uint32_t space = ring_buf_put_claim(&dap->buf.swo, &ptr, DAP_SWO_RING_BUF_SIZE);
        if (space == 0) {
            dap->swo.overrun = true;
            uint8_t drop;
            LOG_ERR("buffer full, dropping swo data");
            while (uart_fifo_read(dev, &drop, 1) > 0) {
                continue;
            }
            break;
        }

        int32_t read = uart_fifo_read(dev, ptr, space);
        if (read < 0) {
            LOG_ERR("swo fifo read failed with error %d", read);
            read = 0;
        }
        /* should never fail since we always read less than the claim size */
        FATAL_CHECK(ring_buf_put_finish(&dap->buf.swo, read) == 0, "swo buffer read fail");  
    }

    return;
}

int32_t dap_reset(struct dap_driver *dap) {
    LOG_INF("resetting driver state");

    /* config the pinctrl settings for the tdo/swo pin, default to tdo functionality */
    FATAL_CHECK(dap_configure_pin(dap->pinctrl.jtag_state_pins) == 0, "tdo/swo pinctrl failed");

    /* jtag / swd gpios must be in a safe state on reset */
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tck_swclk, GPIO_INPUT) >= 0, "tck swclk config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT) >= 0, "tms swdio config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tdo, GPIO_INPUT) >= 0, "tdo config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.tdi, GPIO_INPUT) >= 0, "tdi config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap->io.nreset, GPIO_INPUT) >= 0, "nreset config failed");

    dap->transport = NULL;

    /* set all internal state to sane defaults */
    dap->swj.port = dap_port_disabled;
    dap->swj.clock = dap_default_swj_clock_rate;
    dap->swj.delay_ns = 1000000000 / dap_default_swj_clock_rate / 2;
    dap->jtag.count = 0;
    dap->jtag.index = 0;
    memset(dap->jtag.ir_length, 0, sizeof(dap->jtag.ir_length));
    memset(dap->jtag.ir_before, 0, sizeof(dap->jtag.ir_before));
    memset(dap->jtag.ir_after, 0, sizeof(dap->jtag.ir_after));
    dap->swd.turnaround_cycles = 1;
    dap->swd.data_phase = false;
    dap->swo.transport = 0;
    dap->swo.mode = 0;
    dap->swo.baudrate = 1000000;
    dap->swo.capture = false;
    dap->swo.error = false;
    dap->swo.overrun = false;
    dap->transfer.idle_cycles = 0;
    dap->transfer.wait_retries = 100;
    dap->transfer.match_retries = 0;
    dap->transfer.match_mask = 0;

    dap->led.connected = false;
    dap->led.running = false;
    k_timer_stop(&dap->led.timer);
    gpio_pin_set_dt(&dap->io.led_connect, 0);
    gpio_pin_set_dt(&dap->io.led_running, 0);

    uart_irq_rx_disable(dap->io.swo_uart);
    uart_irq_err_disable(dap->io.swo_uart);
    struct uart_config uart_config = {
        .baudrate = dap->swo.baudrate,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    FATAL_CHECK(uart_configure(dap->io.swo_uart, &uart_config) >= 0, "uart config failed");

    ring_buf_reset(&dap->buf.request);
    ring_buf_reset(&dap->buf.response);
    ring_buf_reset(&dap->buf.swo);

    return 0;
}

int32_t dap_handle_request(struct dap_driver *dap) {
    /* this will usually just run once, unless an atomic command is being used */
    uint8_t num_commands = 1;
    bool queued_commands = false;
    do {
        if (queued_commands && num_commands == 0) {
            /* this may be the last command in the chain, if not we will reset this flag later */
            num_commands = 1;
            queued_commands = false;
        }

        /* data should be available at the front of the ring buffer before calling this handler */
        uint8_t command = 0xff;
        if (ring_buf_get(&dap->buf.request, &command, 1) != 1) return -EMSGSIZE;

        if (command == dap_cmd_queue_commands) {
            queued_commands = true;
            /* response of queued command is identical to execute commands, so replace the current
             * command to re-use the existing code path */
            command = dap_cmd_execute_commands;
        }
        if (command == dap_cmd_execute_commands) {
            if (ring_buf_get(&dap->buf.request, &num_commands, 1) != 1) return -EMSGSIZE;
            if (ring_buf_put(&dap->buf.response, &command, 1) != 1) return -ENOBUFS;
            if (ring_buf_put(&dap->buf.response, &num_commands, 1) != 1) return -ENOBUFS;
            /* get the next command for processing */
            if (ring_buf_get(&dap->buf.request, &command, 1) != 1) return -EMSGSIZE;
        }

        int32_t ret;
        if (command == dap_cmd_info) { ret = dap_handle_cmd_info(dap); }
        else if (command == dap_cmd_host_status) { ret = dap_handle_cmd_host_status(dap); }
        else if (command == dap_cmd_connect) { ret = dap_handle_cmd_connect(dap); }
        else if (command == dap_cmd_disconnect) { ret = dap_handle_cmd_disconnect(dap); }
        else if (command == dap_cmd_transfer_configure) { ret = dap_handle_cmd_transfer_configure(dap); }
        else if (command == dap_cmd_transfer) { ret = dap_handle_cmd_transfer(dap); }
        else if (command == dap_cmd_transfer_block) { ret = dap_handle_cmd_transfer_block(dap); }
        else if (command == dap_cmd_transfer_abort) { ret = dap_handle_cmd_transfer_abort(dap); }
        else if (command == dap_cmd_write_abort) { ret = dap_handle_cmd_write_abort(dap); }
        else if (command == dap_cmd_delay) { ret = dap_handle_cmd_delay(dap); }
        else if (command == dap_cmd_reset_target) { ret = dap_handle_cmd_reset_target(dap); }
        else if (command == dap_cmd_swj_pins) { ret = dap_handle_cmd_swj_pins(dap); }
        else if (command == dap_cmd_swj_clock) { ret = dap_handle_cmd_swj_clock(dap); }
        else if (command == dap_cmd_swj_sequence) { ret = dap_handle_cmd_swj_sequence(dap); }
        else if (command == dap_cmd_swd_configure) { ret = dap_handle_cmd_swd_configure(dap); }
        else if (command == dap_cmd_jtag_sequence) { ret = dap_handle_cmd_jtag_sequence(dap); }
        else if (command == dap_cmd_jtag_configure) { ret = dap_handle_cmd_jtag_configure(dap); }
        else if (command == dap_cmd_jtag_idcode) { ret = dap_handle_cmd_jtag_idcode(dap); }
        else if (command == dap_cmd_swo_transport) { ret = dap_handle_cmd_swo_transport(dap); }
        else if (command == dap_cmd_swo_mode) { ret = dap_handle_cmd_swo_mode(dap); }
        else if (command == dap_cmd_swo_baudrate) { ret = dap_handle_cmd_swo_baudrate(dap); }
        else if (command == dap_cmd_swo_control) { ret = dap_handle_cmd_swo_control(dap); }
        else if (command == dap_cmd_swo_status) { ret = dap_handle_cmd_swo_status(dap); }
        else if (command == dap_cmd_swo_data) { ret = dap_handle_cmd_swo_data(dap); }
        else if (command == dap_cmd_swd_sequence) { ret = dap_handle_cmd_swd_sequence(dap); }
        else if (command == dap_cmd_swo_extended_status) { ret = dap_handle_cmd_swo_extended_status(dap); } 
        else {
            /* for dap_cmd_uart_*, no intention of support, since the same functionality can be found 
             * over the CDC-ACM virtual com port interface. any other command is totally unknown. */
            LOG_ERR("unsupported command 0x%x", command);
            ret = -ENOTSUP;
        }

        if (ret < 0) {
            LOG_ERR("handle command 0x%x failed with error %d", command, ret);
            return ret;
        }
        num_commands--;

    } while (num_commands > 0 || queued_commands);

    return 0;
}

void dap_thread_fn(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct dap_driver *dap = arg1;
    int32_t ret;

    while (1) {
        if (dap->transport == NULL) {
            STRUCT_SECTION_FOREACH(dap_transport, transport) {
                if ((ret = transport->configure()) == 0) {
                    LOG_DBG("configured transport %s", transport->name);
                    dap->transport = transport;
                    /* just for passing the sleep below */
                    continue;
                } else if (ret < 0 && ret != -EAGAIN) {
                    LOG_ERR("transport configuration failed with error %d", ret);
                }
            }

            k_sleep(K_MSEC(50));
        }

        if (dap->transport != NULL) {
            uint8_t *request;
            uint32_t request_len = ring_buf_put_claim(&dap->buf.request, &request, DAP_MAX_PACKET_SIZE);
            
            if ((ret = dap->transport->recv(request, request_len)) < 0) {
                /* shutdown is an expected condition */
                if (ret != -ESHUTDOWN) LOG_ERR("transport receive failed with error %d", ret);
                dap_reset(dap);
                continue;
            }
            ring_buf_put_finish(&dap->buf.request, ret);

            /* not ready to process command, start the next receive */
            if (*request == dap_cmd_queue_commands) continue;

            /* transport sends rely on having the full length of the response ring buffer from one pointer */
            ring_buf_reset(&dap->buf.response);
            if ((ret = dap_handle_request(dap)) < 0) {
                /* commands that failed or aren't implemented get a simple 0xff reponse byte */
                ring_buf_reset(&dap->buf.response);
                uint8_t response = dap_cmd_response_error;
                FATAL_CHECK(ring_buf_put(&dap->buf.response, &response, 1) == 1, "response buf is size 0");
            }

            uint8_t *response;
            uint32_t response_len = ring_buf_get_claim(&dap->buf.response, &response, DAP_RING_BUF_SIZE);
            if ((ret = dap->transport->send(response, response_len)) < 0) {
                /* shutdown is an expected condition */
                if (ret != -ESHUTDOWN) LOG_ERR("transport send failed with error %d", ret);
                dap_reset(dap);
                continue;
            } else if (ret < response_len) {
                LOG_ERR("transport send dropped %d bytes", response_len - ret);
            }
            ring_buf_get_finish(&dap->buf.response, ret);

            /* transport receives rely on having the full length of the request ring buffer from one pointer */
            ring_buf_reset(&dap->buf.request);
        }
    }
}

K_THREAD_DEFINE(
    dap_thread,
    KB(4),
    dap_thread_fn,
    &dap,
    NULL,
    NULL,
    CONFIG_MAIN_THREAD_PRIORITY + 1,
    0,
    K_TICKS_FOREVER
);

int32_t dap_init(void) {
    int32_t ret;

    /* determine whether we have shared or independent status led */
    if (dap.io.led_connect.port == dap.io.led_running.port &&
        dap.io.led_connect.pin == dap.io.led_running.pin) {
        dap.led.combined = true;
    } else {
        dap.led.combined = false;
    }
    /* if combined, the second call will have no affect */
    FATAL_CHECK(gpio_pin_configure_dt(&dap.io.led_connect, GPIO_OUTPUT_INACTIVE) >= 0, "led config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&dap.io.led_running, GPIO_OUTPUT_INACTIVE) >= 0, "led config failed");
    /* the running led will blink when in use, set up a timer to control this blinking */
    k_timer_init(&dap.led.timer, handle_running_led_timer, NULL);
    k_timer_user_data_set(&dap.led.timer, (void*) &dap);

    /* vtref is only ever an input, doesn't need reconfiguration in dap_reset */
    FATAL_CHECK(gpio_pin_configure_dt(&dap.io.vtref, GPIO_INPUT) >= 0, "vtref config failed");

    if (!device_is_ready(dap.io.swo_uart)) return -ENODEV;
    uart_irq_rx_disable(dap.io.swo_uart);
    uart_irq_tx_disable(dap.io.swo_uart);
    uart_irq_callback_user_data_set(dap.io.swo_uart, swo_uart_isr, (void*) &dap);

    ring_buf_init(&dap.buf.request, sizeof(dap.buf.request_bytes), dap.buf.request_bytes);
    ring_buf_init(&dap.buf.response, sizeof(dap.buf.response_bytes), dap.buf.response_bytes);
    ring_buf_init(&dap.buf.swo, sizeof(dap.buf.swo_bytes), dap.buf.swo_bytes);

    if ((ret = dap_reset(&dap)) < 0) return ret;

    STRUCT_SECTION_FOREACH(dap_transport, transport) {
        if ((ret = transport->init()) < 0) {
            LOG_ERR("transport %s init failed with error %d", transport->name, ret);
            return ret;
        }
    }

    k_thread_start(dap_thread);

    return 0;
}
