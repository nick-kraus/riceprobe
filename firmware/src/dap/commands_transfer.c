#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

/* jtag ir instructions */
static const uint8_t jtag_ir_abort = 0x08;
static const uint8_t jtag_ir_dpacc = 0x0a;
static const uint8_t jtag_ir_apacc = 0x0b;

/* debug port addresses */
static const uint8_t dp_addr_rdbuff = 0x0c;

/* dap transfer request bits */
static const uint8_t transfer_request_apndp = 0x01;
static const uint8_t transfer_request_rnw = 0x02;
static const uint8_t transfer_request_match_value = 0x10;
static const uint8_t transfer_request_match_mask = 0x20;

static const uint8_t transfer_request_apndp_shift = 0x00;
static const uint8_t transfer_request_rnw_shift = 0x01;
static const uint8_t transfer_request_a2_shift = 0x02;
static const uint8_t transfer_request_a3_shift = 0x03;

/* dap transfer response bits */
static const uint8_t transfer_response_ack_ok = 0x01;
static const uint8_t transfer_response_ack_wait = 0x02;
static const uint8_t transfer_response_fault = 0x04;
static const uint8_t transfer_response_error = 0x08;
static const uint8_t transfer_response_value_mismatch = 0x10;

int32_t dap_handle_cmd_transfer_configure(struct dap_driver *dap) {
    if (ring_buf_get(&dap->buf.request, &dap->transfer.idle_cycles, 1) != 1) return -EMSGSIZE;
    if (ring_buf_get_le16(&dap->buf.request, &dap->transfer.wait_retries) < 0) return -EMSGSIZE;
    if (ring_buf_get_le16(&dap->buf.request, &dap->transfer.match_retries) < 0) return -EMSGSIZE;

    uint8_t response[] = {dap_cmd_transfer_configure, dap_cmd_response_ok};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
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
    ack |= jtag_tdio_cycle(dap, request >> transfer_request_rnw_shift) << 1;
    ack |= jtag_tdio_cycle(dap, request >> transfer_request_a2_shift) << 0;
    ack |= jtag_tdio_cycle(dap, request >> transfer_request_a3_shift) << 2;

    if (ack != transfer_response_ack_ok) {
        /* exit-1-dr */
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        jtag_tck_cycle(dap);
        goto end;
    }

    uint32_t dr = 0;
    if ((request & transfer_request_rnw) != 0) {
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
    uint8_t ap_ndp = request >> transfer_request_apndp_shift;
    uint8_t r_nw = request >> transfer_request_rnw_shift;
    uint8_t a2 = request >> transfer_request_a2_shift;
    uint8_t a3 = request >> transfer_request_a3_shift;
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

    if (ack == transfer_response_ack_ok) {
        if ((request & transfer_request_rnw) != 0) {
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
                ack = transfer_response_error;
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
    } else if (ack == transfer_response_ack_wait || ack == transfer_response_fault) {
        if (dap->swd.data_phase && (request & transfer_request_rnw) != 0) {
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
        if (dap->swd.data_phase && (request & transfer_request_rnw) == 0) {
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
    uint8_t transfer_ack = transfer_response_fault;
    for (uint32_t i = 0; i < dap->transfer.wait_retries + 1; i++) {
        if (dap->swj.port == dap_port_jtag) {
            transfer_ack = jtag_transfer(dap, request, transfer_data);
        } else if (dap->swj.port == dap_port_swd) {
            transfer_ack = swd_transfer(dap, request, transfer_data);
        }
        if (transfer_ack != transfer_response_ack_wait) { break; }
    }

    return transfer_ack;
}

static inline void port_set_ir(struct dap_driver *dap, uint32_t *last_ir, uint32_t desired_ir) {
    if (dap->swj.port == dap_port_jtag && (last_ir == NULL || *last_ir != desired_ir)) {
        if (last_ir != NULL) {
            *last_ir = desired_ir;
        }
        jtag_set_ir(dap, desired_ir);
    }
}

int32_t dap_handle_cmd_transfer(struct dap_driver *dap) {
    if (ring_buf_put(&dap->buf.response, &dap_cmd_transfer, 1) != 1) return -ENOBUFS;
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint8_t *response_count_ptr = NULL;
    uint8_t *response_response_ptr = NULL;
    if (ring_buf_put_claim(&dap->buf.response, &response_count_ptr, 1) != 1) return -ENOBUFS;
    if (ring_buf_put_claim(&dap->buf.response, &response_response_ptr, 1) != 1) return -ENOBUFS;
    if (ring_buf_put_finish(&dap->buf.response, 2) < 0) return -ENOBUFS;

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
    if (ring_buf_get(&dap->buf.request, &index, 1) != 1) return -EMSGSIZE;
    /* number of transfers */
    uint8_t count = 0;
    if (ring_buf_get(&dap->buf.request, &count, 1) != 1) return -EMSGSIZE;
    /* number of completed transfers */
    uint8_t completed_count = 0;

    if (dap->swj.port == dap_port_disabled) {
        goto end;
    } else if (dap->swj.port == dap_port_jtag) {
        if (index >= dap->jtag.count) {
            goto end;
        }
        dap->jtag.index = index;
    }

    while (count > 0) {
        uint8_t request = 0;
        if (ring_buf_get(&dap->buf.request, &request, 1) != 1) return -EMSGSIZE;
        uint32_t request_ir = (request & transfer_request_apndp) ? jtag_ir_apacc : jtag_ir_dpacc;
        /* make sure to pull all request data before decrementing count, so that we don't miss
         * request bytes when processing cancelled requests */
        if ((request & transfer_request_rnw) == 0 ||
            (request & transfer_request_match_value) != 0) {
            if (ring_buf_get_le32(&dap->buf.request, &transfer_data) < 0) return -EMSGSIZE;
        }
        count--;

        /* TODO: for now we are just going to do the simple thing and read the previously posted value,
         * without posting the next read if available */
        if (read_pending) {
            port_set_ir(dap, &last_ir, jtag_ir_dpacc);
            transfer_ack = port_transfer(dap, transfer_request_rnw | dp_addr_rdbuff , &transfer_data);
            read_pending = false;
            if (transfer_ack != transfer_response_ack_ok) { break; }

            if (ring_buf_put_le32(&dap->buf.request, transfer_data) < 0) return -ENOBUFS;
        }

        if ((request & transfer_request_rnw) != 0) {
            if ((request & transfer_request_match_value) != 0) {
                /* read with match value */
                /* match value already stored in transfer_data, but shift it here to free up transfer_data */
                uint32_t match_value = transfer_data;
                port_set_ir(dap, &last_ir, request_ir);
                /* if using the SWD transport and reading from the DP, we don't need to post a read request
                 * first, it will be immediately available on the later read value, otherwise post first */
                if (dap->swj.port == dap_port_jtag || (request & transfer_request_apndp) != 0) {
                    /* post the read request */
                    transfer_ack = port_transfer(dap, request, &transfer_data);
                    if (transfer_ack != transfer_response_ack_ok) { break; }
                }
                /* get and check read value */
                for (uint32_t i = 0; i < dap->transfer.match_retries + 1; i++) {
                    transfer_ack = port_transfer(dap, request, &transfer_data);
                    if (transfer_ack != transfer_response_ack_ok ||
                        (transfer_data & dap->transfer.match_mask) == match_value) {
                        break;
                    }
                }
                if ((transfer_data & dap->transfer.match_mask) != match_value) {
                    transfer_ack |= transfer_response_value_mismatch;
                }
                if (transfer_ack != transfer_response_ack_ok) { break; }
            } else {
                /* normal read request */
                port_set_ir(dap, &last_ir, request_ir);
                transfer_ack = port_transfer(dap, request, &transfer_data);
                if (transfer_ack != transfer_response_ack_ok) { break; }
                /* on SWD reads to DP there is no nead to post the read, the correct data has been received */
                if (dap->swj.port == dap_port_swd && (request & transfer_request_apndp) == 0) {
                    if (ring_buf_put_le32(&dap->buf.request, transfer_data) < 0) return -ENOBUFS;
                } else {
                    read_pending = true;
                }
            }
            ack_pending = false;
        } else {
            if ((request & transfer_request_match_mask) != 0) {
                dap->transfer.match_mask = transfer_data;
                transfer_ack = transfer_response_ack_ok;
            } else {
                /* normal write request */
                port_set_ir(dap, &last_ir, request_ir);
                transfer_ack = port_transfer(dap, request, &transfer_data);
                if (transfer_ack != transfer_response_ack_ok) { break; }
                ack_pending = true;
            }
        }

        completed_count++;
    }

end:
    memcpy(response_count_ptr, &completed_count, 1);
    memcpy(response_response_ptr, &transfer_ack, 1);

    /* process remaining (canceled) request bytes */
    while (count > 0) {
        count--;

        uint8_t request = 0;
        if (ring_buf_get(&dap->buf.request, &request, 1) != 1) return -EMSGSIZE;
        /* write requests and read match value both have 4 bytes of request input */
        if (((request & transfer_request_rnw) != 0 &&
            (request & transfer_request_match_value) != 0) ||
            (request & transfer_request_rnw) == 0) {
            
            uint32_t temp;
            if (ring_buf_get(&dap->buf.request, (uint8_t*) &temp, 4) != 4) return -EMSGSIZE;
        }
    }

    /* perform final read to get last transfer ack and collect pending data if needed */
    if (transfer_ack == transfer_response_ack_ok && (read_pending || ack_pending)) {
        port_set_ir(dap, &last_ir, jtag_ir_dpacc);
        transfer_ack = port_transfer(dap, dp_addr_rdbuff | transfer_request_rnw, &transfer_data);
        if (transfer_ack == transfer_response_ack_ok && read_pending) {
            if (ring_buf_put_le32(&dap->buf.request, transfer_data) < 0) return -ENOBUFS;
        }

        memcpy(response_response_ptr, &transfer_ack, 1);
    }

    return 0;
}

int32_t dap_handle_cmd_transfer_block(struct dap_driver *dap) {
    if (ring_buf_put(&dap->buf.response, &dap_cmd_transfer_block, 1) != 1) return -ENOBUFS;
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint8_t *response_count_ptr = NULL;
    uint8_t *response_response_ptr = NULL;
    /* response_count is a uint16_t value, but we need to interact through uint8_t pointers for alignment reasons */
    if (ring_buf_put_claim(&dap->buf.response, &response_count_ptr, 2) != 2) return -ENOBUFS;
    if (ring_buf_put_claim(&dap->buf.response, &response_response_ptr, 1) != 1) return -ENOBUFS;
    if (ring_buf_put_finish(&dap->buf.response, 3) < 0) return -ENOBUFS;

    /* transfer acknowledge and data storage */
    uint8_t transfer_ack = 0;
    uint32_t transfer_data = 0;
    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    if (ring_buf_get(&dap->buf.request, &index, 1) != 1) return -EMSGSIZE;
    /* number of words transferred */
    uint16_t count = 0;
    if (ring_buf_get(&dap->buf.request, (uint8_t*) &count, 2) != 2) return -EMSGSIZE;
    /* number of completed transfers */
    uint16_t completed_count = 0;
    /* transfer request metadata */
    uint8_t request = 0;
    if (ring_buf_get(&dap->buf.request, &request, 1) != 1) return -EMSGSIZE;

    if (dap->swj.port == dap_port_disabled) {
        goto end;
    } else if (dap->swj.port == dap_port_jtag) {
        if (index >= dap->jtag.count) {
            goto end;
        }
        dap->jtag.index = index;
    }

    uint32_t request_ir = (request & transfer_request_apndp) ? jtag_ir_apacc : jtag_ir_dpacc;
    port_set_ir(dap, NULL, request_ir);

    if ((request & transfer_request_rnw) != 0) {
        /* for JTAG transfers and SWD transfers to the AP, we must first post the read request */
        if (dap->swj.port == dap_port_jtag || (request & transfer_request_apndp) != 0) {
            transfer_ack = port_transfer(dap, request, &transfer_data);
            if (transfer_ack != transfer_response_ack_ok) { goto end; }
        }

        while (count > 0) {
            count--;

            /* for JTAG transfers and SWD transfers to the AP, the final read should be to DP RDBUFF
             * so we don't post any further transactions */
            if (count == 0) {
                if (dap->swj.port == dap_port_jtag || (request & transfer_request_apndp) != 0) {
                    port_set_ir(dap, &request_ir, jtag_ir_dpacc);
                    request = dp_addr_rdbuff | transfer_request_rnw;
                }
            }

            transfer_ack = port_transfer(dap, request, &transfer_data);
            if (transfer_ack != transfer_response_ack_ok) { goto end; }
            if (ring_buf_put(&dap->buf.response, (uint8_t*) &transfer_data, 4) != 4) return -ENOBUFS;
            completed_count++;
        }
    } else {
        /* write transfer */
        while (count > 0) {
            count--;

            if (ring_buf_get(&dap->buf.request, (uint8_t*) &transfer_data, 4) != 4) return -EMSGSIZE;
            transfer_ack = port_transfer(dap, request, &transfer_data);
            if (transfer_ack != transfer_response_ack_ok) { goto end; }
            completed_count++;
        }
        /* get ack of last write */
        port_set_ir(dap, &request_ir, jtag_ir_dpacc);
        request = dp_addr_rdbuff | transfer_request_rnw;
        transfer_ack = port_transfer(dap, request, &transfer_data);
    }

end:
    sys_put_le16(completed_count, response_count_ptr);
    memcpy(response_response_ptr, &transfer_ack, 1);

    /* process remaining (canceled) request bytes */
    if (count > 0 && (request & transfer_request_rnw) == 0) {
        uint8_t *temp = NULL;
        uint16_t request_remaining = count * 4;
        if (ring_buf_get_claim(&dap->buf.request, &temp, request_remaining) != request_remaining) return -EMSGSIZE;
        if (ring_buf_get_finish(&dap->buf.request, request_remaining) < 0) return -EMSGSIZE;
    }

    return 0;
}

int32_t dap_handle_cmd_transfer_abort(struct dap_driver *dap) {
    /* TODO: eventually we should separate reading data from the transport from replying to the requests,
     * so that we can actually scan for the abort request and cancel an in-progress transfer */

    return 0;
}

int32_t dap_handle_cmd_write_abort(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    if (ring_buf_get(&dap->buf.request, &index, 1) != 1) return -EMSGSIZE;
    /* value to write to the abort register */
    uint32_t abort = 0;
    if (ring_buf_get_le32(&dap->buf.request, &abort) < 0) return -EMSGSIZE;

    if (dap->swj.port == dap_port_disabled) {
        status = dap_cmd_response_error;
        goto end;
    } else if (dap->swj.port == dap_port_jtag) {
        if (index >= dap->jtag.count) {
            status = dap_cmd_response_error;
            goto end;
        }
        dap->jtag.index = index;
    }

    port_set_ir(dap, NULL, jtag_ir_abort);
    /* DP write, address 0x0, */
    port_transfer(dap, 0x00, &abort);

end: ;
    uint8_t response[] = {dap_cmd_write_abort, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}
