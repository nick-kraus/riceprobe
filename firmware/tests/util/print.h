#ifndef __UTIL_PRINT_H__
#define __UTIL_PRINT_H__

#include <ctype.h>

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

#endif /* __UTIL_PRINT_H__ */
