#ifndef __DT_PINCTRL_POSIX_H__
#define __DT_PINCTRL_POSIX_H__

/* pin number field */
#define POSIX_PINMUX_PIN_MASK       (0xff)
#define POSIX_PINMUX_PIN_SHIFT      (0)
/* function field */
#define POSIX_PINMUX_FUNC_MASK      (0xff)
#define POSIX_PINMUX_FUNC_SHIFT     (POSIX_PINMUX_PIN_SHIFT + 8)

/* options for the pinmux function field */
#define POSIX_PINMUX_FUNC_GPIO      (0)
#define POSIX_PINMUX_FUNC_UART      (1)

/* pinmux bit field declaration */
#define POSIX_PINMUX(pin, func)                                         \
    ((((pin) & POSIX_PINMUX_PIN_MASK) << POSIX_PINMUX_PIN_SHIFT) |      \
    (((func) & POSIX_PINMUX_FUNC_MASK) << POSIX_PINMUX_FUNC_SHIFT))

#endif /* __DT_PINCTRL_POSIX_H__ */
