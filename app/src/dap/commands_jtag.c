#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"
#include "util.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

static inline void jtag_tck_cycle(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
}

static inline void jtag_tdi_cycle(const struct device *dev, uint8_t tdi) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tdi_gpio, tdi & 0x01);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
}

static inline uint8_t jtag_tdo_cycle(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    uint8_t tdo = gpio_pin_get_dt(&config->tdo_gpio);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
    return tdo;
}

static inline uint8_t jtag_tdio_cycle(const struct device *dev, uint8_t tdi) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    gpio_pin_set_dt(&config->tdi_gpio, tdi & 0x01);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 0);
    busy_wait_nanos(data->swj.delay_ns);
    uint8_t tdo = gpio_pin_get_dt(&config->tdo_gpio);
    gpio_pin_set_dt(&config->tck_swclk_gpio, 1);
    busy_wait_nanos(data->swj.delay_ns);
    return tdo;
}

static void jtag_set_ir(const struct device *dev, uint8_t index, uint32_t ir) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;

    /* assumes we are starting in idle tap state, move to select-dr-scan then select-ir-scan */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* capture-ir, then shift-ir */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* bypass all tap bits before index */
    gpio_pin_set_dt(&config->tdi_gpio, 1);
    for (int i = 0; i < data->jtag.ir_before[index]; i++) {
        jtag_tck_cycle(dev);
    }
    /* set all ir bits except the last */
    for (int i = 0; i < data->jtag.ir_length[index] - 1; i++) {
        jtag_tdi_cycle(dev, ir);
        ir >>= 1;
    }
    /* set last ir bit and bypass all remaining ir bits */
    if (data->jtag.ir_after[index] == 0) {
        /* set last ir bit, then exit-1-ir */
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        jtag_tdi_cycle(dev, ir);
    } else {
        jtag_tdi_cycle(dev, ir);
        gpio_pin_set_dt(&config->tdi_gpio, 1);
        for (int i = 0; i < data->jtag.ir_after[index] - 1; i++) {
            jtag_tck_cycle(dev);
        }
        /* set last bypass bit, then exit-1-ir */
        gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
        jtag_tck_cycle(dev);
    }

    /* update-ir then idle */
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tdi_gpio, 1);

    return;
}

static uint32_t jtag_get_dr_le32(const struct device *dev, uint8_t index) {
    const struct dap_config *config = dev->config;

    /* assumes we are starting in idle tap state, move to select-dr-scan */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    jtag_tck_cycle(dev);

    /* capture-dr, then shift-dr */
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);
    jtag_tck_cycle(dev);

    /* bypass for every tap before the current index */
    for (int i = 0; i < index; i++) {
        jtag_tck_cycle(dev);
    }

    /* tdo bits 0..30 */
    uint32_t word = 0;
    for (int i = 0; i < 31; i++) {
        word |= jtag_tdo_cycle(dev) << i;
    }
    /* last tdo bit and exit-1-dr*/
    gpio_pin_set_dt(&config->tms_swdio_gpio, 1);
    word |= jtag_tdo_cycle(dev) << 31;

    /* update-dr, then idle */
    jtag_tck_cycle(dev);
    gpio_pin_set_dt(&config->tms_swdio_gpio, 0);
    jtag_tck_cycle(dev);

    return sys_le32_to_cpu(word);
}

int32_t dap_handle_command_jtag_configure(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* minimum possible size for command, but actual length is variable */
    if (ring_buf_size_get(config->request_buf) < 2) { return -EMSGSIZE; }

    uint8_t count = 0;
    ring_buf_get(config->request_buf, &count, 1);
    if (count > DAP_JTAG_MAX_DEVICE_COUNT ||
        ring_buf_size_get(config->request_buf) < count) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    }

    uint16_t ir_length_sum = 0;
    data->jtag.count = count;
    for (int i = 0; i < data->jtag.count; i++) {
        uint8_t len = 0;
        ring_buf_get(config->request_buf, &len, 1);
        data->jtag.ir_before[i] = ir_length_sum;
        ir_length_sum += len;
        data->jtag.ir_length[i] = len;
    }
    for (int i = 0; i < data->jtag.count; i++) {
        ir_length_sum -= data->jtag.ir_length[i];
        data->jtag.ir_after[i] = ir_length_sum;
    }

end: ;
    uint8_t response[] = {DAP_COMMAND_JTAG_CONFIGURE, status};
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));

    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_jtag_sequence(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;

    /* command info bitfield */
    const uint8_t info_tck_cycles_mask = 0x3f;
    const uint8_t info_tdo_capture_mask = 0x80;
    const uint8_t info_tms_value_shift = 6;

    /* minimum possible size for command, but actual length is variable */
    if (ring_buf_size_get(config->request_buf) < 3) { return -EMSGSIZE; }

    ring_buf_put(config->response_buf, &((uint8_t) {DAP_COMMAND_JTAG_SEQUENCE}), 1);
    /* need a pointer to this item because we will write to it after trying the rest of the command */
    uint8_t *command_status = NULL;
    ring_buf_put_claim(config->response_buf, &command_status, 1);
    *command_status = 0;
    ring_buf_put_finish(config->response_buf, 1);

    if (data->swj.port != DAP_PORT_JTAG) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    }

    uint8_t seq_count = 0;
    ring_buf_get(config->request_buf, &seq_count, 1);
    for (int i = 0; i < seq_count; i++) {
        uint8_t info = 0;
        ring_buf_get(config->request_buf, &info, 1);

        uint8_t tck_cycles = info & info_tck_cycles_mask;
        if (tck_cycles == 0) {
            tck_cycles = 64;
        }

        uint8_t tms_val = (info & BIT(info_tms_value_shift)) >> info_tms_value_shift;
        gpio_pin_set_dt(&config->tms_swdio_gpio, tms_val);

        while (tck_cycles > 0) {
            uint8_t tdi = 0;
            ring_buf_get(config->request_buf, &tdi, 1);
            uint8_t tdo = 0;

            uint8_t bits = 8;
            while (bits > 0 && tck_cycles > 0) {
                uint8_t tdo_bit = jtag_tdio_cycle(dev, tdi);
                tdi >>= 1;
                tdo >>= 1;
                tdo |= tdo_bit << 7;
                bits--;
                tck_cycles--;
            }
            /* if we are on byte boundary, no-op, otherwise move tdo to final bit position */
            tdo >>= bits;

            if ((info & info_tdo_capture_mask) != 0) {
                ring_buf_put(config->response_buf, &tdo, 1);
            }
        }
    }

end:
    *command_status = status;
    return ring_buf_size_get(config->response_buf);
}

int32_t dap_handle_command_jtag_idcode(const struct device *dev) {
    struct dap_data *data = dev->data;
    const struct dap_config *config = dev->config;
    uint8_t status = DAP_COMMAND_RESPONSE_OK;
    uint32_t idcode = 0;

    const uint8_t jtag_ir_idcode = 0x0e;

    if (ring_buf_size_get(config->request_buf) < 1) { return -EMSGSIZE; }

    uint8_t index = 0;
    ring_buf_get(config->request_buf, &index, 1);
    if ((data->swj.port != DAP_PORT_JTAG) ||
        (index >= data->jtag.count)) {
        status = DAP_COMMAND_RESPONSE_ERROR;
        goto end;
    }

    jtag_set_ir(dev, index, jtag_ir_idcode);
    idcode = jtag_get_dr_le32(dev, index);

end: ;
    uint8_t response[] = {DAP_COMMAND_JTAG_IDCODE, status, 0, 0, 0, 0};
    memcpy(&response[2], (uint8_t*) &idcode, sizeof(idcode));
    ring_buf_put(config->response_buf, response, ARRAY_SIZE(response));
    return ring_buf_size_get(config->response_buf);
}
