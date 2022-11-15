#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/slist.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "dap/usb.h"
#include "util.h"

LOG_MODULE_REGISTER(dap, CONFIG_DAP_LOG_LEVEL);

int32_t dap_reset(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    LOG_INF("resetting driver state");

    /* jtag / swd gpios must be in a safe state on reset */
    FATAL_CHECK(gpio_pin_configure_dt(&config->tck_swclk_gpio, GPIO_INPUT) >= 0, "tck swclk config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0, "tms swdio config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdo_gpio, GPIO_INPUT) >= 0, "tdo config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->tdi_gpio, GPIO_INPUT) >= 0, "tdi config failed");
    FATAL_CHECK(gpio_pin_configure_dt(&config->nreset_gpio, GPIO_INPUT) >= 0, "nreset config failed");

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
    data->transfer.idle_cycles = 0;
    data->transfer.wait_retries = 100;
    data->transfer.match_retries = 0;
    data->transfer.match_mask = 0;

    ring_buf_reset(config->request_buf);
    ring_buf_reset(config->response_buf);

    data->led.connected = false;
    data->led.running = false;
    k_timer_stop(&data->led.timer);
    gpio_pin_set_dt(&config->led_connect_gpio, 0);
    gpio_pin_set_dt(&config->led_running_gpio, 0);

    return 0;
}

static int32_t dap_handle_single_request(const struct device *dev, uint8_t command) {
    switch (command) {
    case DAP_COMMAND_INFO:
        return dap_handle_command_info(dev);
    case DAP_COMMAND_HOST_STATUS:
        return dap_handle_command_host_status(dev);
    case DAP_COMMAND_CONNECT:
        return dap_handle_command_connect(dev);
    case DAP_COMMAND_DISCONNECT:
        return dap_handle_command_disconnect(dev);
    case DAP_COMMAND_TRANSFER_CONFIGURE:
        return dap_handle_command_transfer_configure(dev);
    case DAP_COMMAND_TRANSFER:
        return dap_handle_command_transfer(dev);
    case DAP_COMMAND_TRANSFER_BLOCK:
        return dap_handle_command_transfer_block(dev);
    case DAP_COMMAND_TRANSFER_ABORT:
        return dap_handle_command_transfer_abort(dev);
    case DAP_COMMAND_WRITE_ABORT:
        return dap_handle_command_write_abort(dev);
    case DAP_COMMAND_DELAY:
        return dap_handle_command_delay(dev);
    case DAP_COMMAND_RESET_TARGET:
        return dap_handle_command_reset_target(dev);
    case DAP_COMMAND_SWJ_PINS:
        return dap_handle_command_swj_pins(dev);
    case DAP_COMMAND_SWJ_CLOCK:
        return dap_handle_command_swj_clock(dev);
    case DAP_COMMAND_SWJ_SEQUENCE:
        return dap_handle_command_swj_sequence(dev);
    case DAP_COMMAND_SWD_CONFIGURE:
        return dap_handle_command_swd_configure(dev);
    case DAP_COMMAND_JTAG_SEQUENCE:
        return dap_handle_command_jtag_sequence(dev);
    case DAP_COMMAND_JTAG_CONFIGURE:
        return dap_handle_command_jtag_configure(dev);
    case DAP_COMMAND_JTAG_IDCODE:
        return dap_handle_command_jtag_idcode(dev);
    case DAP_COMMAND_SWO_TRANSPORT:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_SWO_MODE:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_SWO_BAUDRATE:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_SWO_CONTROL:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_SWO_STATUS:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_SWO_DATA:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_SWD_SEQUENCE:
        return dap_handle_command_swd_sequence(dev);
    case DAP_COMMAND_SWO_EXTENDED_STATUS:
        return -ENOTSUP; /* TODO */
    case DAP_COMMAND_UART_TRANSPORT:
    case DAP_COMMAND_UART_CONFIGURE:
    case DAP_COMMAND_UART_TRANSFER:
    case DAP_COMMAND_UART_CONTROL:
    case DAP_COMMAND_UART_STATUS:
        /* no intention on implementing the DAP UART commands, can just be interfaced over the
         * CDC-ACM virtual com port interface */
        return -ENOTSUP;
    case DAP_COMMAND_QUEUE_COMMANDS:
        return -ENOTSUP; /* TODO */
    default:
        LOG_ERR("unsupported command 0x%x", command);
        return -ENOTSUP;
    }
}

int32_t dap_handle_request(const struct device *dev) {
    const struct dap_config *config = dev->config;

    /* this request should not be handled before the previous response was transmitted, so
     * we can assume that the response buffer is safe to reset */
    ring_buf_reset(config->response_buf);

    /* this will usually just run once, unless an atomic command is being used */
    uint8_t num_commands = 1;
    do {
        /* data should be available at the front of the ring buffer before calling this handler */
        uint8_t command = 0xff;
        CHECK_EQ(ring_buf_get(config->request_buf, &command, 1), 1, -EMSGSIZE);

        if (command == DAP_COMMAND_EXECUTE_COMMANDS) {
            CHECK_EQ(ring_buf_get(config->request_buf, &num_commands, 1), 1, -EMSGSIZE);
            CHECK_EQ(ring_buf_put(config->response_buf, &command, 1), 1, -ENOBUFS);
            CHECK_EQ(ring_buf_put(config->response_buf, &num_commands, 1), 1, -ENOBUFS);
            /* get the next command for processing */
            CHECK_EQ(ring_buf_get(config->request_buf, &command, 1), 1, -EMSGSIZE);
        }

        int32_t size = dap_handle_single_request(dev, command);
        if (size < 0) {
            return size;
        } else {
            num_commands--;
        }
    } while (num_commands > 0);

    return ring_buf_size_get(config->response_buf);
}

static void handle_running_led_timer(struct k_timer *timer) {
    const struct device *dev = timer->user_data;
    const struct dap_config *config = dev->config;

    /* we choose to manually control the running led if the leds are combined,
     * so we don't need to check here, and can just do a simple toggle */
    gpio_pin_toggle_dt(&config->led_running_gpio);
}

sys_slist_t dap_devlist;

static int32_t dap_init(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    data->dev = dev;
    sys_slist_append(&dap_devlist, &data->devlist_node);

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

    return dap_reset(dev);
}

#define DT_DRV_COMPAT rice_dap

#define DAP_DT_DEVICE_DEFINE(idx)                                           \
                                                                            \
    static uint8_t dap_ep_buffer_##idx[DAP_BULK_EP_MPS];                    \
    RING_BUF_DECLARE(dap_request_buf_##idx, DAP_RING_BUF_SIZE);             \
    RING_BUF_DECLARE(dap_respones_buf_##idx, DAP_RING_BUF_SIZE);            \
                                                                            \
    DAP_USB_CONFIG_DEFINE(dap_usb_config_##idx, idx);                       \
                                                                            \
    struct dap_data dap_data_##idx;                                         \
    const struct dap_config dap_config_##idx = {                            \
        .ep_buf = dap_ep_buffer_##idx,                                      \
        .request_buf = &dap_request_buf_##idx,                              \
        .response_buf = &dap_respones_buf_##idx,                            \
        .usb_config = &dap_usb_config_##idx,                                \
        .tck_swclk_gpio = GPIO_DT_SPEC_INST_GET(idx, tck_swclk_gpios),      \
        .tms_swdio_gpio = GPIO_DT_SPEC_INST_GET(idx, tms_swdio_gpios),      \
        .tdo_gpio = GPIO_DT_SPEC_INST_GET(idx, tdo_gpios),                  \
        .tdi_gpio = GPIO_DT_SPEC_INST_GET(idx, tdi_gpios),                  \
        .nreset_gpio = GPIO_DT_SPEC_INST_GET(idx, nreset_gpios),            \
        .vtref_gpio = GPIO_DT_SPEC_INST_GET(idx, vtref_gpios),              \
        .led_connect_gpio = GPIO_DT_SPEC_INST_GET(idx, led_connect_gpios),  \
        .led_running_gpio = GPIO_DT_SPEC_INST_GET(idx, led_running_gpios),  \
    };                                                                      \
                                                                            \
    DEVICE_DT_INST_DEFINE(                                                  \
        idx,                                                                \
        dap_init,                                                           \
        NULL,                                                               \
        &dap_data_##idx,                                                    \
        &dap_config_##idx,                                                  \
        APPLICATION,                                                        \
        40,                                                                 \
        NULL,                                                               \
    );

DT_INST_FOREACH_STATUS_OKAY(DAP_DT_DEVICE_DEFINE);
