#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>

#include "dap/dap.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

void jtag_tck_cycle(struct dap_driver *dap) {
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
}

void jtag_tdi_cycle(struct dap_driver *dap, uint8_t tdi) {
    gpio_pin_set_dt(&dap->io.tdi, tdi & 0x01);
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
}

uint8_t jtag_tdo_cycle(struct dap_driver *dap) {
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    uint8_t tdo = gpio_pin_get_dt(&dap->io.tdo);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
    return tdo & 0x01;
}

uint8_t jtag_tdio_cycle(struct dap_driver *dap, uint8_t tdi) {
    gpio_pin_set_dt(&dap->io.tdi, tdi & 0x01);
    gpio_pin_set_dt(&dap->io.tck_swclk, 0);
    busy_wait_nanos(dap->swj.delay_ns);
    uint8_t tdo = gpio_pin_get_dt(&dap->io.tdo);
    gpio_pin_set_dt(&dap->io.tck_swclk, 1);
    busy_wait_nanos(dap->swj.delay_ns);
    return tdo & 0x01;
}

void jtag_set_ir(struct dap_driver *dap, uint32_t ir) {
    /* assumes we are starting in idle tap state, move to select-dr-scan then select-ir-scan */
    gpio_pin_set_dt(&dap->io.tms_swdio, 1);
    jtag_tck_cycle(dap);
    jtag_tck_cycle(dap);

    /* capture-ir, then shift-ir */
    gpio_pin_set_dt(&dap->io.tms_swdio, 0);
    jtag_tck_cycle(dap);
    jtag_tck_cycle(dap);

    /* bypass all tap bits before index */
    gpio_pin_set_dt(&dap->io.tdi, 1);
    for (uint16_t i = 0; i < dap->jtag.ir_before[dap->jtag.index]; i++) {
        jtag_tck_cycle(dap);
    }
    /* set all ir bits except the last */
    for (uint16_t i = 0; i < dap->jtag.ir_length[dap->jtag.index] - 1; i++) {
        jtag_tdi_cycle(dap, ir);
        ir >>= 1;
    }
    /* set last ir bit and bypass all remaining ir bits */
    if (dap->jtag.ir_after[dap->jtag.index] == 0) {
        /* set last ir bit, then exit-1-ir */
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        jtag_tdi_cycle(dap, ir);
    } else {
        jtag_tdi_cycle(dap, ir);
        gpio_pin_set_dt(&dap->io.tdi, 1);
        for (uint16_t i = 0; i < dap->jtag.ir_after[dap->jtag.index] - 1; i++) {
            jtag_tck_cycle(dap);
        }
        /* set last bypass bit, then exit-1-ir */
        gpio_pin_set_dt(&dap->io.tms_swdio, 1);
        jtag_tck_cycle(dap);
    }

    /* update-ir then idle */
    jtag_tck_cycle(dap);
    gpio_pin_set_dt(&dap->io.tms_swdio, 0);
    jtag_tck_cycle(dap);
    gpio_pin_set_dt(&dap->io.tdi, 1);

    return;
}

int32_t dap_handle_cmd_jtag_configure(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    uint8_t count = 0;
    if (ring_buf_get(&dap->buf.request, &count, 1) != 1) return -EMSGSIZE;
    if (count > DAP_JTAG_MAX_DEVICE_COUNT) {
        status = dap_cmd_response_error;
        /* process remaining request bytes */
        uint8_t *temp = NULL;
        if (ring_buf_get_claim(&dap->buf.request, &temp, count) != count) return -EMSGSIZE;
        if (ring_buf_get_finish(&dap->buf.request, count) != 0) return -EMSGSIZE;
        goto end;
    }

    uint16_t ir_length_sum = 0;
    dap->jtag.count = count;
    for (uint8_t i = 0; i < dap->jtag.count; i++) {
        uint8_t len = 0;
        if (ring_buf_get(&dap->buf.request, &len, 1) != 1) return -EMSGSIZE;
        dap->jtag.ir_before[i] = ir_length_sum;
        ir_length_sum += len;
        dap->jtag.ir_length[i] = len;
    }
    for (uint8_t i = 0; i < dap->jtag.count; i++) {
        ir_length_sum -= dap->jtag.ir_length[i];
        dap->jtag.ir_after[i] = ir_length_sum;
    }

end: ;
    uint8_t response[] = {dap_cmd_jtag_configure, status};
    if (ring_buf_put(&dap->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}

int32_t dap_handle_cmd_jtag_sequence(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;

    /* command info bitfield */
    const uint8_t info_tck_cycles_mask = 0x3f;
    const uint8_t info_tdo_capture_mask = 0x80;
    const uint8_t info_tms_value_shift = 6;

    if (ring_buf_put(&dap->buf.response, &dap_cmd_jtag_sequence, 1) != 1) return -EMSGSIZE;
    /* need a pointer to this item because we will write to it after trying the rest of the command */
    uint8_t *response_status = NULL;
    if (ring_buf_put_claim(&dap->buf.response, &response_status, 1) != 1) return -ENOBUFS;
    if (ring_buf_put_finish(&dap->buf.response, 1) < 0) return -ENOBUFS;

    uint8_t seq_count = 0;
    if (ring_buf_get(&dap->buf.request, &seq_count, 1) != 1) return -EMSGSIZE;
    for (uint8_t i = 0; i < seq_count; i++) {
        uint8_t info = 0;
        if (ring_buf_get(&dap->buf.request, &info, 1) != 1) return -EMSGSIZE;

        uint8_t tck_cycles = info & info_tck_cycles_mask;
        if (tck_cycles == 0) {
            tck_cycles = 64;
        }

        /* if the current port isn't JTAG, just use this loop to process remaining bytes */
        if (dap->swj.port != dap_port_jtag) {
            status = dap_cmd_response_error;
            while (tck_cycles > 0) {
                uint8_t temp = 0;
                if (ring_buf_get(&dap->buf.request, &temp, 1) != 1) return -EMSGSIZE;
                tck_cycles -= MIN(tck_cycles, 8);
            }
            continue;
        }

        uint8_t tms_val = (info & BIT(info_tms_value_shift)) >> info_tms_value_shift;
        gpio_pin_set_dt(&dap->io.tms_swdio, tms_val);

        while (tck_cycles > 0) {
            uint8_t tdi = 0;
            if (ring_buf_get(&dap->buf.request, &tdi, 1) != 1) return -EMSGSIZE;
            uint8_t tdo = 0;

            uint8_t bits = 8;
            while (bits > 0 && tck_cycles > 0) {
                uint8_t tdo_bit = jtag_tdio_cycle(dap, tdi);
                tdi >>= 1;
                tdo >>= 1;
                tdo |= tdo_bit << 7;
                bits--;
                tck_cycles--;
            }
            /* if we are on byte boundary, no-op, otherwise move tdo to final bit position */
            tdo >>= bits;

            if ((info & info_tdo_capture_mask) != 0) {
                if (ring_buf_put(&dap->buf.response, &tdo, 1) != 1) return -ENOBUFS;
            }
        }
    }

    memcpy(response_status, &status, 1);
    return 0;
}

int32_t dap_handle_cmd_jtag_idcode(struct dap_driver *dap) {
    uint8_t status = dap_cmd_response_ok;
    uint32_t idcode = 0;

    uint8_t index = 0;
    if (ring_buf_get(&dap->buf.request, &index, 1) != 1) return -EMSGSIZE;
    if ((dap->swj.port != dap_port_jtag) ||
        (index >= dap->jtag.count)) {
        status = dap_cmd_response_error;
        goto end;
    } else {
        dap->jtag.index = index;
    }

    const uint8_t jtag_ir_idcode = 0x0e;
    jtag_set_ir(dap, jtag_ir_idcode);

    /* select-dr-scan */
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
    /* tdo bits 0..30 */
    for (uint8_t i = 0; i < 31; i++) {
        idcode |= jtag_tdo_cycle(dap) << i;
    }
    /* last tdo bit and exit-1-dr*/
    gpio_pin_set_dt(&dap->io.tms_swdio, 1);
    idcode |= jtag_tdo_cycle(dap) << 31;

    /* update-dr, then idle */
    jtag_tck_cycle(dap);
    gpio_pin_set_dt(&dap->io.tms_swdio, 0);
    jtag_tck_cycle(dap);

end: ;
    uint8_t response[] = {dap_cmd_jtag_idcode, status, 0, 0, 0, 0};
    sys_put_le32(idcode, &response[2]);
    if (ring_buf_put(&dap->buf.response, response, 6) != 6) return -ENOBUFS;

    return 0;
}
