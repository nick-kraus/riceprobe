#include <pinctrl_soc.h>
#include <zephyr/kernel.h>

/* one 'pin' for each possible io */
static pinctrl_soc_pin_t pinctrl_pinmux[256];

static void pinctrl_configure_pin(pinctrl_soc_pin_t pin) {
    uint32_t pin_num = (pin >> SIM_PINMUX_PIN_SHIFT) & SIM_PINMUX_PIN_MASK;
    pinctrl_pinmux[pin_num] = pin & ~(SIM_PINMUX_PIN_MASK << SIM_PINMUX_PIN_SHIFT);
}

int32_t pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg) {
    ARG_UNUSED(reg);

    for (uint8_t i = 0; i < pin_cnt; i++) {
        pinctrl_configure_pin(*pins++);
    }

    return 0;
}

uint8_t pinctrl_pinmux_get_flags(uint8_t pin) {
    return (uint8_t) (pinctrl_pinmux[pin] >> SIM_PINMUX_FLAG_SHIFT) & SIM_PINMUX_FLAG_MASK;
}

uint8_t pinctrl_pinmux_get_func(uint8_t pin) {
    return (uint8_t) (pinctrl_pinmux[pin] >> SIM_PINMUX_FUNC_SHIFT) & SIM_PINMUX_FUNC_MASK;
}
