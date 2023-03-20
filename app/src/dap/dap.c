#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/slist.h>

#include "dap/dap.h"
#include "util.h"

LOG_MODULE_REGISTER(dap, CONFIG_DAP_LOG_LEVEL);

/* stack space for the dap driver thread */
K_THREAD_STACK_DEFINE(dap_driver_thread_stack, 4096);
/* stack space for the tcp transport thread */
K_THREAD_STACK_DEFINE(dap_tcp_thread_stack, 2048);

/* command specific handlers declarations */
int32_t dap_handle_command_info(const struct device *dev);
int32_t dap_handle_command_host_status(const struct device *dev);
int32_t dap_handle_command_connect(const struct device *dev);
int32_t dap_handle_command_disconnect(const struct device *dev);
int32_t dap_handle_command_transfer_configure(const struct device *dev);
int32_t dap_handle_command_transfer(const struct device *dev);
int32_t dap_handle_command_transfer_block(const struct device *dev);
int32_t dap_handle_command_transfer_abort(const struct device *dev);
int32_t dap_handle_command_write_abort(const struct device *dev);
int32_t dap_handle_command_delay(const struct device *dev);
int32_t dap_handle_command_reset_target(const struct device *dev);
int32_t dap_handle_command_swj_pins(const struct device *dev);
int32_t dap_handle_command_swj_clock(const struct device *dev);
int32_t dap_handle_command_swj_sequence(const struct device *dev);
int32_t dap_handle_command_swd_configure(const struct device *dev);
int32_t dap_handle_command_jtag_sequence(const struct device *dev);
int32_t dap_handle_command_jtag_configure(const struct device *dev);
int32_t dap_handle_command_jtag_idcode(const struct device *dev);
int32_t dap_handle_command_swo_transport(const struct device *dev);
int32_t dap_handle_command_swo_mode(const struct device *dev);
int32_t dap_handle_command_swo_baudrate(const struct device *dev);
int32_t dap_handle_command_swo_control(const struct device *dev);
int32_t dap_handle_command_swo_status(const struct device *dev);
int32_t dap_handle_command_swo_data(const struct device *dev);
int32_t dap_handle_command_swd_sequence(const struct device *dev);
int32_t dap_handle_command_swo_extended_status(const struct device *dev);

/* transport declarations */
int32_t dap_usb_recv_begin(const struct device *dev);
int32_t dap_usb_send(const struct device *dev);
void dap_usb_stop(const struct device *dev);
int32_t dap_tcp_init(const struct device *dev);
int32_t dap_tcp_recv_begin(const struct device *dev);
int32_t dap_tcp_send(const struct device *dev);
void dap_tcp_stop(const struct device *dev);
void dap_tcp_thread_fn(void* arg1, void* arg2, void* arg3);

static void dap_transport_buf_reset(const struct device *dev) {
    struct dap_data *data = dev->data;

    ring_buf_reset(&data->buf.request);
    ring_buf_reset(&data->buf.response);
    /* no data in the request buffer, tail should point to the very beginning */
    data->buf.request_tail = data->buf.request_bytes;
}

int32_t dap_reset(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    LOG_INF("resetting driver state");

    /* config the pinctrl settings for the tdo/swo pin, default to tdo functionality */
    FATAL_CHECK(pinctrl_apply_state(config->pinctrl_config, PINCTRL_STATE_TDO) >= 0, "tdo pinctrl failed");

    /* jtag / swd gpios must be in a safe state on reset */
    FATAL_CHECK(gpio_pin_configure_dt(&config->tck_swclk_gpio, GPIO_INPUT) >= 0, "tck swclk config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0, "tms swdio config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdo_gpio, GPIO_INPUT) >= 0, "tdo config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT) >= 0, "tdi config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->nreset_gpio, GPIO_INPUT) >= 0, "nreset config failed");

    /* deselect any current transport */
    data->thread.transport = DAP_TRANSPORT_NONE;
    dap_usb_stop(dev);
    dap_tcp_stop(dev);
    /* reset thread events */
    k_event_set(&data->thread.event, 0);

    /* set all internal state to sane defaults */
    data->swj.port = DAP_PORT_DISABLED;
    data->swj.clock = DAP_DEFAULT_SWJ_CLOCK_RATE;
    data->swj.delay_ns = 1000000000 / DAP_DEFAULT_SWJ_CLOCK_RATE / 2;
    data->jtag.count = 0;
    data->jtag.index = 0;
    memset(data->jtag.ir_length, 0, sizeof(data->jtag.ir_length));
    memset(data->jtag.ir_before, 0, sizeof(data->jtag.ir_before));
    memset(data->jtag.ir_after, 0, sizeof(data->jtag.ir_after));
    data->swd.turnaround_cycles = 1;
    data->swd.data_phase = false;
    data->swo.transport = 0;
    data->swo.mode = 0;
    data->swo.baudrate = 1000000;
    data->swo.capture = false;
    data->swo.error = false;
    data->swo.overrun = false;
    data->transfer.idle_cycles = 0;
    data->transfer.wait_retries = 100;
    data->transfer.match_retries = 0;
    data->transfer.match_mask = 0;

    data->led.connected = false;
    data->led.running = false;
    k_timer_stop(&data->led.timer);
    gpio_pin_set_dt(&config->led_connect_gpio, 0);
    gpio_pin_set_dt(&config->led_running_gpio, 0);

    uart_irq_rx_disable(config->swo_uart_dev);
    uart_irq_err_disable(config->swo_uart_dev);
    struct uart_config uart_config = {
        .baudrate = data->swo.baudrate,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    FATAL_CHECK(uart_configure(config->swo_uart_dev, &uart_config) >= 0, "uart config failed");

    dap_transport_buf_reset(dev);
    ring_buf_reset(&data->buf.swo);

    return 0;
}

int32_t dap_handle_request(const struct device *dev) {
    struct dap_data *data = dev->data;

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
        CHECK_EQ(ring_buf_get(&data->buf.request, &command, 1), 1, -EMSGSIZE);

        if (command == DAP_COMMAND_QUEUE_COMMANDS) {
            queued_commands = true;
            /* response of queued command is identical to execute commands, so replace the current
             * command to re-use the existing code path */
            command = DAP_COMMAND_EXECUTE_COMMANDS;
        }
        if (command == DAP_COMMAND_EXECUTE_COMMANDS) {
            CHECK_EQ(ring_buf_get(&data->buf.request, &num_commands, 1), 1, -EMSGSIZE);
            CHECK_EQ(ring_buf_put(&data->buf.response, &command, 1), 1, -ENOBUFS);
            CHECK_EQ(ring_buf_put(&data->buf.response, &num_commands, 1), 1, -ENOBUFS);
            /* get the next command for processing */
            CHECK_EQ(ring_buf_get(&data->buf.request, &command, 1), 1, -EMSGSIZE);
        }

        int32_t ret;
        if (command == DAP_COMMAND_INFO) {
            ret = dap_handle_command_info(dev);
        } else if (command == DAP_COMMAND_HOST_STATUS) {
            ret = dap_handle_command_host_status(dev);
        } else if (command == DAP_COMMAND_CONNECT) {
            ret = dap_handle_command_connect(dev);
        } else if (command == DAP_COMMAND_DISCONNECT) {
            ret = dap_handle_command_disconnect(dev);
        } else if (command == DAP_COMMAND_TRANSFER_CONFIGURE) {
            ret = dap_handle_command_transfer_configure(dev);
        } else if (command == DAP_COMMAND_TRANSFER) {
            ret = dap_handle_command_transfer(dev);
        } else if (command == DAP_COMMAND_TRANSFER_BLOCK) {
            ret = dap_handle_command_transfer_block(dev);
        } else if (command == DAP_COMMAND_TRANSFER_ABORT) {
            ret = dap_handle_command_transfer_abort(dev);
        } else if (command == DAP_COMMAND_WRITE_ABORT) {
            ret = dap_handle_command_write_abort(dev);
        } else if (command == DAP_COMMAND_DELAY) {
            ret = dap_handle_command_delay(dev);
        } else if (command == DAP_COMMAND_RESET_TARGET) {
            ret = dap_handle_command_reset_target(dev);
        } else if (command == DAP_COMMAND_SWJ_PINS) {
            ret = dap_handle_command_swj_pins(dev);
        } else if (command == DAP_COMMAND_SWJ_CLOCK) {
            ret = dap_handle_command_swj_clock(dev);
        } else if (command == DAP_COMMAND_SWJ_SEQUENCE) {
            ret = dap_handle_command_swj_sequence(dev);
        } else if (command == DAP_COMMAND_SWD_CONFIGURE) {
            ret = dap_handle_command_swd_configure(dev);
        } else if (command == DAP_COMMAND_JTAG_SEQUENCE) {
            ret = dap_handle_command_jtag_sequence(dev);
        } else if (command == DAP_COMMAND_JTAG_CONFIGURE) {
            ret = dap_handle_command_jtag_configure(dev);
        } else if (command == DAP_COMMAND_JTAG_IDCODE) {
            ret = dap_handle_command_jtag_idcode(dev);
        } else if (command == DAP_COMMAND_SWO_TRANSPORT) {
            ret = dap_handle_command_swo_transport(dev);
        } else if (command == DAP_COMMAND_SWO_MODE) {
            ret = dap_handle_command_swo_mode(dev);
        } else if (command == DAP_COMMAND_SWO_BAUDRATE) {
            ret = dap_handle_command_swo_baudrate(dev);
        } else if (command == DAP_COMMAND_SWO_CONTROL) {
            ret = dap_handle_command_swo_control(dev);
        } else if (command == DAP_COMMAND_SWO_STATUS) {
            ret = dap_handle_command_swo_status(dev);
        } else if (command == DAP_COMMAND_SWO_DATA) {
            ret = dap_handle_command_swo_data(dev);
        } else if (command == DAP_COMMAND_SWD_SEQUENCE) {
            ret = dap_handle_command_swd_sequence(dev);
        } else if (command == DAP_COMMAND_SWO_EXTENDED_STATUS) {
            ret = dap_handle_command_swo_extended_status(dev);
        } else {
            /* for DAP_COMMAND_UART_*, no intention of support, since the same functionality can be found 
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

static void handle_running_led_timer(struct k_timer *timer) {
    const struct device *dev = timer->user_data;
    const struct dap_config *config = dev->config;

    /* we choose to manually control the running led if the leds are combined,
     * so we don't need to check here, and can just do a simple toggle */
    gpio_pin_toggle_dt(&config->led_running_gpio);
}

static void swo_uart_isr(const struct device *dev, void *user_data) {
    const struct device *dap_dev = user_data;
    struct dap_data *data = dap_dev->data;

    if (uart_err_check(dev) > 0) {
        data->swo.error = true;
    }

    while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
        uint8_t *ptr;
        uint32_t space = ring_buf_put_claim(&data->buf.swo, &ptr, DAP_SWO_RING_BUF_SIZE);
        if (space == 0) {
            data->swo.overrun = true;
            uint8_t drop;
            LOG_ERR("buffer full, flushing swo data");
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
        FATAL_CHECK(ring_buf_put_finish(&data->buf.swo, read) == 0, "swo buffer read fail");  
    }

    return;
}

static int32_t dap_transport_recv_begin(const struct device *dev) {
    struct dap_data *data = dev->data;
    int32_t ret;

    if (data->thread.transport == DAP_TRANSPORT_USB) {
        ret = dap_usb_recv_begin(dev);
    } else if (data->thread.transport == DAP_TRANSPORT_TCP) {
        ret = dap_tcp_recv_begin(dev);
    } else {
        LOG_ERR("no transport configured for next request");
        return -ENOTCONN;
    }

    if (ret < 0) LOG_ERR("receive begin failed with error %d", ret);
    return ret;
}

static int32_t dap_transport_send(const struct device *dev) {
    struct dap_data *data = dev->data;
    int32_t ret;

    if (data->thread.transport == DAP_TRANSPORT_USB) {
        ret = dap_usb_send(dev);
    } else if (data->thread.transport == DAP_TRANSPORT_TCP) {
        ret = dap_tcp_send(dev);
    } else {
        LOG_ERR("no transport configured for response");
        return -ENOTCONN;
    }

    if (ret < 0) LOG_ERR("send begin failed with error %d", ret);
    return ret;
}

void dap_thread_fn(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    const struct device *dev = (const struct device*) arg1;
    struct dap_data *data = dev->data;

    while (1) {
        uint32_t wait_events;
        if (data->thread.transport == DAP_TRANSPORT_NONE) {
            wait_events = DAP_THREAD_EVENT_USB_CONNECT | DAP_THREAD_EVENT_TCP_CONNECT;
        } else {
            wait_events = DAP_THREAD_EVENT_READ_READY | DAP_THREAD_EVENT_DISCONNECT;
        }
        uint32_t events = k_event_wait(&data->thread.event, wait_events, false, K_FOREVER);

        /* reset will halt any in-progress transport transfers, and set transport to DAP_TRANSPORT_NONE */
        if ((events & DAP_THREAD_EVENT_DISCONNECT) != 0) {
            k_event_set_masked(&data->thread.event, 0, DAP_THREAD_EVENT_DISCONNECT);
            dap_reset(dev);
            continue;
        }
        
        if ((events & DAP_THREAD_EVENT_USB_CONNECT) != 0) {
            k_event_set_masked(&data->thread.event, 0, DAP_THREAD_EVENT_USB_CONNECT);
            data->thread.transport = DAP_TRANSPORT_USB;
            if (dap_transport_recv_begin(dev) < 0) dap_reset(dev);
        }

        if ((events & DAP_THREAD_EVENT_TCP_CONNECT) != 0) {
            k_event_set_masked(&data->thread.event, 0, DAP_THREAD_EVENT_TCP_CONNECT);
            data->thread.transport = DAP_TRANSPORT_TCP;
            if (dap_transport_recv_begin(dev) < 0) dap_reset(dev);
        }

        if ((events & DAP_THREAD_EVENT_READ_READY) != 0) {
            k_event_set_masked(&data->thread.event, 0, DAP_THREAD_EVENT_READ_READY);
            /* the transport at this time has made sure that we should run the commands available
             * within the buffer (i.e. the last request wasn't a DAP_COMMAND_QUEUE_COMMANDS) */
            if (dap_handle_request(dev) < 0) {
                /* commands that failed or aren't implemented get a simple 0xff reponse byte, and we also
                 * reset the request buffer since they are probably in a bad state */
                dap_transport_buf_reset(dev);
                uint8_t response = DAP_COMMAND_RESPONSE_ERROR;
                FATAL_CHECK(ring_buf_put(&data->buf.response, &response, 1) == 1, "response buf is size 0");
            }

            if (dap_transport_send(dev) < 0) {
                dap_reset(dev);
                continue;
            }

            /* wait for the response to be sent and the buffer to be free */
            uint32_t write_complete_event = k_event_wait(
                &data->thread.event,
                DAP_THREAD_EVENT_WRITE_COMPLETE | DAP_THREAD_EVENT_DISCONNECT,
                false,
                K_SECONDS(30)
            );
            if ((write_complete_event & DAP_THREAD_EVENT_DISCONNECT) != 0) {
                dap_reset(dev);
                continue;
            } else if ((write_complete_event & DAP_THREAD_EVENT_WRITE_COMPLETE) == 0) {
                LOG_ERR("transport send timed out");
                dap_reset(dev);
                continue;
            }
            k_event_set_masked(&data->thread.event, 0, DAP_THREAD_EVENT_WRITE_COMPLETE);

            /* ensure both buffers are clear before waiting on the next read, which allows us to always 
             * start a new request at the begging of the ring buffer (skipping the 'ring' part) */
            dap_transport_buf_reset(dev);
            /* restart the next read request, whether or not the previous one succeeded */
            if (dap_transport_recv_begin(dev) < 0) dap_reset(dev);
        }
    }
}

static int32_t dap_init(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    int32_t ret;

    /* determine whether we have shared or independent status led */
    if (config->led_connect_gpio.port == config->led_running_gpio.port &&
        config->led_connect_gpio.pin == config->led_running_gpio.pin) {
        data->led.combined = true;
    } else {
        data->led.combined = false;
    }
    /* if combined, the second call will have no affect */
    FATAL_CHECK(gpio_pin_configure_dt(&config->led_connect_gpio, GPIO_OUTPUT_INACTIVE) >= 0, "led config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->led_running_gpio, GPIO_OUTPUT_INACTIVE) >= 0, "led config failed");
    /* the running led will blink when in use, set up a timer to control this blinking */
    k_timer_init(&data->led.timer, handle_running_led_timer, NULL);
    k_timer_user_data_set(&data->led.timer, (void*) dev);

    /* vtref is only ever an input, doesn't need reconfiguration in dap_reset */
    FATAL_CHECK(gpio_pin_configure_dt(&config->vtref_gpio, GPIO_INPUT) >= 0, "vtref config failed");

    if (!device_is_ready(config->swo_uart_dev)) return -ENODEV;
    uart_irq_rx_disable(config->swo_uart_dev);
    uart_irq_tx_disable(config->swo_uart_dev);
    uart_irq_callback_user_data_set(config->swo_uart_dev, swo_uart_isr, (void*) dev);

    ring_buf_init(&data->buf.request, sizeof(data->buf.request_bytes), data->buf.request_bytes);
    ring_buf_init(&data->buf.response, sizeof(data->buf.response_bytes), data->buf.response_bytes);
    ring_buf_init(&data->buf.swo, sizeof(data->buf.swo_bytes), data->buf.swo_bytes);

    if ((ret = dap_tcp_init(dev)) < 0) return ret;
    if ((ret = dap_reset(dev)) < 0) return ret;

    /* threading initialization */
    k_event_init(&data->thread.event);
    k_thread_create(
        &data->thread.driver,
        dap_driver_thread_stack,
        K_THREAD_STACK_SIZEOF(dap_driver_thread_stack),
        dap_thread_fn,
        (void*) dev,
        NULL,
        NULL,
        CONFIG_MAIN_THREAD_PRIORITY + 1,
        0,
        K_NO_WAIT
    );
    k_thread_create(
        &data->thread.tcp,
        dap_tcp_thread_stack,
        K_THREAD_STACK_SIZEOF(dap_tcp_thread_stack),
        dap_tcp_thread_fn,
        (void*) dev,
        NULL,
        NULL,
        CONFIG_MAIN_THREAD_PRIORITY + 2,
        0,
        K_MSEC(10)
    );

    return 0;
}

PINCTRL_DT_DEFINE(DAP_DT_NODE);

struct dap_data dap_data;
const struct dap_config dap_config = {
    .tck_swclk_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, tck_swclk_gpios),
    .tms_swdio_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, tms_swdio_gpios),
    .tdo_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, tdo_gpios),
    .tdi_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, tdi_gpios),
    .nreset_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, nreset_gpios),
    .vtref_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, vtref_gpios),
    .led_connect_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, led_connect_gpios),
    .led_running_gpio = GPIO_DT_SPEC_GET(DAP_DT_NODE, led_running_gpios),
    .swo_uart_dev = DEVICE_DT_GET(DT_PHANDLE(DAP_DT_NODE, swo_uart)),
    .pinctrl_config = PINCTRL_DT_DEV_CONFIG_GET(DAP_DT_NODE),
};

DEVICE_DT_DEFINE(
    DAP_DT_NODE,
    dap_init,
    NULL,
    &dap_data,
    &dap_config,
    APPLICATION,
    40,
    NULL,
);
