#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "io/io.h"
#include "utf8.h"
#include "util.h"

LOG_MODULE_DECLARE(io, CONFIG_IO_LOG_LEVEL);

/* bit field for supported and requested pinctrl flags */
const uint8_t pinctrl_flags_len = 1;
const uint8_t pinctrl_flags_pull_up = BIT(0);
const uint8_t pinctrl_flags_pull_down = BIT(1);
const uint8_t pinctrl_flags_open_drain = BIT(2);

/* request:
 * | UTF8 | U8  |
 * | 0x09 | Pin |
 * 
 * response:
 * | UTF8 | U8     | U8        | U8+   | U8           | (U8, U8)+         |
 * | 0x09 | Status | Flags Len | Flags | Function Len | (Function, Index) |
 */
int32_t io_handle_cmd_pins_caps(struct io_driver *io) {
    int32_t ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_pins_caps)) < 0) return ret;

    uint8_t pin = 0;
    if (ring_buf_get(&io->buf.request, &pin, 1) != 1) return -EMSGSIZE;

    /* unsupported pin number, since all pins support GPIO */
    if (pin > IO_GPIOS_CNT()) {
        if ((ret = ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1)) != 1) return -ENOBUFS;
        return 0;
    }
    if ((ret = ring_buf_put(&io->buf.response, &io_cmd_response_ok, 1)) != 1) return -ENOBUFS;

    /* supported on all IOs */
    const uint8_t supported_flags = pinctrl_flags_pull_up |
                                    pinctrl_flags_pull_down |
                                    pinctrl_flags_open_drain;
    const uint8_t flags_response[2] = { pinctrl_flags_len, supported_flags };
    if ((ret = ring_buf_put(&io->buf.response, flags_response, 2)) != 2) return -ENOBUFS;

    /* need a pointer to this item for writing to after discovering number of functions */
    uint8_t *response_func_len_ptr = NULL;
    if (ring_buf_put_claim(&io->buf.response, &response_func_len_ptr, 1) != 1) return -ENOBUFS;
    if (ring_buf_put_finish(&io->buf.response, 1) < 0) return -ENOBUFS;
    *response_func_len_ptr = 0;

    for (uint16_t i = 0; i < ARRAY_SIZE(io->pinctrls); i++) {
        if (io->pinctrls[i].pin != pin) continue;

        uint8_t func_response[2] = { io->pinctrls[i].function, io->pinctrls[i].index };
        if (ring_buf_put(&io->buf.response, func_response, 2) != 2) return -ENOBUFS;
        *response_func_len_ptr += 1;
    }

    return 0;
}

/* request:
 * | UTF8 |
 * | 0x0a |
 * 
 * response:
 * | UTF8 | U8     |
 * | 0x0a | Status |
 */
int32_t io_handle_cmd_pins_default(struct io_driver *io) {
    int32_t ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_pins_default)) < 0) return ret;

    for (uint8_t i = 0; i < ARRAY_SIZE(io->gpios); i++) {
        if ((ret = gpio_pin_configure_dt(&io->gpios[i], GPIO_INPUT)) < 0) {
            LOG_ERR("gpio%u initialize failed (%d)", i, ret);
            if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
            return 0;
        }
    }

    for (uint16_t i = 0; i < ARRAY_SIZE(io->pinctrls); i++) {
        if (io->pinctrls[i].function == IO_PIN_FUNC_GPIO && (ret = io_configure_pin(&io->pinctrls[i].pinctrl)) < 0) {
            LOG_ERR("io%u pinctrl configure failed (%d)", io->pinctrls[i].pin, ret);
            if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
            return 0;
        }
    }

    if (ring_buf_put(&io->buf.response, &io_cmd_response_ok, 1) != 1) return -ENOBUFS;
    return 0;
}

/* request:
 * | UTF8 | U8  | U8       | U8    | U8        | U8+   |
 * | 0x0b | Pin | Function | Index | Flags Len | Flags |
 * 
 * response:
 * | UTF8 | U8     |
 * | 0x0b | Status |
 */
int32_t io_handle_cmd_pins_cfg(struct io_driver *io) {
    int32_t ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_pins_cfg)) < 0) return ret;

    uint8_t pin = 0;
    if (ring_buf_get(&io->buf.request, &pin, 1) != 1) return -EMSGSIZE;
    uint8_t function = 0;
    if (ring_buf_get(&io->buf.request, &function, 1) != 1) return -EMSGSIZE;
    uint8_t index = 0;
    if (ring_buf_get(&io->buf.request, &index, 1) != 1) return -EMSGSIZE;

    uint8_t flags_len = 0;
    if (ring_buf_get(&io->buf.request, &flags_len, 1) != 1) return -EMSGSIZE;
    uint8_t flags = 0;
    if (ring_buf_get(&io->buf.request, &flags, 1) != 1) return -EMSGSIZE;
    /* only support a single flags byte for now */
    if (flags_len > pinctrl_flags_len) {
        LOG_WRN("Only support a single pin flag byte");
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
        return 0;
    }

    pinctrl_soc_pin_t *pinctrl = NULL;
    for (uint16_t i = 0; i < ARRAY_SIZE(io->pinctrls); i++) {
        if (io->pinctrls[i].pin == pin &&
            io->pinctrls[i].function == function &&
            io->pinctrls[i].index == index) {

            pinctrl = &io->pinctrls[i].pinctrl;
        }
    }
    if (pinctrl == NULL) {
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
        return 0;
    }

    /* adjust the pinctrl value based on the provided flags */
    if ((flags & pinctrl_flags_pull_up) != 0) *pinctrl |= IO_PINCTRL_FLAG_PULLUP;
    if ((flags & pinctrl_flags_pull_down) != 0) *pinctrl |= IO_PINCTRL_FLAG_PULLDOWN;
    if ((flags & pinctrl_flags_open_drain) != 0) *pinctrl |= IO_PINCTRL_FLAG_OPENDRAIN;

    /* if the pin is also being configured as a GPIO, ensure that it is initially
     * configured as an input, and that the same pull up/down flags are set. */
    if (function == IO_PIN_FUNC_GPIO) {
        gpio_flags_t gpio_flags = GPIO_INPUT |
            ((flags & pinctrl_flags_pull_up) != 0 ? GPIO_PULL_UP : 0) |
            ((flags & pinctrl_flags_pull_down) != 0 ? GPIO_PULL_DOWN : 0);        
        if ((ret = gpio_pin_configure_dt(&io->gpios[index], gpio_flags)) < 0) {
            LOG_ERR("gpio%u initialize failed (%d)", index, ret);
            if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
            return 0;
        }
    }

    if ((ret = io_configure_pin(pinctrl)) < 0) {
        LOG_ERR("io%u pinctrl configure failed (%d)", pin, ret);
        if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
    } else {
        if (ring_buf_put(&io->buf.response, &io_cmd_response_ok, 1) != 1) return -ENOBUFS;
    }

    return 0;
}
