#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

int32_t dap_handle_command_transfer_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    CHECK_EQ(ring_buf_get(config->request_buf, &data->transfer.idle_cycles, 1), 1, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &data->transfer.wait_retries, 2), 2, -EMSGSIZE);
    CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &data->transfer.match_retries, 2), 2, -EMSGSIZE);

    uint8_t response[] = {DAP_COMMAND_TRANSFER_CONFIGURE, DAP_COMMAND_RESPONSE_OK};
    CHECK_EQ(ring_buf_put(config->response_buf, response, 2), 2, -ENOBUFS);
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_transfer(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_TRANSFER}), 1), 1, -ENOBUFS);
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint8_t *response_count = NULL;
    uint8_t *response_response = NULL;
    CHECK_EQ(ring_buf_put_claim(config->response_buf, &response_count, 1), 1, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_claim(config->response_buf, &response_response, 1), 1, -ENOBUFS);
    *response_response = 0;
    CHECK_EQ(ring_buf_put_finish(config->response_buf, 2), 0, -ENOBUFS);

    /* transfer acknowledge and data storage */
    uint8_t transfer_ack = 0;
    uint32_t transfer_data = 0;
    /* cache current ir value, only change tap ir value when needed */
    uint32_t last_ir = 0;
    /* set after a read request is made, to capture data on the next transfer (or at end) */
    bool read_pending = false;
    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &index, 1), 1, -EMSGSIZE);
    /* number of transfers */
    uint8_t count = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &count, 1), 1, -EMSGSIZE);

    if (data->swj.port == DAP_PORT_DISABLED) {
        goto end;
    } else if (data->swj.port == DAP_PORT_SWD) {
        /* TODO: add support for swd and remove this check */
        goto end;
    } else if (data->swj.port == DAP_PORT_JTAG) {
        if (index >= data->jtag.count) {
            goto end;
        }
        data->jtag.index = index;
    }

    while (count > 0) {
        uint8_t request = 0;
        CHECK_EQ(ring_buf_get(config->request_buf, &request, 1), 1, -EMSGSIZE);
        uint32_t request_ir = (request & TRANSFER_REQUEST_APnDP) ? JTAG_IR_APACC : JTAG_IR_DPACC;
        /* make sure to pull all request data before decrementing count, so that we don't miss
         * request bytes when processing cancelled requests */
        if ((request & TRANSFER_REQUEST_RnW) == 0 ||
            (request & TRANSFER_REQUEST_MATCH_VALUE) != 0) {
            CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &transfer_data, 4), 4, -EMSGSIZE);
        }
        count--;

        if (read_pending) {
            /* the previous transfer was a read and data is currently pending */
            if (request_ir == last_ir &&
                (request & TRANSFER_REQUEST_RnW) != 0 &&
                (request & TRANSFER_REQUEST_MATCH_VALUE) == 0) {
                /* if the current request is a read request and the ir is already correct, we can
                   read previous data and submit the current read in one transfer */
                
                for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                    transfer_ack = jtag_transfer(dev, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) {
                        break;
                    }
                }
                /* no need to clear read pending because this current request is also a read */
            } else {
                if (last_ir != JTAG_IR_DPACC) {
                    last_ir = JTAG_IR_DPACC;
                    jtag_set_ir(dev, JTAG_IR_DPACC);
                }
                for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                    transfer_ack = jtag_transfer(dev, TRANSFER_REQUEST_RnW | DP_ADDR_RDBUFF , &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
                }
                read_pending = false;
            }
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) {
                break;
            } else {
                CHECK_EQ(ring_buf_put(config->response_buf, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
            }
        }

        if ((request & TRANSFER_REQUEST_RnW) != 0) {
            if ((request & TRANSFER_REQUEST_MATCH_VALUE) != 0) {
                /* read with match value */
                /* match value already stored in transfer_data, but shift it here to free up transfer_data */
                uint32_t match_value = transfer_data;
                if (last_ir != request_ir) {
                    last_ir = request_ir;
                    jtag_set_ir(dev, request_ir);
                }
                for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                    transfer_ack = jtag_transfer(dev, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
                }
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                for (uint32_t i = 0; i < data->transfer.match_retries + 1; i++) {
                    for (uint32_t j = 0; j < data->transfer.wait_retries + 1; j++) {
                        transfer_ack = jtag_transfer(dev, request, &transfer_data);
                        if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
                    }
                    
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
                if (last_ir != request_ir) {
                    last_ir = request_ir;
                    jtag_set_ir(dev, request_ir);
                }
                for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                    transfer_ack = jtag_transfer(dev, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
                }
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
                read_pending = true;
            }
        } else {
            if ((request & TRANSFER_REQUEST_MATCH_MASK) != 0) {
                data->transfer.match_mask = transfer_data;
                transfer_ack = TRANSFER_RESPONSE_ACK_OK;
            } else {
                /* normal write request */
                if (last_ir != request_ir) {
                    last_ir = request_ir;
                    jtag_set_ir(dev, request_ir);
                }
                for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                    transfer_ack = jtag_transfer(dev, request, &transfer_data);
                    if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
                }
                if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { break; }
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
        CHECK_EQ(ring_buf_get(config->request_buf, &request, 1), 1, -EMSGSIZE);
        /* write requests and read match value both have 4 bytes of request input */
        if (((request & TRANSFER_REQUEST_RnW) != 0 &&
            (request & TRANSFER_REQUEST_MATCH_VALUE) != 0) ||
            (request & TRANSFER_REQUEST_RnW) == 0) {
            
            uint32_t temp;
            CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &temp, 4), 4, -EMSGSIZE);
        }
    }

    /* perform final read to get last transfer ack and collect pending data if needed */
    if (transfer_ack == TRANSFER_RESPONSE_ACK_OK) {
        if (last_ir != JTAG_IR_DPACC) {
            last_ir = JTAG_IR_DPACC;
            jtag_set_ir(dev, JTAG_IR_DPACC);
        }
        for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
            transfer_ack = jtag_transfer(dev, DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
        }
        if (transfer_ack == TRANSFER_RESPONSE_ACK_OK && read_pending) {
            CHECK_EQ(ring_buf_put(config->response_buf, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
        }

        *response_response = transfer_ack;
    }

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_transfer_block(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    CHECK_EQ(ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_TRANSFER_BLOCK}), 1), 1, -ENOBUFS);
    /* need a pointer to these items because we will write to them after trying the rest of the command */
    uint16_t *response_count = NULL;
    uint8_t *response_response = NULL;
    CHECK_EQ(ring_buf_put_claim(config->response_buf, (uint8_t**) &response_count, 2), 2, -ENOBUFS);
    *response_count = 0;
    CHECK_EQ(ring_buf_put_claim(config->response_buf, &response_response, 1), 1, -ENOBUFS);
    *response_response = 0;
    CHECK_EQ(ring_buf_put_finish(config->response_buf, 3), 0, -ENOBUFS);

    /* transfer acknowledge and data storage */
    uint8_t transfer_ack = 0;
    uint32_t transfer_data = 0;
    /* jtag index, ignored for SWD */
    uint8_t index = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &index, 1), 1, -EMSGSIZE);
    /* number of words transferred */
    uint16_t count = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &count, 2), 2, -EMSGSIZE);
    /* transfer request metadata */
    uint8_t request = 0;
    CHECK_EQ(ring_buf_get(config->request_buf, &request, 1), 1, -EMSGSIZE);

    if (data->swj.port == DAP_PORT_DISABLED) {
        goto end;
    } else if (data->swj.port == DAP_PORT_SWD) {
        /* TODO: add support for swd and remove this check */
        goto end;
    } else if (data->swj.port == DAP_PORT_JTAG) {
        if (index >= data->jtag.count) {
            goto end;
        }
        data->jtag.index = index;
    }

    uint32_t request_ir = (request & TRANSFER_REQUEST_APnDP) ? JTAG_IR_APACC : JTAG_IR_DPACC;
    jtag_set_ir(dev, request_ir);

    if ((request & TRANSFER_REQUEST_RnW) != 0) {
        /* read transfer, prepare read then get all words */
        for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
            transfer_ack = jtag_transfer(dev, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
        }
        if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }

        while (count > 0) {
            count--;

            /* final read should be to DP RDBUFF, just to get last data out */
            if (count == 0) {
                if (request_ir != JTAG_IR_DPACC) {
                    jtag_set_ir(dev, JTAG_IR_DPACC);
                }
                request = DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW;
            }

            for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                transfer_ack = jtag_transfer(dev, request, &transfer_data);
                if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
            }
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) {
                goto end;
            } else {
                CHECK_EQ(ring_buf_put(config->response_buf, (uint8_t*) &transfer_data, 4), 4, -ENOBUFS);
            }
            *response_count += 1;
        }
    } else {
        /* write transfer */
        while (count > 0) {
            count--;

            CHECK_EQ(ring_buf_get(config->request_buf, (uint8_t*) &transfer_data, 4), 4, -EMSGSIZE);
            for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
                transfer_ack = jtag_transfer(dev, request, &transfer_data);
                if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
            }
            if (transfer_ack != TRANSFER_RESPONSE_ACK_OK) { goto end; }
            *response_count += 1;
        }
        /* get ack of last write */
        if (request_ir != JTAG_IR_DPACC) {
            jtag_set_ir(dev, JTAG_IR_DPACC);
        }
        request = DP_ADDR_RDBUFF | TRANSFER_REQUEST_RnW;
        for (uint32_t i = 0; i < data->transfer.wait_retries + 1; i++) {
            transfer_ack = jtag_transfer(dev, request, &transfer_data);
            if (transfer_ack != TRANSFER_RESPONSE_ACK_WAIT) { break; }
        }
    }
    *response_response = transfer_ack;

end:
    /* process remaining (canceled) request bytes */
    if ((request & TRANSFER_REQUEST_RnW) == 0) {
        uint8_t *temp = NULL;
        uint16_t request_remaining = count * 4;
        CHECK_EQ(ring_buf_get_claim(config->request_buf, &temp, request_remaining), request_remaining, -EMSGSIZE);
        CHECK_EQ(ring_buf_get_finish(config->request_buf, request_remaining), 0, -EMSGSIZE);
    }

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_transfer_abort(const struct device *dev) {
    /* TODO: eventually we should separate reading data from the USB from replying to the requests,
     * so that we can actually scan for the abort request and cancel an in-progress transfer */

    return 0;
}
