#ifndef __PINCTRL_SOC_H__
#define __PINCTRL_SOC_H__

#include <zephyr/devicetree.h>
#include <zephyr/types.h>

typedef uint32_t pinctrl_soc_pin_t;

static inline int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg) {
    return 0;
}

#define Z_PINCTRL_STATE_PIN_INIT(node_id, prop, idx) (DT_PROP_BY_IDX(node_id, prop, idx)),

#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop) {                                          \
    DT_FOREACH_CHILD_VARGS(                                                                 \
        DT_PHANDLE(node_id, prop), DT_FOREACH_PROP_ELEM, pins, Z_PINCTRL_STATE_PIN_INIT     \
    )}

#endif /* __PINCTRL_SOC_H__ */
