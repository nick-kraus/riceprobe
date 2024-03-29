#ifndef __DAP_EMUL_H__
#define __DAP_EMUL_H__

#include <string.h>

/* initialize emulator data structures */
void dap_emul_init(void);

/* reset emulator internal state */
void dap_emul_reset(void);

/* (reset internal state and) start emulator */
void dap_emul_start(void);

/* ends emulator run */
void dap_emul_end(void);

/* set tdo data to be sent to probe input (default 0) */
void dap_emul_set_tdo_in(uint8_t *data, size_t len);

/* set tms/swdio data to be sent to probe input (defualt 0) */
void dap_emul_set_tms_swdio_in(uint8_t *data, size_t len);

/* returns values from the current emulator run, but preferred to use the assertion helpers below */
uint16_t dap_emul_get_clk_cycles(void);
uint64_t dap_emul_get_avg_clk_period(void);
uint8_t* dap_emul_get_tms_swdio_dir(void);
uint8_t* dap_emul_get_tms_swdio_out(void);
uint8_t* dap_emul_get_tdi_out(void);

#define assert_dap_emul_clk_cycles(_exp)                                                                \
    zassert(                                                                                            \
        dap_emul_get_clk_cycles() == _exp,                                                              \
        "dap_emul_get_clk_cycles() not equal to " #_exp,                                                \
        "\t\e[0;31m%u received clock cycles, expected %u\e[0m\n",                                       \
        dap_emul_get_clk_cycles(), _exp                                                                 \
    )

#define assert_dap_emul_clk_period(_exp)                                                                \
    /* a little bit of play for any rounding issues */                                                  \
    zassert_between_inclusive(dap_emul_get_avg_clk_period(), _exp - 1, _exp + 1)

/* always expects '_exp' in string forms, so make sure to skip the null terminator when calculating size. */
#define assert_dap_emul_tms_swdio_dir(_exp)                                                             \
    do {                                                                                                \
        uint8_t exp[] = _exp;                                                                           \
        if (memcmp(dap_emul_get_tms_swdio_dir(), exp, sizeof(_exp) - 1) != 0) {                         \
            hex_diff_printk(dap_emul_get_tms_swdio_dir(), exp, sizeof(_exp) - 1);                       \
            zassert(false, "\e[0;31mio direction of tms/swdio does not match expected values\e[0m\n");  \
        }                                                                                               \
    } while (0)

/* always expects '_exp' in string forms, so make sure to skip the null terminator when calculating size. */
#define assert_dap_emul_tms_swdio_out(_exp)                                                             \
    do {                                                                                                \
        uint8_t exp[] = _exp;                                                                           \
        if (memcmp(dap_emul_get_tms_swdio_out(), exp, sizeof(_exp) - 1) != 0) {                         \
            hex_diff_printk(dap_emul_get_tms_swdio_out(), exp, sizeof(_exp) - 1);                       \
            zassert(false, "\e[0;31mdata output on tms/swdio does not match expected values\e[0m\n");   \
        }                                                                                               \
    } while (0)

/* always expects '_exp' in string forms, so make sure to skip the null terminator when calculating size. */
#define assert_dap_emul_tdi_out(_exp)                                                                   \
    do {                                                                                                \
        uint8_t exp[] = _exp;                                                                           \
        if (memcmp(dap_emul_get_tdi_out(), exp, sizeof(_exp) - 1) != 0) {                               \
            hex_diff_printk(dap_emul_get_tdi_out(), exp, sizeof(_exp) - 1);                             \
            zassert(false, "\e[0;31mdata output on tdi does not match expected values\e[0m\n");         \
        }                                                                                               \
    } while (0)

#endif /* __DAP_EMUL_H__ */
