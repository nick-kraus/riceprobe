#ifndef __IO_PRIV_H__
#define __IO_PRIV_H__

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include "io/transport.h"

/* size of the internal buffers in bytes */
#define IO_RING_BUF_SIZE        (2048)
/* maximum size for any single transport transfer */
#define IO_MAX_PACKET_SIZE      (512)

/* possible status responses to commands */
static const uint8_t io_cmd_response_ok = 0x00;
static const uint8_t io_cmd_response_eio = 0xfe;
static const uint8_t io_cmd_response_enotsup = 0xff;

/* different functionality types a pin can be mapped to */
enum io_func {
    IO_PIN_FUNC_GPIO,
    IO_PIN_FUNC_UART,
    IO_PIN_FUNC_I2C,
};

struct io_driver_pin_function {
    uint8_t pin;
    uint8_t function;
    uint8_t index;
    pinctrl_soc_pin_t pinctrl;
};

#define UTIL_ONE(...) 1

/* ensure we have one and exactly one io driver in the devicetree */
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(rice_io) == 1);
#define IO_DT_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(rice_io)
/* ensure the number of pins in the devicetree is equal to the number of GPIO phandles */
BUILD_ASSERT(DT_FOREACH_CHILD_SEP(IO_DT_NODE, UTIL_ONE, (+)) == DT_PROP_LEN(IO_DT_NODE, gpios));

/* important that these don't both use the DT_FOREACH_CHILD_SEP macro, which will
 * stop being expanded after the first time (without macro hackery) */
#define IO_PINCTRL_PIN_FUNCS_CNT(_node) DT_FOREACH_CHILD_SEP(_node, UTIL_ONE, (+))
#define IO_PINCTRL_FUNCS_CNT(...) DT_FOREACH_CHILD_STATUS_OKAY_SEP(IO_DT_NODE, IO_PINCTRL_PIN_FUNCS_CNT, (+))

/* important that these don't both use the DT_FOREACH_CHILD macro, which will
 * stop being expanded after the first time (without macro hackery) */
#define IO_PINCTRL_FUNC_ONE(_node) {                                            \
    .pin = DT_REG_ADDR(DT_PARENT(_node)),                                       \
    .function = UTIL_CAT(IO_PIN_FUNC_, DT_STRING_UPPER_TOKEN(_node, func)),     \
    .index = DT_PROP(_node, idx),                                               \
    .pinctrl = (pinctrl_soc_pin_t) Z_PINCTRL_STATE_PINS_INIT(_node, pinctrl),   \
},
#define IO_PINCTRL_PIN_FUNCS(_node) DT_FOREACH_CHILD(_node, IO_PINCTRL_FUNC_ONE)
#define IO_PINCTRL_FUNCS_DECLARE(...) { DT_FOREACH_CHILD_STATUS_OKAY(IO_DT_NODE, IO_PINCTRL_PIN_FUNCS) }

#define IO_GPIOS_CNT(...) DT_PROP_LEN(IO_DT_NODE, gpios)
#define IO_GPIOS_DECLARE(...) { DT_FOREACH_PROP_ELEM_SEP(IO_DT_NODE, gpios, GPIO_DT_SPEC_GET_BY_IDX, (,)) }

/* macros to dynamically adjust pinctrl pin flags based on provided devicetree nodes */
#define IO_PINCTRL_FLAG_PULLUP      ((pinctrl_soc_pin_t) Z_PINCTRL_STATE_PINS_INIT(IO_DT_NODE, pinctrl_pull_up))
#define IO_PINCTRL_FLAG_PULLDOWN    ((pinctrl_soc_pin_t) Z_PINCTRL_STATE_PINS_INIT(IO_DT_NODE, pinctrl_pull_down))
#define IO_PINCTRL_FLAG_OPENDRAIN   ((pinctrl_soc_pin_t) Z_PINCTRL_STATE_PINS_INIT(IO_DT_NODE, pinctrl_open_drain))

struct io_driver {
    struct io_driver_pin_function pinctrls[IO_PINCTRL_FUNCS_CNT()];
    struct gpio_dt_spec gpios[IO_GPIOS_CNT()];

    struct {
        uint8_t request_bytes[IO_RING_BUF_SIZE];
        struct ring_buf request;
        uint8_t response_bytes[IO_RING_BUF_SIZE];
        struct ring_buf response;
    } buf;

    struct io_transport *transport;
};

/* command ids */
static const uint32_t io_cmd_info = 0x01;
static const uint32_t io_cmd_multi = 0x02;
static const uint32_t io_cmd_queue = 0x03;
static const uint32_t io_cmd_delay = 0x04;
static const uint32_t io_cmd_pins_caps = 0x09;
static const uint32_t io_cmd_pins_default = 0x0a;
static const uint32_t io_cmd_pins_cfg = 0x0b;

/* command handlers */
int32_t io_handle_cmd_info(struct io_driver *io);
int32_t io_handle_cmd_delay(struct io_driver *io);
int32_t io_handle_cmd_pins_caps(struct io_driver *io);
int32_t io_handle_cmd_pins_default(struct io_driver *io);
int32_t io_handle_cmd_pins_cfg(struct io_driver *io);

/** @brief configure a io_driver pinctrl state */
static inline int32_t io_configure_pin(const pinctrl_soc_pin_t *pinctrl_state) {
    return pinctrl_configure_pins(pinctrl_state, 1, PINCTRL_REG_NONE);
}

#endif /* __IO_PRIV_H__ */
