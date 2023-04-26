#ifndef __TEST_UTIL_H__
#define __TEST_UTIL_H__

#include <ctype.h>

#if CONFIG_GPIO_EMUL

#include <zephyr/drivers/gpio/gpio_emul.h>

static inline int32_t gpio_emul_input_set_dt(struct gpio_dt_spec *gpio, int32_t val) {
    return gpio_emul_input_set(gpio->port, gpio->pin, val);
}

static inline int32_t gpio_emul_output_get_dt(struct gpio_dt_spec *gpio) {
    return gpio_emul_output_get(gpio->port, gpio->pin);
}

static inline int32_t gpio_emul_flags_get_dt(struct gpio_dt_spec *gpio, gpio_flags_t *flags) {
    return gpio_emul_flags_get(gpio->port, gpio->pin, flags);
}

#endif /* CONFIG_GPIO_EMUL */

/* prints two buffers as a hexdump, while highlighting their differences */
static inline void hex_diff_printk(uint8_t *received, uint8_t *expected, size_t len) {
    for (uint8_t b = 0; b < 2; b++) {
        uint8_t *buf = b == 0 ? received : expected;
        printk("%s:\n", b == 0 ? "received" : "expected");

        for (size_t i = 0; i < len; i += 16) {
            printk("\t%04x:  ", i);

            /* hex representation */
            for (uint8_t j = 0; j < 16; j++) {
                if (i + j < len) {
                    if (received[i + j] != expected[i + j]) printk("%s", b == 0 ? "\e[0;31m" : "\e[0;32m");
                    printk("%02x \e[0m", buf[i + j]);
                } else {
                    printk("   ");
                }
            }
            printk("  ");

            /* ascii representation */
            for (uint8_t j = 0; j < 16; j++) {
                if (i + j < len) {
                    if (received[i + j] != expected[i + j]) printk("%s", b == 0 ? "\e[0;31m" : "\e[0;32m");
                    printk("%c\e[0m", isprint(buf[i + j]) ? buf[i + j] : '.');
                } else {
                    printk(" ");
                }
            }
            printk("\n");
        }
    }
}

#endif /* __TEST_UTIL_H__ */
