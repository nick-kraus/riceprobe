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

void jtag_tck_cycle(struct dap_driver *dap);
void jtag_tdi_cycle(struct dap_driver *dap, uint8_t tdi);
uint8_t jtag_tdo_cycle(struct dap_driver *dap);
uint8_t jtag_tdio_cycle(struct dap_driver *dap, uint8_t tdi);
void jtag_set_ir(struct dap_driver *dap, uint32_t ir);

uint8_t swd_read_cycle(struct dap_driver *dap);
void swd_write_cycle(struct dap_driver *dap, uint8_t swdio);
void swd_swclk_cycle(struct dap_driver *dap);

int32_t dap_handle_command_transfer_configure(struct dap_driver *dap) {
    CHECK_EQ(ring_buf_get(&dap->buf.request, &dap->transfer.idle_cycles, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &dap->transfer.wait_retries, 2), 2, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &dap->transfer.match_retries, 2), 2, -EMSGSIZE);

    uint8_t response[] = {DAP_COMMAND_TRANSFER_CONFIGURE, DAP_COMMAND_RESPONSE_OK};
    CHECK_EQ(ring_buf_put(&dap->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}

uint8_t jtag_transfer(struct dap_driver *dap, uint8_t request, uint32_t *transfer_data) {
    /* assumes we are starting in idle tap state, move to select-dr-scan */
    gpio_pin_set_dt(&dap->io.tms_swdio, 1);
    jtag_tck_cycle(dap);

    /* capture-dr, then shift-dr */
    gpio_pin_set_dt(&dap->io.tms_swdio, 0);
    jtag_tck_cycle(dap);
    jtag_tck_cycle(dap);

    /* bypass for every tap before the current index */
    for (uint8_t i = 0; i < dap->jtag.index; i++) {
        jtag_tck_cycle(dap);
    }

    /* set RnW, A2, and A3, and get previous ack[0..2]. ack[0] and ack[1] are swapped here
     * because the bottom two bits of the JTAG ack response are flipped from the dap transfer
     * ack response (i.e. jtag ack ok/fault = 0x2, dap ack ok/fault = 0x1) */
    uint8_t ack = 0;
    ack |= jtag_tdio_cycle(dap, request >> TRANSFER_REQUEST_RnW_SHIFT) << 1;
    ack |= jtag_tdio_cycle(dap, request >> TRANSFER_REQUEST_A2_SHIFT) << 0;
    ack |= jtag_tdio_cycle(dap, request >> TRANSFER_REQUEST_A3_SHIFT) << 2;

    if (ack != TRANSFER_RESPONSE_ACK_OK) {
        /* exit-1-dr */
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        jtag_tck_cycle(dap);
        goto end;
    }

    uint32_t dr = 0;
    if ((request & TRANSFER_REQUEST_RnW) != 0) {
        /* get bits 0..30 */
        for (uint8_t i = 0; i < 31; i++) {
            dr |= jtag_tdo_cycle(dap) << i;
        }

        uint8_t after_index = dap->jtag.count - dap->jtag.index - 1;
        if (after_index > 0) {
            /* get bit 31, then bypass after index */
            dr |= jtag_tdo_cycle(dap) << 31;
            for (uint8_t i = 0; i < after_index - 1; i++) {
                jtag_tck_cycle(dap);
            }
            /* bypass, then exit-1-dr */
            gpio_pin_set_dt(&dap->io.tms_swdio, 1);
            jtag_tck_cycle(dap);
        } else {
            /* get bit 31, then exit-1-dr */
            gpio_pin_set_dt(&dap->io.tms_swdio, 1);
            dr |= jtag_tdo_cycle(dap) << 31;
        }

        *transfer_data = dr;
    } else {
        dr = *transfer_data;

        /* set bits 0..30 */
        for (uint8_t i = 0; i < 31; i++) {
            jtag_tdi_cycle(dap, dr);
            dr >>= 1;
        }

        uint8_t after_index = dap->jtag.count - dap->jtag.index - 1;
        if (after_index > 0) {
            /* set bit 31, then bypass after index */
            jtag_tdi_cycle(dap, dr);
            for (uint8_t i = 0; i < after_index - 1; i++) {
                jtag_tck_cycle(dap);
            }
            /* bypass, then exit-1-dr */
            gpio_pin_set_dt(&dap->io.tms_swdio, 1);
            jtag_tck_cycle(dap);
        } else {
            /* set bit 31, then exit-1-dr */
            gpio_pin_set_dt(&dap->io.tms_swdio, 1);
            jtag_tdi_cycle(dap, dr);
        }
    }

end:
    /* update-dr, then idle */
    jtag_tck_cycle(dap);
    gpio_pin_set_dt(&dap->io.tms_swdio, 0);
    jtag_tck_cycle(dap);
    gpio_pin_set_dt(&dap->io.tdi, 1);

    /* idle for configured cycles */
    for (uint8_t i = 0; i < dap->transfer.idle_cycles; i++) {
        jtag_tck_cycle(dap);
    }

    return ack;
}

uint8_t swd_transfer(struct dap_driver *dap, uint8_t request, uint32_t *transfer_data) {
    /* 8-bit packet request */
    uint8_t ap_ndp = request >> TRANSFER_REQUEST_APnDP_SHIFT;
    uint8_t r_nw = request >> TRANSFER_REQUEST_RnW_SHIFT;
    uint8_t a2 = request >> TRANSFER_REQUEST_A2_SHIFT;
    uint8_t a3 = request >> TRANSFER_REQUEST_A3_SHIFT;
    uint32_t parity = ap_ndp + r_nw + a2 + a3;
    /* start bit */
    swd_write_cycle(dap, 1);
    swd_write_cycle(dap, ap_ndp);
    swd_write_cycle(dap, r_nw);
    swd_write_cycle(dap, a2);
    swd_write_cycle(dap, a3);
    swd_write_cycle(dap, parity);
    /* stop then park bits */
    swd_write_cycle(dap, 0);
    swd_write_cycle(dap, 1);

    /* turnaround bits */
    FATAL_CHECK(
        gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT) >= 0,
        "tms swdio config failed"
    );
    for (uint8_t i = 0; i < dap->swd.turnaround_cycles; i++) {
        swd_swclk_cycle(dap);
    }

    /* acknowledge bits */
    uint8_t ack = 0;
    ack |= swd_read_cycle(dap) << 0;
    ack |= swd_read_cycle(dap) << 1;
    ack |= swd_read_cycle(dap) << 2;

    if (ack == TRANSFER_RESPONSE_ACK_OK) {
        if ((request & TRANSFER_REQUEST_RnW) != 0) {
            /* read data */
            uint32_t read = 0;
            parity = 0;
            for (uint8_t i = 0; i < 32; i++) {
                uint8_t bit = swd_read_cycle(dap);
                read |= bit << i;
                parity += bit;
            }
            uint8_t parity_bit = swd_read_cycle(dap);
            if ((parity & 0x01) != parity_bit) {
                ack = TRANSFER_RESPONSE_ERROR;
            }
            *transfer_data = read;
            /* turnaround bits */
            for (uint8_t i = 0; i < dap->swd.turnaround_cycles; i++) {
                swd_swclk_cycle(dap);
            }
            FATAL_CHECK(
                gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
        } else {
            /* turnaround bits */
            for (uint8_t i = 0; i < dap->swd.turnaround_cycles; i++) {
                swd_swclk_cycle(dap);
            }
            FATAL_CHECK(
                gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
                "tms swdio config failed"
            );
            /* write data */
            uint32_t write = *transfer_data;
            parity = 0;
            for (uint8_t i = 0; i < 32; i++) {
                swd_write_cycle(dap, (uint8_t) write);
                parity += write;
                write >>= 1;
            }
            swd_write_cycle(dap, (uint8_t) parity);
        }
        /* idle cycles */
        for (uint8_t i = 0; i < dap->transfer.idle_cycles; i++) {
            swd_write_cycle(dap, 0);
        }
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        return ack;
    } else if (ack == TRANSFER_RESPONSE_ACK_WAIT || ack == TRANSFER_RESPONSE_FAULT) {
        if (dap->swd.data_phase && (request & TRANSFER_REQUEST_RnW) != 0) {
            /* dummy read through 32 bits and parity */
            for (uint8_t i = 0; i < 33; i++) {
                swd_swclk_cycle(dap);
            }
        }
        /* turnaround bits */
        for (uint8_t i = 0; i < dap->swd.turnaround_cycles; i++) {
            swd_swclk_cycle(dap);
        }
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
        if (dap->swd.data_phase && (request & TRANSFER_REQUEST_RnW) == 0) {
            gpio_pin_set_dt(&dap->io.tms_swdio, 0);
            /* dummy write through 32 bits and parity */
            for (uint8_t i = 0; i < 33; i++) {
                swd_swclk_cycle(dap);
            }
        }
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        return ack;
    } else {
        /* dummy read through turnaround bits, 32 bits and parity */
        for (uint8_t i = 0; i < dap->swd.turnaround_cycles + 33; i++) {
            swd_swclk_cycle(dap);
        }
        FATAL_CHECK(
            gpio_pin_configure_dt(&dap->io.tms_swdio, GPIO_INPUT | GPIO_OUTPUT) >= 0,
            "tms swdio config failed"
        );
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        return ack;
    }
}

static inline uint8_t port_transfer(struct dap_driver *dap, uint8_t request, uint32_t *transfer_data) {
    uint8_t transfer_ack = 0;
    for (uint32_t i = 0; i < dap->transfer.wait_retries + 1; i++) {
        if (dap->swj.port == DAP_PORT_JTAG) {
            transfer_ack = jtag_transfer(dap, request, transfer_data);
        } else if (dap->swj.port == DAP_PORT_SWD) {
            transfer_ack = swd_transfer(dap, request, transfer_data);
        } else {
            return TRANSFER_RESPONSE_FAULT;
        }
        if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
    }

    return transfer_ack;
}

static inline void port_set_ir(struct dap_driver *dap, uint32_t *last_ir, uint32_t desired_ir) {
    if (dap->swj.port == DAP_PORT_JTAG && (last_ir == NULL || *last_ir != desired_ir)) {
        if (last_ir != NULL) {
            *last_ir = desired_ir;
        }
        jtag_set_ir(dap, desired_ir);
    }
}

int32_t dap_handle_command_transfer(struct dap_driver *dap) {
    CHECK_EQ(ring_buf_put(&dap->buf.response, &((uint8_t) {DAP_COMMAND_TRANSFER}), 1), 1, -ENOBUFS);
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint8_t *response_count = NULL;
    uint8_t *response_response = NULL;
    CHECK_EQ(ring_buf_put_claim(&dap->buf.response, &response_count, 1), 1, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_claim(&dap->buf.response, &response_response, 1), 1, -ENOBUFS);
    *response_response = 0;
    CHECK_EQ(ring_buf_put_finish(&dap->buf.response, 2), 0, -ENOBUFS);

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
    CHECK_EQ(ring_buf_get(&dap->buf.request, &index, 1), 1, -EMSGSIZE);
    /* number of transfers */
    uint8_t count = 0;
    CHECK_EQ(ring_buf_get(&dap->buf.request, &count, 1), 1, -EMSGSIZE);

    if (dap->swj.port == DAP_PORT_DISABLED) {
        goto end;
    } else if (dap->swj.port == DAP_PORT_JTAG) {
        if (index >= dap->jtag.count) {
            goto end;
        }
        dap->jtag.index = index;
    }

    while (count > 0) {
        uint8_t request = 0;
        CHECK_EQ(ring_buf_get(&dap->buf.request, &request, 1), 1, -EMSGSIZE);
        uint32_t request_ir = (request & TRANSFER_REQUEST_APnDP) ? JTAG_IR_APACC : JTAG_IR_DPACC;
        /* make sure to pull all request data before decrementing count, so that we don't miss
         * request bytes when processing cancelled requests */
        if ((request & TRANSFER_REQUEST_RnW) == 0 ||
            (request & TRANSFER_REQUEST_MATCH_VALUE) != 0) {
            CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &transfer_data, 4), 4, -EMSGSIZE);
        }
        count--;

        /* TODO: for now we are just going to do the simple thing and read the previously posted value,
         * without posting the next read if available */
        if (read_pending) {
            port_set_ir(dap, &last_ir, JTAG_IR_DPACC);
            transfer_ack = port_transfer(dap, TRANSFER_REQUEST_RnW | DP_ADDR_RDBUFF , &transfer_data);
            read_pending = false;
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
            CHECK_EQ(ring_buf_put(&dap->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
        }

        if ((request & TRANSFER_REQUEST_RnW) != 0) {
            if ((request & TRANSFER_REQUEST_MATCH_VALUE) != 0) {
                /* read with match value */
                /* match value already stored in transfer_data, but shift it here to free up transfer_data */
                uint32_t match_value = transfer_data;
                port_set_ir(dap, &last_ir, request_ir);
                /* if using the SWD transport and reading from the DP, we don't need to post a read request
                 * first, it will be immediately available on the later read value, otherwise post first */
                if (dap->swj.port == DAP_PORT_JTAG || (request & TRANSFER_REQUEST_APnDP) != 0) {
                    /* post the read request */
                    transfer_ack = port_transfer(dap, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                }
                /* get and check read value */
                for (uint32_t i = 0; i < dap->transfer.match_retries + 1; i++) {
                    transfer_ack = port_transfer(dap, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_OK ||
                        (transfer_data & dap->transfer.match_mask) == match_value) {
                        break;
                    }
                }
                if ((transfer_data & dap->transfer.match_mask) != match_value) {
                    transfer_ack |= TRANSFER_RESPONSE_VALUE_MISMATCH;
                }
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
            } else {
                /* normal read request */
                port_set_ir(dap, &last_ir, request_ir);
                transfer_ack = port_transfer(dap, request, &transfer_data);
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                /* on SWD reads to DP there is no nead to post the read, the correct data has been received */
                if (dap->swj.port == DAP_PORT_SWD && (request & TRANSFER_REQUEST_APnDP) == 0) {
                    CHECK_EQ(ring_buf_put(&dap->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
                } else {
                    read_pending = true;
                }
            }
            ack_pending = false;
        } else {
            if ((request & TRANSFER_REQUEST_MATCH_MASK) != 0) {
                dap->transfer.match_mask = transfer_data;
                transfer_ack = TRANSFER_RESPONSE_ACK_OK;
            } else {
                /* normal write request */
                port_set_ir(dap, &last_ir, request_ir);
                transfer_ack = port_transfer(dap, request, &transfer_data);
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
        CHECK_EQ(ring_buf_get(&dap->buf.request, &request, 1), 1, -EMSGSIZE);
        /* write requests and read match value both have 4 bytes of request input */
        if (((request & TRANSFER_REQUEST_RnW) != 0 &&
            (request & TRANSFER_REQUEST_MATCH_VALUE) != 0) ||
            (request & TRANSFER_REQUEST_RnW) == 0) {
            
            uint32_t temp;
            CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &temp, 4), 4, -EMSGSIZE);
        }
    }

    /* perform final read to get last transfer ack and collect pending data if needed */
    if (transfer_ack == TRANSFER_RESPONSE_ACK_OK && (read_pending || ack_pending)) {
        port_set_ir(dap, &last_ir, JTAG_IR_DPACC);
        transfer_ack = port_transfer(dap, DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW, &transfer_data);
        if (transfer_ack == TRANSFER_RESPONSE_ACK_OK && read_pending) {
            CHECK_EQ(ring_buf_put(&dap->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
        }

        *response_response = transfer_ack;
    }

    return 0;
}

int32_t dap_handle_command_transfer_block(struct dap_driver *dap) {
    CHECK_EQ(ring_buf_put(&dap->buf.response, &((uint8_t) {DAP_COMMAND_TRANSFER_BLOCK}), 1), 1, -ENOBUFS);
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint16_t *response_count = NULL;
    uint8_t *response_response = NULL;
    CHECK_EQ(ring_buf_put_claim(&dap->buf.response, (uint8_t**) &response_count, 2), 2, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_claim(&dap->buf.response, &response_response, 1), 1, -ENOBUFS);
    *response_response = 0;
    CHECK_EQ(ring_buf_put_finish(&dap->buf.response, 3), 0, -ENOBUFS);

    /* transfer acknowledge and data storage */
    uint8_t transfer_ack = 0;
    uint32_t transfer_data = 0;
    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(&dap->buf.request, &index, 1), 1, -EMSGSIZE);
    /* number of words transferred */
    uint16_t count = 0;
    CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &count, 2), 2, -EMSGSIZE);
    /* transfer request metadata */
    uint8_t request = 0;
    CHECK_EQ(ring_buf_get(&dap->buf.request, &request, 1), 1, -EMSGSIZE);

    if (dap->swj.port == DAP_PORT_DISABLED) {
        goto end;
    } else if (dap->swj.port == DAP_PORT_JTAG) {
        if (index >= dap->jtag.count) {
            goto end;
        }
        dap->jtag.index = index;
    }

    uint32_t request_ir = (request & TRANSFER_REQUEST_APnDP) ? JTAG_IR_APACC : JTAG_IR_DPACC;
    port_set_ir(dap, NULL, request_ir);

    if ((request & TRANSFER_REQUEST_RnW) != 0) {
        /* for JTAG transfers and SWD transfers to the AP, we must first post the read request */
        if (dap->swj.port == DAP_PORT_JTAG || (request & TRANSFER_REQUEST_APnDP) != 0) {
            transfer_ack = port_transfer(dap, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
        }

        while (count > 0) {
            count--;

            /* for JTAG transfers and SWD transfers to the AP, the final read should be to DP RDBUFF
             * so we don't post any further transactions */
            if (count == 0) {
                if (dap->swj.port == DAP_PORT_JTAG || (request & TRANSFER_REQUEST_APnDP) != 0) {
                    port_set_ir(dap, &request_ir, JTAG_IR_DPACC);
                    request = DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW;
                }
            }

            transfer_ack = port_transfer(dap, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
            CHECK_EQ(ring_buf_put(&dap->buf.response, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
            *response_count += 1;
        }
    } else {
        /* write transfer */
        while (count > 0) {
            count--;

            CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &transfer_data, 4), 4, -EMSGSIZE);
            transfer_ack = port_transfer(dap, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
            *response_count += 1;
        }
        /* get ack of last write */
        port_set_ir(dap, &request_ir, JTAG_IR_DPACC);
        request = DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW;
        transfer_ack = port_transfer(dap, request, &transfer_data);
    }
    *response_response = transfer_ack;

end:
    /* process remaining (canceled) request bytes */
    if (count > 0 && (request & TRANSFER_REQUEST_RnW) == 0) {
        uint8_t *temp = NULL;
        uint16_t request_remaining = count * 4;
        CHECK_EQ(ring_buf_get_claim(&dap->buf.request, &temp, request_remaining), request_remaining, -EMSGSIZE);
        CHECK_EQ(ring_buf_get_finish(&dap->buf.request, request_remaining), 0, -EMSGSIZE);
    }

    return 0;
}

int32_t dap_handle_command_transfer_abort(struct dap_driver *dap) {
    /* TODO: eventually we should separate reading data from the transport from replying to the requests,
     * so that we can actually scan for the abort request and cancel an in-progress transfer */

    return 0;
}

int32_t dap_handle_command_write_abort(struct dap_driver *dap) {
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(&dap->buf.request, &index, 1), 1, -EMSGSIZE);
    /* value to write to the abort register */
    uint32_t abort = 0;
    CHECK_EQ(ring_buf_get(&dap->buf.request, (uint8_t*) &abort, 4), 4, -EMSGSIZE);

    if (dap->swj.port == DAP_PORT_DISABLED) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    } else if (dap->swj.port == DAP_PORT_JTAG) {
        if (index >= dap->jtag.count) {
            status = DAP_COMMAND_RESPONSE_ERROR;
            goto end;
        }
        dap->jtag.index = index;
    }

    port_set_ir(dap, NULL, JTAG_IR_ABORT);
    /* DP write, address 0x0, */
    port_transfer(dap, 0x00, &abort);

end: ;
    uint8_t response[] = {DAP_COMMAND_WRITE_ABORT, status};
    CHECK_EQ(ring_buf_put(&dap->buf.response, response, 2), 2, -ENOBUFS);
    return 0;
}
