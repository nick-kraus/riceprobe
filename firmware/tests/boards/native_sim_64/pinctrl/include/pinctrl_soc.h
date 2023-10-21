#ifndef __PINCTRL_SOC_H__
#define __PINCTRL_SOC_H__

#include <dt-bindings/pinctrl/pinctrl_sim.h>
#include <zephyr/devicetree.h>
#include <zephyr/types.h>

typedef uint32_t pinctrl_soc_pin_t;

#define Z_PINCTRL_STATE_PIN_INIT(node_id, prop, idx) (DT_PROP_BY_IDX(node_id, prop, idx)),

#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop) {                                          \
    DT_FOREACH_CHILD_VARGS(                                                                 \
        DT_PHANDLE(node_id, prop), DT_FOREACH_PROP_ELEM, pins, Z_PINCTRL_STATE_PIN_INIT     \
    )}

int32_t pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg);

/* returns the pinmux function for a given pin */
uint32_t pinctrl_pinmux_get_func(uint8_t pin);

/* assertions to check that a certain pin is configured to a given function */
#define assert_pinctrl_sim_func(_pin, _exp_func)                            \
    zassert(                                                                \
        pinctrl_pinmux_get_func(_pin) == _exp_func,                         \
        "pinctrl_pinmux_get_func(" #_pin ") not equal to " #_exp_func,      \
        "\t\e[0;31mcurrent pin function is 0x%x, expected 0x%x\e[0m\n",     \
        pinctrl_pinmux_get_func(_pin), _exp_func                            \
    )

#endif /* __PINCTRL_SOC_H__ */
