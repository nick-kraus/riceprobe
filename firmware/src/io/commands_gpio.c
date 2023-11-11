#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "io/io.h"
#include "utf8.h"
#include "util.h"

LOG_MODULE_DECLARE(io, CONFIG_IO_LOG_LEVEL);

/* bit field for supported and requested gpio flags */
const uint8_t gpio_flags_len = 1;
const uint8_t gpio_flags_output = BIT(0);
const uint8_t gpio_flags_active_low = BIT(1);
const uint8_t gpio_flags_pull_up = BIT(2);
const uint8_t gpio_flags_pull_down = BIT(3);
const uint8_t gpio_flags_open_drain = BIT(4);

/* request:
 * | UTF8 |
 * | 0x10 |
 * 
 * response:
 * | UTF8 | U8     | U8       | U8+  | U8        | U8+   |
 * | 0x10 | Status | Caps Len | Caps | Flags Len | Flags |
 */
int32_t io_handle_cmd_gpio_caps(struct io_driver *io) {
    int32_t ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_gpio_caps)) < 0) return ret;

    if (ring_buf_put(&io->buf.response, &io_cmd_response_ok, 1) != 1) return -ENOBUFS;

    const uint8_t gpio_caps_len = 1;
    const uint8_t gpio_caps_support_input_output_wait = BIT(0);
    const uint8_t gpio_caps_support_toggle = BIT(1);
    const uint8_t gpio_caps = gpio_caps_support_input_output_wait |
                              gpio_caps_support_toggle;
    const uint8_t caps_response[2] = { gpio_caps_len, gpio_caps };
    if (ring_buf_put(&io->buf.response, caps_response, 2) != 2) return -ENOBUFS;

    /* all devices support output, active low, and pull up/down */
    uint8_t gpio_flags = gpio_flags_output |
                         gpio_flags_active_low |
                         gpio_flags_pull_up |
                         gpio_flags_pull_down;
    /* posix based boards do not support open drain IO */
    gpio_flags |= IO_PINCTRL_FLAG_OPENDRAIN == 0 ? 0 : gpio_flags_open_drain;
    const uint8_t flags_response[2] = { gpio_flags_len, gpio_flags };
    if (ring_buf_put(&io->buf.response, flags_response, 2) != 2) return -ENOBUFS;

    return 0;
}

/* request:
 * | UTF8 | U8   | U8        | U8+   |
 * | 0x11 | GPIO | Flags Len | Flags |
 * 
 * response:
 * | UTF8 | U8     |
 * | 0x11 | Status |
 */
int32_t io_handle_cmd_gpio_cfg(struct io_driver *io) {
    int32_t ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_gpio_cfg)) < 0) return ret;

    uint8_t gpion = 0;
    if (ring_buf_get(&io->buf.request, &gpion, 1) != 1) return -EMSGSIZE;

    uint8_t flags_len = 0;
    if (ring_buf_get(&io->buf.request, &flags_len, 1) != 1) return -EMSGSIZE;
    /* only support a single flags byte for now */
    if (flags_len != gpio_flags_len) {
        LOG_WRN("Only support a single gpio flag byte");
        /* clear remaining request bytes */
        if (ring_buf_get_skip(&io->buf.request, flags_len) < 0) return -EMSGSIZE;
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
        return 0;
    }
    uint8_t flags = 0;
    if (ring_buf_get(&io->buf.request, &flags, 1) != 1) return -EMSGSIZE;

    /* unsupported gpio */
    if (gpion > ARRAY_SIZE(io->gpios)) {
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
        return 0;
    }

    /* in addition to what was requested, always initialize as an input */
    gpio_flags_t gpio_flags = GPIO_INPUT;
    if ((flags & gpio_flags_output) != 0) gpio_flags |= GPIO_OUTPUT;
    if ((flags & gpio_flags_active_low) != 0) gpio_flags |= GPIO_ACTIVE_LOW;
    if ((flags & gpio_flags_pull_up) != 0) gpio_flags |= GPIO_PULL_UP;
    if ((flags & gpio_flags_pull_down) != 0) gpio_flags |= GPIO_PULL_DOWN;
    if ((flags & gpio_flags_open_drain) != 0) gpio_flags |= GPIO_OPEN_DRAIN;

    /* can't have pull up and pull down at the same time */
    if ((gpio_flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) == (GPIO_PULL_UP | GPIO_PULL_DOWN)) {
        LOG_WRN("don't support pull up and pull down on single gpio");
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
        return 0;
    }
    /* also can't be open drain without being an output */
    if ((gpio_flags & (GPIO_OUTPUT | GPIO_OPEN_DRAIN)) == GPIO_OPEN_DRAIN) {
        LOG_WRN("open drain gpio must also be output");
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
        return 0;
    }

    if ((ret = gpio_pin_configure_dt(&io->gpios[gpion], gpio_flags)) < 0) {
        LOG_ERR("gpio%u initialize failed (%d)", gpion, ret);
        if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
        return 0;
    }

    if (ring_buf_put(&io->buf.response, &io_cmd_response_ok, 1) != 1) return -ENOBUFS;
    return 0;
}

/* request:
 * | UTF8 | U8   | U8  | U32?       |
 * | 0x12 | GPIO | Cmd | Wait State |
 * 
 * response:
 * | UTF8 | U8     | U8?      |
 * | 0x12 | Status | Read Val |
 */
int32_t io_handle_cmd_gpio_ctrl(struct io_driver *io) {
    int32_t ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_gpio_ctrl)) < 0) return ret;

    const uint8_t gpio_cmd_write_bit = BIT(0);
    const uint8_t gpio_cmd_write_out = BIT(1);
    const uint8_t gpio_cmd_toggle_out = BIT(2);
    const uint8_t gpio_cmd_read_in = BIT(3);
    const uint8_t gpio_cmd_wait_in = BIT(4);

    uint8_t gpion = 0;
    if (ring_buf_get(&io->buf.request, &gpion, 1) != 1) return -EMSGSIZE;
    uint8_t cmd = 0;
    if (ring_buf_get(&io->buf.request, &cmd, 1) != 1) return -EMSGSIZE;

    uint32_t wait_state = 0;
    if ((cmd & gpio_cmd_wait_in) != 0 && ring_buf_get_le32(&io->buf.request, &wait_state) < 0) return -EMSGSIZE;

    /* write new output value */
    if ((cmd & gpio_cmd_write_out) != 0) {
        if ((ret = gpio_pin_set_dt(&io->gpios[gpion], cmd & gpio_cmd_write_bit)) < 0) {
            LOG_ERR("gpio%u set failed (%d)", gpion, ret);
            if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
            return 0;
        }
    }

    /* toggle current output value */
    if ((cmd & gpio_cmd_toggle_out) != 0) {
        if ((ret = gpio_pin_toggle_dt(&io->gpios[gpion])) < 0) {
            LOG_ERR("gpio%u toggle failed (%d)", gpion, ret);
            if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
            return 0;
        }
    }

    /* read input value */
    uint8_t read_val = 0;
    if ((cmd & gpio_cmd_read_in) != 0) {
        if ((ret = gpio_pin_get_dt(&io->gpios[gpion])) < 0) {
            LOG_ERR("gpio%u get failed (%d)", gpion, ret);
            if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
            return 0;
        }

        read_val = ret;
    }

    /* wait for input to change to requested state */
    if ((cmd & gpio_cmd_wait_in) != 0) {
        /* lsb determines gpio state to wait for */
        uint32_t wait_val = wait_state & 0x1;
        /* remaining bits (shifted) determine milliseconds to wait up to */
        k_timepoint_t wait_timeout = sys_timepoint_calc(K_MSEC(wait_state >> 1));

        do {
            if ((ret = gpio_pin_get_dt(&io->gpios[gpion])) < 0) {
                LOG_ERR("gpio%u get failed (%d)", gpion, ret);
                if (ring_buf_put(&io->buf.response, &io_cmd_response_eio, 1) != 1) return -ENOBUFS;
                return 0;
            }
            if (ret == wait_val) { break; }
            k_sleep(K_MSEC(1));
        } while (!sys_timepoint_expired(wait_timeout));
        if (sys_timepoint_expired(wait_timeout)) {
            if (ring_buf_put(&io->buf.response, &io_cmd_response_etimedout, 1) != 1) return -ENOBUFS;
            return 0;
        }
    }

    /* if we have made it here without returning, our command was successful */
    if (ring_buf_put(&io->buf.response, &io_cmd_response_ok, 1) != 1) return -ENOBUFS;
    /* if applicable, also respond with GPIO read value */
    if ((cmd & gpio_cmd_read_in) != 0) {
        if (ring_buf_put(&io->buf.response, &read_val, 1) != 1) return -ENOBUFS;
    }

    return 0;
}
