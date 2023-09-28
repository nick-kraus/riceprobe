#include <pinctrl_soc.h>
#include <zephyr/kernel.h>

/* one 'pin' for each possible io */
static pinctrl_soc_pin_t pinctrl_pinmux[256];

static void pinctrl_configure_pin(pinctrl_soc_pin_t pin) {
    uint32_t pin_num = (pin >> POSIX_PINMUX_PIN_SHIFT) & POSIX_PINMUX_PIN_MASK;
    pinctrl_pinmux[pin_num] = pin;
}

int32_t pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg) {
    ARG_UNUSED(reg);

    for (uint8_t i = 0; i < pin_cnt; i++) {
        pinctrl_configure_pin(*pins++);
    }

    return 0;
}

uint32_t pinctrl_pinmux_get_func(uint8_t pin) {

    pinctrl_soc_pin_t pinmux = pinctrl_pinmux[pin];
    return (pinmux >> POSIX_PINMUX_FUNC_SHIFT) & POSIX_PINMUX_FUNC_MASK;
}
