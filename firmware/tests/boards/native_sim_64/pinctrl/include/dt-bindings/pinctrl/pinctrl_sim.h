#ifndef __DT_PINCTRL_SIM_H__
#define __DT_PINCTRL_SIM_H__

/* pin number field */
#define SIM_PINMUX_PIN_MASK       (0xff)
#define SIM_PINMUX_PIN_SHIFT      (0)
/* function field */
#define SIM_PINMUX_FUNC_MASK      (0xff)
#define SIM_PINMUX_FUNC_SHIFT     (SIM_PINMUX_PIN_SHIFT + 8)

/* options for the pinmux function field */
#define SIM_PINMUX_FUNC_GPIO      (0)
#define SIM_PINMUX_FUNC_UART      (1)

/* pinmux bit field declaration */
#define SIM_PINMUX(pin, func)                                       \
    ((((pin) & SIM_PINMUX_PIN_MASK) << SIM_PINMUX_PIN_SHIFT) |      \
    (((func) & SIM_PINMUX_FUNC_MASK) << SIM_PINMUX_FUNC_SHIFT))

#endif /* __DT_PINCTRL_SIM_H__ */
