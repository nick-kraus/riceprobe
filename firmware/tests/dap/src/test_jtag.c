#include <zephyr/ztest.h>

#include "dap_io.h"
#include "dap_target_emul.h"
#include "dap_transport.h"
#include "util/gpio.h"

ZTEST(dap, test_jtag_sequence) {
    /* start in JTAG mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    dap_target_emul_start();

    /* 1 sequence: no tdo capture, tms = 1, 32 cycles */
    assert_dap_command_expect("\x14\x01\x60\x13\x57\x9b\xdf", "\x14\x00");
    assert_dap_target_clk_cycles_equal(32);
    assert_dap_target_tms_swdio_equal("\xff\xff");
    assert_dap_target_tdi_equal("\x13\x57\x9b\xdf");

    /* 2 sequences:
     *      1: tdo capture, tms = 0, 8 cycles
     *      2: no tdo capture, tms = 1, 8 cycles */
    dap_target_emul_reset();
    dap_target_emul_set_tdo_out("\x39\x00", 2);
    assert_dap_command_expect("\x14\x02\x88\x55\x48\xaa", "\x14\x00\x39");
    assert_dap_target_clk_cycles_equal(16);
    assert_dap_target_tms_swdio_equal("\x00\xff");
    assert_dap_target_tdi_equal("\x55\xaa");

    /* incomplete command request */
    assert_dap_command_expect("\x14", "\xff");
    assert_dap_command_expect("\x14\x01\x88", "\xff");
    assert_dap_command_expect("\x14\x02\x88\x55", "\xff");
}

ZTEST(dap, test_jtag_configure_idcode) {
    /* start in JTAG mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");

    /* only support 4 devices in the jtag chain, more will be an error */
    assert_dap_command_expect("\x15\x05\x01\x01\x01\x01\x01", "\x15\xff");

    /* two devices in chain, first 4 bits, second 5 bits */
    assert_dap_command_expect("\x15\x02\x04\x05", "\x15\x00");
    /* idcode index must be within configured jtag tap, this should error */
    assert_dap_command_expect("\x16\x02", "\x16\xff");
    /* also can't get idcode when probe is swd configured */
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    assert_dap_command_expect("\x16\x01", "\x16\xff");
    assert_dap_command_expect("\x02\x02", "\x02\x02");

    /* now actually acquire an emulated idcode */
    dap_target_emul_start();
    dap_target_emul_set_tdo_out("\x00\x00\x00\x12\x34\x56\x78\x00", 8);
    /* 6 extra clock cycles to byte-align the tdo values above */
    assert_dap_command_expect("\x14\x01\x06\x00", "\x14\x00");
    assert_dap_command_expect("\x16\x00", "\x16\x00\x12\x34\x56\x78");
    assert_dap_target_clk_cycles_equal(58);
    assert_dap_target_tms_swdio_equal("\xc0\x00\x2c\x00\x00\x00\x80\x01");

    /* idcode for further device indexes require extra */
    dap_target_emul_reset();
    dap_target_emul_set_tdo_out("\x00\x00\x00\x12\x34\x56\x78\x00", 8);
    /* 5 extra clock cycles to byte-align the tdo values above */
    assert_dap_command_expect("\x14\x01\x05\x00", "\x14\x00");
    assert_dap_command_expect("\x16\x01", "\x16\x00\x12\x34\x56\x78");
    assert_dap_target_clk_cycles_equal(58);
    /* shifted one bit from the previous example */
    assert_dap_target_tms_swdio_equal("\x60\x00\x16\x00\x00\x00\x80\x01");

    /* a chain with longer instruction lengths requires more cycles to produce the idcode */
    dap_target_emul_reset();
    assert_dap_command_expect("\x15\x03\x0a\x0b\x0c", "\x15\x00");
    dap_target_emul_set_tdo_out("\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78\x00", 11);
    /* 4 extra clock cycles to byte-align the tdo values above */
    assert_dap_command_expect("\x14\x01\x04\x00", "\x14\x00");
    assert_dap_command_expect("\x16\x02", "\x16\x00\x12\x34\x56\x78");
    /* requires 24 cycles more than previous attempt, because additional tap length */
    assert_dap_target_clk_cycles_equal(82);

    /* incomplete command request */
    assert_dap_command_expect("\x15", "\xff");
    assert_dap_command_expect("\x15\x03\x04\x05", "\xff");
    assert_dap_command_expect("\x16", "\xff");
}
