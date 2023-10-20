#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

/* swo modes */
static const uint8_t swo_mode_uart = 0x01;
static const uint8_t swo_mode_manchester = 0x02;

void swo_capture_control(struct dap_driver *dap, bool enable) {
    if (enable) {
        dap->swo.capture = true;
        dap->swo.overrun = false;
        ring_buf_reset(&dap->buf.swo);
        uart_irq_err_enable(dap->io.swo_uart);
        uart_irq_rx_enable(dap->io.swo_uart);
    } else {
        uart_irq_rx_disable(dap->io.swo_uart);
        uart_irq_err_disable(dap->io.swo_uart);
        dap->swo.capture = false;
        dap->swo.error = false;
    }
}

int32_t dap_handle_cmd_swo_transport(struct dap_driver *dap) {
    /* swo transport options */
    const uint8_t swo_transport_none = 0x00;
    const uint8_t swo_transport_command = 0x01;

    uint8_t status = dap_cmd_response_ok;

    uint8_t transport = 0;
    if (ring_buf_get(&dap->buf.request, &transport, 1) != 1) return -EMSGSIZE;
    if (transport == swo_transport_command || transport == swo_transport_none) {
        dap->swo.transport = transport;
    } else {
        status = dap_cmd_response_error;
    }

    uint8_t response[] = {dap_cmd_swo_transport, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_swo_mode(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    uint8_t mode = 0;
    if (ring_buf_get(&dap->buf.request, &mode, 1) != 1) return -EMSGSIZE;

    /* only allow SWO to be initialized if the DAP port is SWD, and no support for manchester encoding */
    if (dap->swj.port != dap_port_swd || mode >= swo_mode_manchester) {
        status = dap_cmd_response_error;
        goto end;
    }

    /* disable capture on the existing swo mode */
    if (dap->swo.mode == swo_mode_uart) {
        swo_capture_control(dap, false);
    }
    dap->swo.mode = mode;

    /* we don't actually need to perform anything else, since we only support the UART mode for SWO,
     * we always keep the uart driver enabled (but with capture disabled), and switch the tdo/swo
     * pinctrl to be UART when we configure the overal SWD port */

end: ;
    uint8_t response[] = {dap_cmd_swo_mode, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}   

int32_t dap_handle_cmd_swo_baudrate(struct dap_driver *dap) {
    uint32_t baudrate = 0;
    if (ring_buf_get_le32(&dap->buf.request, &baudrate) < 0) return -EMSGSIZE;
    if (ring_buf_put(&dap->buf.response, &dap_cmd_swo_baudrate, 1) != 1) return -ENOBUFS;
    
    /* if currently in UART mode then re-configure the driver */
    if (dap->swo.mode == swo_mode_uart) {
        struct uart_config uart_config = {
            .baudrate = baudrate,
            .parity = UART_CFG_PARITY_NONE,
            .stop_bits = UART_CFG_STOP_BITS_1,
            .data_bits = UART_CFG_DATA_BITS_8,
            .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
        };
        if (uart_configure(dap->io.swo_uart, &uart_config) == 0) {
            dap->swo.baudrate = baudrate;
        } else {
            /* a rate of 0 indicates the baudrate was not configured */
            dap->swo.baudrate = 0;
        }
    }

    if (ring_buf_put_le32(&dap->buf.response, dap->swo.baudrate) < 0) return -ENOBUFS;

    return 0;
}

int32_t dap_handle_cmd_swo_control(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    uint8_t control = 0;
    if (ring_buf_get(&dap->buf.request, &control, 1) != 1) return -EMSGSIZE;

    /* only enable SWO data capture if the DAP port is SWD and the correct mode is configured */
    if (dap->swj.port != dap_port_swd || (control == 1 && dap->swo.mode != swo_mode_uart)) {
        status = dap_cmd_response_error;
    } else {
        swo_capture_control(dap, control == 1 ? true : false);
    }

    uint8_t response[] = {dap_cmd_swo_control, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_swo_status(struct dap_driver *dap) {
    if (ring_buf_put(&dap->buf.response, &dap_cmd_swo_status, 1) != 1) return -ENOBUFS;

    uint8_t trace_capture = dap->swo.capture ? 0x01 : 0x00;
    uint8_t trace_error = dap->swo.error ? 0x40 : 0x00;
    uint8_t trace_overrun = dap->swo.overrun ? 0x80 : 0x00;
    uint8_t trace_status = trace_capture | trace_error | trace_overrun;
    if (ring_buf_put(&dap->buf.response, &trace_status, 1) != 1) return -ENOBUFS;
    
    uint32_t trace_count = ring_buf_size_get(&dap->buf.swo);
    if (ring_buf_put_le32(&dap->buf.response, trace_count) < 0) return -ENOBUFS;

    return 0;
}

int32_t dap_handle_cmd_swo_extended_status(struct dap_driver *dap) {
    uint8_t control = 0;
    if (ring_buf_get(&dap->buf.request, &control, 1) != 1) return -EMSGSIZE;
    if (ring_buf_put(&dap->buf.response, &dap_cmd_swo_extended_status, 1) != 1) return -ENOBUFS;

    if ((control & BIT(0)) != 0) {
        uint8_t trace_capture = dap->swo.capture ? 0x01 : 0x00;
        uint8_t trace_error = dap->swo.error ? 0x40 : 0x00;
        uint8_t trace_overrun = dap->swo.overrun ? 0x80 : 0x00;
        uint8_t trace_status = trace_capture | trace_error | trace_overrun;
        if (ring_buf_put(&dap->buf.response, &trace_status, 1) != 1) return -ENOBUFS;
    }
    
    if ((control & BIT(1)) != 0) {
        uint32_t trace_count = ring_buf_size_get(&dap->buf.swo);
        if (ring_buf_put_le32(&dap->buf.response, trace_count) < 0) return -ENOBUFS;
    }

    return 0;
}

int32_t dap_handle_cmd_swo_data(struct dap_driver *dap) {
    uint16_t max_count = 0;
    if (ring_buf_get(&dap->buf.request, (uint8_t*) &max_count, 2) != 2) return -EMSGSIZE;
    if (ring_buf_put(&dap->buf.response, &dap_cmd_swo_data, 1) != 1) return -ENOBUFS;

    uint8_t trace_capture = dap->swo.capture ? 0x01 : 0x00;
    uint8_t trace_error = dap->swo.error ? 0x40 : 0x00;
    uint8_t trace_overrun = dap->swo.overrun ? 0x80 : 0x00;
    uint8_t trace_status = trace_capture | trace_error | trace_overrun;
    if (ring_buf_put(&dap->buf.response, &trace_status, 1) != 1) return -ENOBUFS;

    /* need a pointer to this item because we will write to it after finding out how much data to read */
    uint8_t *response_count_ptr = NULL;
    if (ring_buf_put_claim(&dap->buf.response, &response_count_ptr, 2) != 2) return -ENOBUFS;
    if (ring_buf_put_finish(&dap->buf.response, 2) < 0) return -ENOBUFS;

    /* actual count of bytes retreived from the SWO buffer */
    uint16_t count = 0;

    /* we may need to process a claim multiple times, in case our copies overlap a ring buffer gap */
    do {
        uint8_t *swo_ptr;
        uint32_t swo_read_size = ring_buf_get_claim(&dap->buf.swo, &swo_ptr, max_count);
        uint8_t *response_ptr;
        uint32_t response_write_size = ring_buf_put_claim(&dap->buf.response, &response_ptr, swo_read_size);
        memcpy(response_ptr, swo_ptr, response_write_size);
        int32_t get_ret = ring_buf_get_finish(&dap->buf.swo, response_write_size);
        int32_t put_ret = ring_buf_put_finish(&dap->buf.response, response_write_size);
        if (get_ret != 0 || put_ret != 0) { return -ENOBUFS; }
        count += (uint16_t) response_write_size;
    } while (
        count < max_count &&
        ring_buf_size_get(&dap->buf.swo) > 0 &&
        ring_buf_space_get(&dap->buf.response) > 0
    );

    sys_put_le16(count, response_count_ptr);
    return 0;
}
