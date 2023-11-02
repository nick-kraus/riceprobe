#ifndef __UTIL_GPIO_H__
#define __UTIL_GPIO_H__

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

/* assert an emulated gpio output is currently set to a certain value */
#define assert_gpio_emul_output_val(_io, _val)                      \
    zassert_equal(gpio_emul_output_get((_io)->port, (_io)->pin), (_val))

/* set an emulated input to a specified value */
#define assert_gpio_emul_input_set(_io, _val)                       \
    zassert_ok(gpio_emul_input_set((_io)->port, (_io)->pin, (_val)))

/* assert an emulated gpio has a given set of flags */
#define assert_gpio_emul_has_flag(_io, _flag)                       \
    do {                                                            \
        gpio_flags_t flags;                                         \
        gpio_emul_flags_get((_io)->port, (_io)->pin, &flags);       \
        zassert_equal(flags & (_flag), (_flag));                    \
    } while (0)

/* assert an emulated gpio DOES NOT have a given set of flags */
#define assert_gpio_emul_not_flag(_io, _flag)                       \
    do {                                                            \
        gpio_flags_t flags;                                         \
        gpio_emul_flags_get((_io)->port, (_io)->pin, &flags);       \
        zassert_not_equal(flags & (_flag), (_flag));                \
    } while (0)

#endif /* __UTIL_GPIO_H__ */
