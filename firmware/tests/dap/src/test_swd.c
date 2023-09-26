#include <zephyr/ztest.h>

#include "dap_io.h"
#include "dap_emul.h"
#include "dap_transport.h"
#include "util/gpio.h"

ZTEST(dap, test_swd_sequence) {
    /* start in SWD mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* set swclk clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");

    /* 2 SWD sequences: 
     *      32 cycles, SWDIO output
     *      64 cycles (encoded as '0'), SWDIO input
     */
    dap_emul_start();
    dap_emul_set_tms_swdio_in("\x02\x13\x24\x35\x46\x57\x68\x79\x8a\x9b\xac\xbd", 12);
    assert_dap_command_expect("\x1d\x02\x20\x12\x34\x56\x78\x80", "\x1d\x00\x46\x57\x68\x79\x8a\x9b\xac\xbd");
    assert_dap_emul_clk_cycles(96);
    assert_dap_emul_tms_swdio_dir("\x00\x00\x00\x00\xff\xff\xff\xff\xff\xff\xff\xff");
    assert_dap_emul_tms_swdio_out("\x12\x34\x56\x78\x46\x57\x68\x79\x8a\x9b\xac\xbd");

    /* 2 SWD sequences: 
     *      7 cycles, SWDIO output
     *      13 cycles, SWDIO input
     */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x19\x08", 3);
    assert_dap_command_expect("\x1d\x02\x07\xff\x8d", "\x1d\x00\x32\x10");
    assert_dap_emul_clk_cycles(20);
    assert_dap_emul_tms_swdio_dir("\x80\xff\x0f");
    assert_dap_emul_tms_swdio_out("\x7f\x19\x08");

    /* if we are configured as something other than SWD, make sure we process all command
     * data and can run further commands successfully */
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    assert_dap_command_expect("\x1d\x02\x07\xff\x8d", "\x1d\xff");
    assert_dap_command_expect("\x02\x01", "\x02\x01");

    /* incomplete command request */
    assert_dap_command_expect("\x1d", "\xff");
    assert_dap_command_expect("\x1d\x01", "\xff");
    assert_dap_command_expect("\x1d\x01\x08", "\xff");
    assert_dap_command_expect("\x1d\x03\x08\xff\x80", "\xff");
}
