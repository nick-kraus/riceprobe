#ifndef __DAP_TARGET_EMUL_H__
#define __DAP_TARGET_EMUL_H__

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

#define assert_dap_target_clk_period_equal(_val)                                        \
    /* a little bit of play for any rounding issues */                                  \
    zassert_between_inclusive(dap_target_emul_avg_clk_period(), _val - 1, _val + 1)

#endif /* __DAP_TARGET_EMUL_H__ */
