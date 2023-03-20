#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

/* jtag ir instructions */
#define JTAG_IR_ABORT                       ((uint8_t) 0x08)
#define JTAG_IR_DPACC                       ((uint8_t) 0x0a)
#define JTAG_IR_APACC                       ((uint8_t) 0x0b)

/* debug port addresses */
#define DP_ADDR_RDBUFF                      ((uint8_t) 0x0c)

/* dap transfer request bits */
#define TRANSFER_REQUEST_APnDP              ((uint8_t) 0x01)
#define TRANSFER_REQUEST_RnW                ((uint8_t) 0x02)
#define TRANSFER_REQUEST_MATCH_VALUE        ((uint8_t) 0x10)
#define TRANSFER_REQUEST_MATCH_MASK         ((uint8_t) 0x20)

#define TRANSFER_REQUEST_APnDP_SHIFT        ((uint8_t) 0x00)
#define TRANSFER_REQUEST_RnW_SHIFT          ((uint8_t) 0x01)
#define TRANSFER_REQUEST_A2_SHIFT           ((uint8_t) 0x02)
#define TRANSFER_REQUEST_A3_SHIFT           ((uint8_t) 0x03)

/* dap transfer response bits */
#define TRANSFER_RESPONSE_ACK_OK            ((uint8_t) 0x01)
#define TRANSFER_RESPONSE_ACK_WAIT          ((uint8_t) 0x02)
#define TRANSFER_RESPONSE_FAULT             ((uint8_t) 0x04)
#define TRANSFER_RESPONSE_ERROR             ((uint8_t) 0x08)
#define TRANSFER_RESPONSE_VALUE_MISMATCH    ((uint8_t) 0x10)

void jtag_tck_cycle(const struct device *dev);
void jtag_tdi_cycle(const struct device *dev, uint8_t tdi);
uint8_t jtag_tdo_cycle(const struct device *dev);
uint8_t jtag_tdio_cycle(const struct device *dev, uint8_t tdi);
void jtag_set_ir(const struct device *dev, uint32_t ir);

uint8_t swd_read_cycle(const struct device *dev);
void swd_write_cycle(const struct device *dev, uint8_t swdio);
void swd_swclk_cycle(const struct device *dev);

int32_t dap_handle_command_transfer_configure(const struct device *dev) {
    struct dap_data *data = dev->data;

    CHECK_EQ(ring_buf_get(&data->buf.request, &data->transfer.idle_cycles, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &data->transfer.wait_retries, 2), 2, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &data->transfer.match_retries, 2), 2, -EMSGSIZE);

    uint8_t response[] = {DAP_COMMAND_TRANSFER_CONFIGURE, DAP_COMMAND_RESPONSE_OK};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

uint8_t jtag_transfer(const struct device *dev, uint8_t request, uint32_t *transfer_data) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* assumes we are starting in idle tap state, move to select-dr-scan */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    jtag_tck_cycle(dev);

    /* capture-dr, then shift-dr */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* bypass for every tap before the current index */
    for (uint8_t i = 0; i < data->jtag.index; i++) {
        jtag_tck_cycle(dev);
    }

    /* set RnW, A2, and A3, and get previous ack[0..2]. ack[0] and ack[1] are swapped here
     * because the bottom two bits of the JTAG ack response are flipped from the dap transfer
     * ack response (i.e. jtag ack ok/fault = 0x2, dap ack ok/fault = 0x1) */
    uint8_t ack = 0;
    ack |= jtag_tdio_cycle(dev, request >> TRANSFER_REQUEST_RnW_SHIFT) << 1;
    ack |= jtag_tdio_cycle(dev, request >> TRANSFER_REQUEST_A2_SHIFT) << 0;
    ack |= jtag_tdio_cycle(dev, request >> TRANSFER_REQUEST_A3_SHIFT) << 2;

    if (ack != TRANSFER_RESPONSE_ACK_OK) {
        /* exit-1-dr */
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        jtag_tck_cycle(dev);
        goto end;
    }

    uint32_t dr = 0;
    if ((request & TRANSFER_REQUEST_RnW) != 0) {
        /* get bits 0..30 */
        for (uint8_t i = 0; i < 31; i++) {
            dr |= jtag_tdo_cycle(dev) << i;
        }

        uint8_t after_index = data->jtag.count - data->jtag.index - 1;
        if (after_index > 0) {
            /* get bit 31, then bypass after index */
            dr |= jtag_tdo_cycle(dev) << 31;
            for (uint8_t i = 0; i < after_index - 1; i++) {
                jtag_tck_cycle(dev);
            }
            /* bypass, then exit-1-dr */
            gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
            jtag_tck_cycle(dev);
        } else {
            /* get bit 31, then exit-1-dr */
            gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
            dr |= jtag_tdo_cycle(dev) << 31;
        }

        *transfer_data = dr;
    } else {
        dr = *transfer_data;

        /* set bits 0..30 */
        for (uint8_t i = 0; i < 31; i++) {
            jtag_tdi_cycle(dev, dr);
            dr >>= 1;
        }

        uint8_t after_index = data->jtag.count - data->jtag.index - 1;
        if (after_index > 0) {
            /* set bit 31, then bypass after index */
            jtag_tdi_cycle(dev, dr);
            for (uint8_t i = 0; i < after_index - 1; i++) {
                jtag_tck_cycle(dev);
            }
            /* bypass, then exit-1-dr */
            gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
            jtag_tck_cycle(dev);
        } else {
            /* set bit 31, then exit-1-dr */
            gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
            jtag_tdi_cycle(dev, dr);
        }
    }

end:
    /* update-dr, then idle */
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tdi_gpio, 1);

    /* idle for configured cycles */
    for (uint8_t i = 0; i < data->transfer.idle_cycles; i++) {
        jtag_tck_cycle(dev);
    }

    return ack;
}

uint8_t swd_transfer(const struct device *dev, uint8_t request, uint32_t *transfer_data) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* 8-bit packet request */
    uint8_t ap_ndp = request >> TRANSFER_REQUEST_APnDP_SHIFT;
    uint8_t r_nw = request >> TRANSFER_REQUEST_RnW_SHIFT;
    uint8_t a2 = request >> TRANSFER_REQUEST_A2_SHIFT;
    uint8_t a3 = request >> TRANSFER_REQUEST_A3_SHIFT;
    uint32_t parity = ap_ndp + r_nw + a2 + a3;
    /* start bit */
    swd_write_cycle(dev, 1);
    swd_write_cycle(dev, ap_ndp);
    swd_write_cycle(dev, r_nw);
    swd_write_cycle(dev, a2);
    swd_write_cycle(dev, a3);
    swd_write_cycle(dev, parity);
    /* stop then park bits */
    swd_write_cycle(dev, 0);
    swd_write_cycle(dev, 1);

    /* turnaround bits */
    FATAL_CHECK(
        gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT) >= 0,
        "tms swdio config failed"
    );
    for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
        swd_swclk_cycle(dev);
    }

    /* acknowledge bits */
    uint8_t ack = 0;
    ack |= swd_read_cycle(dev) << 0;
    ack |= swd_read_cycle(dev) << 1;
    ack |= swd_read_cycle(dev) << 2;

    if (ack == TRANSFER_RESPONSE_ACK_OK) {
        if ((request & TRANSFER_REQUEST_RnW) != 0) {
            /* read data */
            uint32_t read = 0;
            parity = 0;
            for (uint8_t i = 0; i < 32; i++) {
                uint8_t bit = swd_read_cycle(dev);
                read |= bit << i;
                parity += bit;
            }
            uint8_t parity_bit = swd_read_cycle(dev);
            if ((parity & 0x01) != parity_bit) {
                ack = TRANSFER_RESPONSE_ERROR;
            }
            *transfer_data = read;
            /* turnaround bits */
            for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
                swd_swclk_cycle(dev);
            }
            FATAL_CHECK(
                gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
        } else {
            /* turnaround bits */
            for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
                swd_swclk_cycle(dev);
            }
            FATAL_CHECK(
                gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
            /* write data */
            uint32_t write = *transfer_data;
            parity = 0;
            for (uint8_t i = 0; i < 32; i++) {
                swd_write_cycle(dev, (uint8_t) write);
                parity += write;
                write >>= 1;
            }
            swd_write_cycle(dev, (uint8_t) parity);
        }
        /* idle cycles */
        for (uint8_t i = 0; i < data->transfer.idle_cycles; i++) {
            swd_write_cycle(dev, 0);
        }
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        return ack;
    } else if (ack == TRANSFER_RESPONSE_ACK_WAIT || ack == TRANSFER_RESPONSE_FAULT) {
        if (data->swd.data_phase && (request & TRANSFER_REQUEST_RnW) != 0) {
            /* dummy read through 32 bits and parity */
            for (uint8_t i = 0; i < 33; i++) {
                swd_swclk_cycle(dev);
            }
        }
        /* turnaround bits */
        for (uint8_t i = 0; i < data->swd.turnaround_cycles; i++) {
            swd_swclk_cycle(dev);
        }
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
        if (data->swd.data_phase && (request & TRANSFER_REQUEST_RnW) == 0) {
            gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
            /* dummy write through 32 bits and parity */
            for (uint8_t i = 0; i < 33; i++) {
                swd_swclk_cycle(dev);
            }
        }
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        return ack;
    } else {
        /* dummy read through turnaround bits, 32 bits and parity */
        for (uint8_t i = 0; i < data->swd.turnaround_cycles + 33; i++) {
            swd_swclk_cycle(dev);
        }
        FATAL_CHECK(
            gpio_pin_configure_dt(&config->tms_swdio_gpio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        return ack;
    }
}

static inline uint8_t port_transfer(const struct device *dev, uint8_t request, uint32_t *transfer_data) {
    struct dap_data *data = dev->data;

    uint8_t transfer_ack = 0;
    for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
        if (data->swj.port == DAP_PORT_JTAG) {
            transfer_ack = jtag_transfer(dev, request, transfer_data);
        } else if (data->swj.port == DAP_PORT_SWD) {
            transfer_ack = swd_transfer(dev, request, transfer_data);
        } else {
            return TRANSFER_RESPONSE_FAULT;
        }
        if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
    }

    return transfer_ack;
}

static inline void port_set_ir(const struct device *dev, uint32_t *last_ir, uint32_t desired_ir) {
    struct dap_data *data = dev->data;

    if (data->swj.port == DAP_PORT_JTAG && (last_ir == NULL || *last_ir != desired_ir)) {
        if (last_ir != NULL) {
            *last_ir = desired_ir;
        }
        jtag_set_ir(dev, desired_ir);
    }
}

int32_t dap_handle_command_transfer(const struct device *dev) {
    struct dap_data *data = dev->data;

    CHECK_EQ(ring_buf_put(&data->buf.response, &((uint8_t) {DAP_COMMAND_TRANSFER}), 1), 1, -ENOBUFS);
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint8_t *response_count = NULL;
    uint8_t *response_response = NULL;
    CHECK_EQ(ring_buf_put_claim(&data->buf.response, &response_count, 1), 1, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_claim(&data->buf.response, &response_response, 1), 1, -ENOBUFS);
    *response_response = 0;
    CHECK_EQ(ring_buf_put_finish(&data->buf.response, 2), 0, -ENOBUFS);

    /* transfer acknowledge and data storage */
    uint8_t transfer_ack = 0;
    uint32_t transfer_data = 0;
    /* cache current ir value, only change tap ir value when needed */
    uint32_t last_ir = 0;
    /* set after a read request is made, to capture data on the next transfer (or at end) */
    bool read_pending = false;
    bool ack_pending = false;
    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &index, 1), 1, -EMSGSIZE);
    /* number of transfers */
    uint8_t count = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &count, 1), 1, -EMSGSIZE);

    if (data->swj.port == DAP_PORT_DISABLED) {
        goto end;
    } else if (data->swj.port == DAP_PORT_JTAG) {
        if (index >= data->jtag.count) {
            goto end;
        }
        data->jtag.index = index;
    }

    while (count > 0) {
        uint8_t request = 0;
        CHECK_EQ(ring_buf_get(&data->buf.request, &request, 1), 1, -EMSGSIZE);
        uint32_t request_ir = (request & TRANSFER_REQUEST_APnDP) ? JTAG_IR_APACC : JTAG_IR_DPACC;
        /* make sure to pull all request data before decrementing count, so that we don't miss
         * request bytes when processing cancelled requests */
        if ((request & TRANSFER_REQUEST_RnW) == 0 ||
            (request & TRANSFER_REQUEST_MATCH_VALUE) != 0) {
            CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &transfer_data, 4), 4, -EMSGSIZE);
        }
        count--;

        /* TODO: for now we are just going to do the simple thing and read the previously posted value,
         * without posting the next read if available */
        if (read_pending) {
            port_set_ir(dev, &last_ir, JTAG_IR_DPACC);
            transfer_ack = port_transfer(dev, TRANSFER_REQUEST_RnW | DP_ADDR_RDBUFF , &transfer_data);
            read_pending = false;
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
            CHECK_EQ(ring_buf_put(&data->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
        }

        if ((request & TRANSFER_REQUEST_RnW) != 0) {
            if ((request & TRANSFER_REQUEST_MATCH_VALUE) != 0) {
                /* read with match value */
                /* match value already stored in transfer_data, but shift it here to free up transfer_data */
                uint32_t match_value = transfer_data;
                port_set_ir(dev, &last_ir, request_ir);
                /* if using the SWD transport and reading from the DP, we don't need to post a read request
                 * first, it will be immediately available on the later read value, otherwise post first */
                if (data->swj.port == DAP_PORT_JTAG || (request & TRANSFER_REQUEST_APnDP) != 0) {
                    /* post the read request */
                    transfer_ack = port_transfer(dev, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                }
                /* get and check read value */
                for (uint32_t i = 0; i < data->transfer.match_retries + 1; i++) {
                    transfer_ack = port_transfer(dev, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_OK ||
                        (transfer_data & data->transfer.match_mask) == match_value) {
                        break;
                    }
                }
                if ((transfer_data & data->transfer.match_mask) != match_value) {
                    transfer_ack |= TRANSFER_RESPONSE_VALUE_MISMATCH;
                }
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
            } else {
                /* normal read request */
                port_set_ir(dev, &last_ir, request_ir);
                transfer_ack = port_transfer(dev, request, &transfer_data);
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                /* on SWD reads to DP there is no nead to post the read, the correct data has been received */
                if (data->swj.port == DAP_PORT_SWD && (request & TRANSFER_REQUEST_APnDP) == 0) {
                    CHECK_EQ(ring_buf_put(&data->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
                } else {
                    read_pending = true;
                }
            }
            ack_pending = false;
        } else {
            if ((request & TRANSFER_REQUEST_MATCH_MASK) != 0) {
                data->transfer.match_mask = transfer_data;
                transfer_ack = TRANSFER_RESPONSE_ACK_OK;
            } else {
                /* normal write request */
                port_set_ir(dev, &last_ir, request_ir);
                transfer_ack = port_transfer(dev, request, &transfer_data);
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                ack_pending = true;
            }
        }

        *response_count += 1;
    }
    *response_response = transfer_ack;

end:
    /* process remaining (canceled) request bytes */
    while (count > 0) {
        count--;

        uint8_t request = 0;
        CHECK_EQ(ring_buf_get(&data->buf.request, &request, 1), 1, -EMSGSIZE);
        /* write requests and read match value both have 4 bytes of request input */
        if (((request & TRANSFER_REQUEST_RnW) != 0 &&
            (request & TRANSFER_REQUEST_MATCH_VALUE) != 0) ||
            (request & TRANSFER_REQUEST_RnW) == 0) {
            
            uint32_t temp;
            CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &temp, 4), 4, -EMSGSIZE);
        }
    }

    /* perform final read to get last transfer ack and collect pending data if needed */
    if (transfer_ack == TRANSFER_RESPONSE_ACK_OK && (read_pending || ack_pending)) {
        port_set_ir(dev, &last_ir, JTAG_IR_DPACC);
        transfer_ack = port_transfer(dev, DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW, &transfer_data);
        if (transfer_ack == TRANSFER_RESPONSE_ACK_OK && read_pending) {
            CHECK_EQ(ring_buf_put(&data->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
        }

        *response_response = transfer_ack;
    }

    return 0;
}

int32_t dap_handle_command_transfer_block(const struct device *dev) {
    struct dap_data *data = dev->data;

    CHECK_EQ(ring_buf_put(&data->buf.response, &((uint8_t) {DAP_COMMAND_TRANSFER_BLOCK}), 1), 1, -ENOBUFS);
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint16_t *response_count = NULL;
    uint8_t *response_response = NULL;
    CHECK_EQ(ring_buf_put_claim(&data->buf.response, (uint8_t**) &response_count, 2), 2, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_claim(&data->buf.response, &response_response, 1), 1, -ENOBUFS);
    *response_response = 0;
    CHECK_EQ(ring_buf_put_finish(&data->buf.response, 3), 0, -ENOBUFS);

    /* transfer acknowledge and data storage */
    uint8_t transfer_ack = 0;
    uint32_t transfer_data = 0;
    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &index, 1), 1, -EMSGSIZE);
    /* number of words transferred */
    uint16_t count = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &count, 2), 2, -EMSGSIZE);
    /* transfer request metadata */
    uint8_t request = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &request, 1), 1, -EMSGSIZE);

    if (data->swj.port == DAP_PORT_DISABLED) {
        goto end;
    } else if (data->swj.port == DAP_PORT_JTAG) {
        if (index >= data->jtag.count) {
            goto end;
        }
        data->jtag.index = index;
    }

    uint32_t request_ir = (request & TRANSFER_REQUEST_APnDP) ? JTAG_IR_APACC : JTAG_IR_DPACC;
    port_set_ir(dev, NULL, request_ir);

    if ((request & TRANSFER_REQUEST_RnW) != 0) {
        /* for JTAG transfers and SWD transfers to the AP, we must first post the read request */
        if (data->swj.port == DAP_PORT_JTAG || (request & TRANSFER_REQUEST_APnDP) != 0) {
            transfer_ack = port_transfer(dev, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
        }

        while (count > 0) {
            count--;

            /* for JTAG transfers and SWD transfers to the AP, the final read should be to DP RDBUFF
             * so we don't post any further transactions */
            if (count == 0) {
                if (data->swj.port == DAP_PORT_JTAG || (request & TRANSFER_REQUEST_APnDP) != 0) {
                    port_set_ir(dev, &request_ir, JTAG_IR_DPACC);
                    request = DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW;
                }
            }

            transfer_ack = port_transfer(dev, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
            CHECK_EQ(ring_buf_put(&data->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
            *response_count += 1;
        }
    } else {
        /* write transfer */
        while (count > 0) {
            count--;

            CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &transfer_data, 4), 4, -EMSGSIZE);
            transfer_ack = port_transfer(dev, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
            *response_count += 1;
        }
        /* get ack of last write */
        port_set_ir(dev, &request_ir, JTAG_IR_DPACC);
        request = DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW;
        transfer_ack = port_transfer(dev, request, &transfer_data);
    }
    *response_response = transfer_ack;

end:
    /* process remaining (canceled) request bytes */
    if (count > 0 && (request & TRANSFER_REQUEST_RnW) == 0) {
        uint8_t *temp = NULL;
        uint16_t request_remaining = count * 4;
        CHECK_EQ(ring_buf_get_claim(&data->buf.request, &temp, request_remaining), request_remaining, -EMSGSIZE);
        CHECK_EQ(ring_buf_get_finish(&data->buf.request, request_remaining), 0, -EMSGSIZE);
    }

    return 0;
}

int32_t dap_handle_command_transfer_abort(const struct device *dev) {
    /* TODO: eventually we should separate reading data from the transport from replying to the requests,
     * so that we can actually scan for the abort request and cancel an in-progress transfer */

    return 0;
}

int32_t dap_handle_command_write_abort(const struct device *dev) {
    struct dap_data *data = dev->data;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, &index, 1), 1, -EMSGSIZE);
    /* value to write to the abort register */
    uint32_t abort = 0;
    CHECK_EQ(ring_buf_get(&data->buf.request, (uint8_t*) &abort, 4), 4, -EMSGSIZE);

    if (data->swj.port == DAP_PORT_DISABLED) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    } else if (data->swj.port == DAP_PORT_JTAG) {
        if (index >= data->jtag.count) {
            status = DAP_COMMAND_RESPONSE_ERROR;
            goto end;
        }
        data->jtag.index = index;
    }

    port_set_ir(dev, NULL, JTAG_IR_ABORT);
    /* DP write, address 0x0, */
    port_transfer(dev, 0x00, &abort);

end: ;
    uint8_t response[] = {DAP_COMMAND_WRITE_ABORT, status};
    CHECK_EQ(ring_buf_put(&data->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}
