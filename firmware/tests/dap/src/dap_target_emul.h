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

/* set tdo output data for target to probe during emulation (default 0) */
void dap_target_emul_set_tdo_out(uint8_t *data, size_t len);

/* returns values from the current emulator run, but preferred to use the assertion helpers below */
uint16_t dap_target_emul_clk_cycles(void);
uint64_t dap_target_emul_avg_clk_period(void);
uint8_t* dap_target_emul_tms_swdio_in(void);
uint8_t* dap_target_emul_tdi_in(void);

#define assert_dap_target_clk_cycles_equal(_exp)                                                \
    zassert(                                                                                    \
        dap_target_emul_clk_cycles() == _exp,                                                   \
        "dap_target_emul_clk_cycles() not equal to " #_exp,                                     \
        "\t\e[0;31m%u received clock cycles, expected %u\e[0m\n",                               \
        dap_target_emul_clk_cycles(), _exp                                                      \
    )

#define assert_dap_target_clk_period_equal(_exp)                                                \
    /* a little bit of play for any rounding issues */                                          \
    zassert_between_inclusive(dap_target_emul_avg_clk_period(), _exp - 1, _exp + 1)

/* always expects '_exp' in string forms, so make sure to skip the null terminator when calculating size. */
#define assert_dap_target_tms_swdio_equal(_exp)                                                 \
    do {                                                                                        \
        uint8_t exp[] = _exp;                                                                   \
        if (memcmp(dap_target_emul_tms_swdio_in(), exp, sizeof(_exp) - 1) != 0) {               \
            hex_diff_printk(dap_target_emul_tms_swdio_in(), exp, sizeof(_exp) - 1);             \
            zassert(false, "\e[0;31mtms/swdio capture does not match expected values\e[0m\n");  \
        }                                                                                       \
    } while (0)

/* always expects '_exp' in string forms, so make sure to skip the null terminator when calculating size. */
#define assert_dap_target_tdi_equal(_exp)                                                       \
    do {                                                                                        \
        uint8_t exp[] = _exp;                                                                   \
        if (memcmp(dap_target_emul_tdi_in(), exp, sizeof(_exp) - 1) != 0) {                     \
            hex_diff_printk(dap_target_emul_tdi_in(), exp, sizeof(_exp) - 1);                   \
            zassert(false, "\e[0;31mtdi capture does not match expected values\e[0m\n");        \
        }                                                                                       \
    } while (0)

#endif /* __DAP_TARGET_EMUL_H__ */
