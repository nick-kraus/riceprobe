#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

#define SWO_TRANSPORT_NONE          ((uint8_t) 0)
#define SWO_TRANSPORT_COMMAND       ((uint8_t) 1)
#define SWO_TRANSPORT_ENDPOINT      ((uint8_t) 2)

#define SWO_MODE_NONE               ((uint8_t) 0)
#define SWO_MODE_UART               ((uint8_t) 1)
#define SWO_MODE_MANCHESTER         ((uint8_t) 2)

void swo_capture_control(const struct device *dev, bool enable) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    if (enable) {
        data->swo.capture = true;
        data->swo.overrun = false;
        ring_buf_reset(config->swo_buf);
        uart_irq_err_enable(config->swo_uart_dev);
        uart_irq_rx_enable(config->swo_uart_dev);
    } else {
        uart_irq_rx_disable(config->swo_uart_dev);
        uart_irq_err_disable(config->swo_uart_dev);
        data->swo.capture = false;
        data->swo.error = false;
    }
}

int32_t dap_handle_command_swo_transport(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    uint8_t transport = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &transport, 1), 1, -EMSGSIZE);
    if (transport == SWO_TRANSPORT_ENDPOINT) {
        status = DAP_COMMAND_RESPONSE_ERROR;
    } else {
        data->swo.transport = transport;
    }

    uint8_t response[] = {DAP_COMMAND_SWO_TRANSPORT, status};
    CHECK_EQ(ring_buf_put(config->response_buf, response, 2), 2, -ENOBUFS);
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swo_mode(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    uint8_t mode = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &mode, 1), 1, -EMSGSIZE);

    /* only allow SWO to be initialized if the DAP port is SWD, and no support for manchester encoding */
    if (data->swj.port != DAP_PORT_SWD || mode >= SWO_MODE_MANCHESTER) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    }

    /* disable capture on the existing swo mode */
    if (data->swo.mode == SWO_MODE_UART) {
        swo_capture_control(dev, false);
    }
    data->swo.mode = mode;

    /* we don't actually need to perform anything else, since we only support the UART mode for SWO,
     * we always keep the uart driver enabled (but with capture disabled), and switch the tdo/swo
     * pinctrl to be UART when we configure the overal SWD port */

end: ;
    uint8_t response[] = {DAP_COMMAND_SWO_MODE, status};
    CHECK_EQ(ring_buf_put(config->response_buf, response, 2), 2, -ENOBUFS);
    return ring_buf_size_get(config->response_buf);
}   

int32_t dap_handle_command_swo_baudrate(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint32_t baudrate = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &baudrate, 4), 4, -EMSGSIZE);
    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_SWO_BAUDRATE}), 1), 1, -ENOBUFS);
    
    /* if currently in UART mode then re-configure the driver */
    if (data->swo.mode == SWO_MODE_UART) {
        struct uart_config uart_config = {
            .baudrate = baudrate,
            .parity = UART_CFG_PARITY_NONE,
            .stop_bits = UART_CFG_STOP_BITS_1,
            .data_bits = UART_CFG_DATA_BITS_8,
            .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
        };
        if (uart_configure(config->swo_uart_dev, &uart_config) == 0) {
            data->swo.baudrate = baudrate;
        } else {
            /* a rate of 0 indicates the baudrate was not configured */
            data->swo.baudrate = 0;
        }
    }

    CHECK_EQ(ring_buf_put(config->response_buf, (uint8_t*) &data->swo.baudrate, 4), 4, -ENOBUFS);
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swo_control(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    uint8_t control = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &control, 1), 1, -EMSGSIZE);

    /* only enable SWO data capture if the DAP port is SWD and the correct mode is configured */
    if (data->swj.port != DAP_PORT_SWD || (control == 1 && data->swo.mode != SWO_MODE_UART)) {
        status = DAP_COMMAND_RESPONSE_ERROR;
    } else {
        swo_capture_control(dev, control == 1 ? true : false);
    }

    uint8_t response[] = {DAP_COMMAND_SWO_CONTROL, status};
    CHECK_EQ(ring_buf_put(config->response_buf, response, 2), 2, -ENOBUFS);
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swo_status(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_SWO_STATUS}), 1), 1, -ENOBUFS);

    uint8_t trace_capture = data->swo.capture ? 0x01 : 0x00;
    uint8_t trace_error = data->swo.error ? 0x40 : 0x00;
    uint8_t trace_overrun = data->swo.overrun ? 0x80 : 0x00;
    uint8_t trace_status = trace_capture | trace_error | trace_overrun;
    CHECK_EQ(ring_buf_put(config->response_buf, &trace_status, 1), 1, -ENOBUFS);
    
    CHECK_EQ(ring_buf_put(config->response_buf, (uint8_t*) &data->swo.baudrate, 4), 4, -ENOBUFS);

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swo_extended_status(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint8_t control = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &control, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_SWO_EXTENDED_STATUS}), 1), 1, -ENOBUFS);

    if ((control & BIT(0)) != 0) {
        uint8_t trace_capture = data->swo.capture ? 0x01 : 0x00;
        uint8_t trace_error = data->swo.error ? 0x40 : 0x00;
        uint8_t trace_overrun = data->swo.overrun ? 0x80 : 0x00;
        uint8_t trace_status = trace_capture | trace_error | trace_overrun;
        CHECK_EQ(ring_buf_put(config->response_buf, &trace_status, 1), 1, -ENOBUFS);
    }
    
    if ((control & BIT(1)) != 0) {
        CHECK_EQ(ring_buf_put(config->response_buf, (uint8_t*) &data->swo.baudrate, 4), 4, -ENOBUFS);
    }

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_swo_data(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    uint16_t count = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &count, 2), 2, -EMSGSIZE);
    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_SWO_DATA}), 1), 1, -ENOBUFS);

    uint8_t trace_capture = data->swo.capture ? 0x01 : 0x00;
    uint8_t trace_error = data->swo.error ? 0x40 : 0x00;
    uint8_t trace_overrun = data->swo.overrun ? 0x80 : 0x00;
    uint8_t trace_status = trace_capture | trace_error | trace_overrun;
    CHECK_EQ(ring_buf_put(config->response_buf, &trace_status, 1), 1, -ENOBUFS);

    /* need a pointer to this item because we will write to it after finding out how much data to read */
    uint16_t *response_count = NULL;
    CHECK_EQ(ring_buf_put_claim(config->response_buf, (uint8_t**) &response_count, 2), 2, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_finish(config->response_buf, 2), 0, -ENOBUFS);

    /* we may need to process a claim multiple times, in case our copies overlap a ring buffer gap */
    do {
        uint8_t *swo_ptr;
        uint32_t swo_read = ring_buf_get_claim(config->swo_buf, &swo_ptr, count);
        uint8_t *response_ptr;
        uint32_t response_write = ring_buf_put_claim(config->response_buf, &response_ptr, swo_read);
        memcpy(response_ptr, swo_ptr, response_write);
        int32_t get_ret = ring_buf_get_finish(config->swo_buf, response_write);
        int32_t put_ret = ring_buf_put_finish(config->response_buf, response_write);
        if (get_ret != 0 || put_ret != 0) { return -ENOBUFS; }
        *response_count += response_write;
    } while (
        *response_count < count &&
        ring_buf_size_get(config->swo_buf) > 0 &&
        ring_buf_space_get(config->response_buf) > 0
    );

    return ring_buf_size_get(config->response_buf);
}
