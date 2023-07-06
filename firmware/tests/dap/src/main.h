#ifndef __MAIN_H__
#define __MAIN_H__

#include <zephyr/drivers/gpio.h>

#include "tests/util.h"

struct dap_fixture {
    struct {
        struct gpio_dt_spec tck_swclk;
        struct gpio_dt_spec tms_swdio;
        struct gpio_dt_spec tdo;
        struct gpio_dt_spec tdi;
        struct gpio_dt_spec nreset;
        struct gpio_dt_spec vtref;
        struct gpio_dt_spec led_connect;
        struct gpio_dt_spec led_running;
    } io;
};

void dap_configure(bool enable);
void dap_command(uint8_t *request, size_t request_len, uint8_t **response, size_t *response_len);

/* always expects '_request' and '_expect' in string forms, so make sure to pass on
 * their null terminator when calculating size. */
#define zassert_dap_command_expect(_request, _expect)                                   \
    do {                                                                                \
        uint8_t req[] = _request;                                                       \
        uint8_t *resp;                                                                  \
        size_t resp_len;                                                                \
        dap_command(req, sizeof(req) - 1, &resp, &resp_len);                            \
        uint8_t exp[] = _expect;                                                        \
        if (memcmp(resp, exp, MIN(sizeof(exp) - 1, resp_len)) != 0) {                   \
            hex_diff_printk(resp, exp, MIN(sizeof(exp) - 1, resp_len));                 \
            zassert(false, "\e[0;31mresponse does not match expected value\e[0m\n");    \
        }                                                                               \
    } while(0)

#endif /* __MAIN_H__ */
