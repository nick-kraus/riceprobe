#ifndef __DAP_TRANSPORT_H__
#define __DAP_TRANSPORT_H__

#include <zephyr/ztest.h>

#include "util/print.h"

void dap_transport_command(uint8_t *request, size_t request_len, uint8_t **response, size_t *response_len);

/* always expects '_request' and '_expect' in string forms, so make sure to pass on
 * their null terminator when calculating size. */
#define assert_dap_command_expect(_request, _expect)                                    \
    do {                                                                                \
        uint8_t req[] = _request;                                                       \
        uint8_t *resp;                                                                  \
        size_t resp_len;                                                                \
        dap_transport_command(req, sizeof(req) - 1, &resp, &resp_len);                  \
        uint8_t exp[] = _expect;                                                        \
        if (memcmp(resp, exp, MIN(sizeof(exp) - 1, resp_len)) != 0) {                   \
            hex_diff_printk(resp, exp, MIN(sizeof(exp) - 1, resp_len));                 \
            zassert(false, "\e[0;31mresponse does not match expected value\e[0m\n");    \
        }                                                                               \
    } while(0)

#endif /* __DAP_TRANSPORT_H__ */
