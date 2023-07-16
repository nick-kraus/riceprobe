#ifndef __DAP_TARGET_EMUL_H__
#define __DAP_TARGET_EMUL_H__

#include <string.h>

/* initialize target emulator data structures */
void dap_target_emul_init(void);

/* reset target emulator internal state */
void dap_target_emul_reset(void);

/* (reset internal state and) start target emulator */
void dap_target_emul_start(void);

/* ends target emulator run */
void dap_target_emul_end(void);

/* returns values from the current emulator run, but preferred to use the assertion helpers below */
uint64_t dap_target_emul_avg_clk_period(void);
uint8_t* dap_target_emul_tms_swdio_val(void);

#define assert_dap_target_clk_period_equal(_val)                                                \
    /* a little bit of play for any rounding issues */                                          \
    zassert_between_inclusive(dap_target_emul_avg_clk_period(), _val - 1, _val + 1)

/* always expects '_expect' in string forms, so make sure to skip the null terminator when calculating size. */
#define assert_dap_target_tms_swdio_equal(_expect)                                              \
    do {                                                                                        \
        uint8_t exp[] = _expect;                                                                \
        if (memcmp(dap_target_emul_tms_swdio_val(), exp, sizeof(_expect) - 1) != 0) {           \
            hex_diff_printk(dap_target_emul_tms_swdio_val(), exp, sizeof(_expect) - 1);         \
            zassert(false, "\e[0;31mtms/swdio capture does not match expected values\e[0m\n");  \
        }                                                                                       \
    } while (0)

#endif /* __DAP_TARGET_EMUL_H__ */
