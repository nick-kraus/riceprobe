#include <zephyr/ztest.h>

#include "dap_io.h"
#include "dap_emul.h"
#include "dap_transport.h"
#include "util/gpio.h"

ZTEST(dap, test_configure_transfer_jtag) {
    /* start in JTAG mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    /* transfer before JTAG port selection is an error */
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x00\x00");
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    /* one device in chain, 5 bits long */
    assert_dap_command_expect("\x15\x01\x05", "\x15\x00");

    /* 8 idle cycles, 2 wait retries, 1 match retry */
    assert_dap_command_expect("\x04\x08\x02\x00\x01\x00", "\x04\x00");

    /* index beyond configured value is an error */
    assert_dap_command_expect("\x05\x02\x01" "\x04\x01\x00\x00\x05", "\x05\x00\x00");

    /* transfer read then write */
    dap_emul_start();
    dap_emul_set_tdo_in(
        "\x00\x80\x00\x00\x00\x00\x00\x80\x02\x04\x06\x08\x00"
        "\x80\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00", 
        26
    );
    assert_dap_command_expect("\x05\x00\x02" "\x06" "\x04\x01\x02\x03\x04", "\x05\x02\x01" "\x01\x02\x03\x04");
    assert_dap_emul_clk_cycles(203);
    assert_dap_emul_tms_swdio_out(
        "\x03\x0b\x00\x00\x00\x00\x03\x08\x00\x00\x00\x00\x03"
        "\x08\x00\x00\x00\x00\x03\x08\x00\x00\x00\x00\x03\x00"
    );
    assert_dap_emul_tdi_out(
        "\xaf\xf8\x00\x00\x00\x00\xf8\xff\xff\xff\xff\xff\xff"
        "\xbf\x02\x04\x06\x08\xf8\xff\xff\xff\xff\xff\xff\x07"
    );

    /* takes extra cycles on a multi-device jtag chain */
    dap_emul_reset();
    assert_dap_command_expect("\x15\x04\x04\x05\x02\x02", "\x15\x00");
    dap_emul_set_tdo_in("\x00\x00\x00\x01\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00", 15);
    assert_dap_command_expect("\x05\x01\x01" "\x04\x00\x00\x00\x00", "\x05\x01\x01");
    assert_dap_emul_clk_cycles(121);
    assert_dap_command_expect("\x15\x01\x05", "\x15\x00");

    /* wait responses terminate transfer after limit (2) is reached */
    dap_emul_reset();
    dap_emul_set_tdo_in("\x00\x40\x00\x80\x00\x00\x01\x00", 8);
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x00\x02");
    assert_dap_emul_clk_cycles(62);
    /* but succeed if limit isn't reached */
    dap_emul_reset();
    dap_emul_set_tdo_in("\x00\x40\x00\x00\x01\x00\x00\x00\x00\x00\x05\x08\x0c\x10\x00\x00", 16);
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x01\x01" "\x01\x02\x03\x04");
    assert_dap_emul_clk_cycles(124);

    /* write to match mask and make sure a proper read matches */
    dap_emul_reset();
    dap_emul_set_tdo_in("\x00\x80\x00\x00\x00\x00\x00\x80\x02\x04\x06\x08\x00\x00", 14);
    assert_dap_command_expect("\x05\x00\x02" "\x20\xff\x00\xff\x00" "\x1f\x01\x00\x03\x00", "\x05\x02\x01");
    assert_dap_emul_clk_cycles(107);
    /* incorrect matches will be retried up to limit (1) */
    dap_emul_reset();
    dap_emul_set_tdo_in("\x00\x80\x00\x00\x00\x00\x00\x80\x04\x04\x06\x08\x00\x80\x02\x04\x06\x08\x00\x00", 20);
    assert_dap_command_expect("\x05\x00\x02" "\x20\xff\x00\xff\x00" "\x1f\x01\x00\x03\x00", "\x05\x02\x01");
    assert_dap_emul_clk_cycles(155);
    /* incorrect matches above the limit will be an error */
    dap_emul_reset();
    dap_emul_set_tdo_in("\x00\x80\x00\x00\x00\x00\x00\x80\x04\x04\x06\x08\x00\x80\x04\x04\x06\x08\x00\x00", 20);
    assert_dap_command_expect("\x05\x00\x02" "\x20\xff\x00\xff\x00" "\x1f\x01\x00\x03\x00", "\x05\x01\x11");
    assert_dap_emul_clk_cycles(155);

    /* incomplete command request */
    assert_dap_command_expect("\x04", "\xff");
    assert_dap_command_expect("\x04\x01", "\xff");
    assert_dap_command_expect("\x04\x01\x02", "\xff");
    assert_dap_command_expect("\x04\x01\x02\x03\x04", "\xff");
    assert_dap_command_expect("\x05", "\xff");
    assert_dap_command_expect("\x05\x00", "\xff");
    assert_dap_command_expect("\x05\x00\x01", "\xff");
    assert_dap_command_expect("\x05\x00\x01\x04", "\xff");
    assert_dap_command_expect("\x05\x00\x01\x04\x01\x00", "\xff");
    assert_dap_command_expect("\x05\x00\x02\x04\x01\x00\x00\x05", "\xff");
}

ZTEST(dap, test_configure_transfer_swd) {
    /* start in SWD mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    /* transfer before SWD port selection is an error */
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x00\x00");
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    /* 1 turnaround cycle, no data phase on fault */
    assert_dap_command_expect("\x13\x00", "\x13\x00");

    /* 8 idle cycles, 0 wait retries, 0 match retry */
    assert_dap_command_expect("\x04\x08\x00\x00\x00\x00", "\x04\x00");

    /* transfer read then write */
    dap_emul_start();
    dap_emul_set_tms_swdio_in(
        "\x00\x12\x20\x30\x40\x10\x00\x80\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00",
        21
    );
    assert_dap_command_expect("\x05\x00\x02" "\x06" "\x04\x01\x02\x03\x04", "\x05\x02\x01" "\x01\x02\x03\x04");
    assert_dap_emul_clk_cycles(162);
    assert_dap_emul_tms_swdio_out(
        "\x8d\x12\x20\x30\x40\x10\x40\xaa\x08\x10\x18\x20\x08\xd0\x2b\x00\x00\x00\x00\x00\x00"
    );

    /* bad parity should report an error */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x02\x00\x00\x01\x00\x00", 7);
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x00\x08");
    assert_dap_emul_clk_cycles(54);

    /* increasing SWD turnaround period increases number of clock cycles */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x02\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00", 14);
    assert_dap_command_expect("\x05\x00\x01" "\x04\x01\x02\x03\x04", "\x05\x01\x01");
    assert_dap_emul_clk_cycles(108);
    /* 2 turnaround clock cycles */
    assert_dap_command_expect("\x13\x01", "\x13\x00");
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x04\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00", 14);
    assert_dap_command_expect("\x05\x00\x01" "\x04\x01\x00\x00\x05", "\x05\x01\x01");
    assert_dap_emul_clk_cycles(112);
    assert_dap_command_expect("\x13\x00", "\x13\x00");

    /* default config is to not generate data phase on WAIT/FAULT */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x04", 2);
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x00\x02");
    assert_dap_emul_clk_cycles(13);
    /* but we can configure to always generate data phase */
    dap_emul_reset();
    assert_dap_command_expect("\x13\x04", "\x13\x00");
    dap_emul_set_tms_swdio_in("\x00\x04\x00\x00\x00\x00\x80\x00\x00\x00\x00", 12);
    assert_dap_command_expect("\x05\x00\x01" "\x06", "\x05\x00\x02");
    assert_dap_emul_clk_cycles(46);
    /* same with write request instead of read request */
    dap_emul_reset();
    assert_dap_command_expect("\x13\x04", "\x13\x00");
    dap_emul_set_tms_swdio_in("\x00\x04\x00\x00\x00\x00\x80\x00\x00\x00\x00", 12);
    assert_dap_command_expect("\x05\x00\x01" "\x04\x01\x02\x03\x04", "\x05\x00\x02");
    assert_dap_emul_clk_cycles(46);
    assert_dap_command_expect("\x13\x00", "\x13\x00");

    /* index just gets ignored for SWD */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x02\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00", 13);
    assert_dap_command_expect("\x05\xff\x01" "\x04\x01\x00\x00\x05", "\x05\x01\x01");
    assert_dap_emul_clk_cycles(108);

    /* incomplete command request */
    assert_dap_command_expect("\x13", "\xff");
    assert_dap_command_expect("\x05", "\xff");
    assert_dap_command_expect("\x05\x00", "\xff");
    assert_dap_command_expect("\x05\x00\x01", "\xff");
    assert_dap_command_expect("\x05\x00\x01\x04", "\xff");
    assert_dap_command_expect("\x05\x00\x01\x04\x01\x00", "\xff");
    assert_dap_command_expect("\x05\x00\x02\x04\x01\x00\x00\x05", "\xff");
}

ZTEST(dap, test_transfer_block) {
    /* start in SWD mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    /* transfer before SWD port selection is an error */
    assert_dap_command_expect("\x06\x00\x01\x00\x06", "\x06\x00\x00");
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    /* 1 turnaround cycle, no data phase on fault */
    assert_dap_command_expect("\x13\x00", "\x13\x00");

    /* 0 idle cycles, 0 wait retries, 0 match retry */
    assert_dap_command_expect("\x04\x08\x00\x00\x00\x00", "\x04\x00");

    /* transfer block write */
    dap_emul_start();
    dap_emul_set_tms_swdio_in(
        "\x00\x02\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00"
        "\x20\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00",
        27
    );
    assert_dap_command_expect(
        "\x06\x00\x03\x00\x04" "\x01\x02\x03\x04" "\x01\x02\x03\x04" "\x01\x02\x03\x04",
        "\x06\x03\x00\x01"
    );
    assert_dap_emul_clk_cycles(216);

    /* transfer block read */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x12\x20\x30\x40\x10\x00\x80\x04\x08\x0c\x10\x04\x00", 14);
    assert_dap_command_expect("\x06\x00\x02\x00\x06", "\x06\x02\x00\x01" "\x01\x02\x03\x04" "\x01\x02\x03\x04");
    assert_dap_emul_clk_cycles(108);

    /* bad parity should report an error */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x02\x00\x00\x01\x00\x00", 7);
    assert_dap_command_expect("\x06\x00\x04\x00\x06", "\x06\x00\x00\x08");
    assert_dap_emul_clk_cycles(54);

    /* fault response should clean up remaining command request */
    dap_emul_reset();
    dap_emul_set_tms_swdio_in("\x00\x08", 2);
    assert_dap_command_expect("\x06\x00\x02\x00\x04" "\x01\x02\x03\x04" "\x01\x02\x03\x04", "\x06\x00\x00\x04");
    assert_dap_emul_clk_cycles(13);

    /* JTAG instead of SWD */
    dap_emul_end();
    assert_dap_command_expect("\x03", "\x03\x00");
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* one device in chain, 5 bits long */
    assert_dap_command_expect("\x15\x01\x05", "\x15\x00");
    /* index beyond configured value is an error */
    assert_dap_command_expect("\x06\x01\x01\x00\x06", "\x06\x00\x00\x00");
    /* JTAG read */
    dap_emul_start();
    dap_emul_set_tdo_in("\x00\x80\x00\x00\x00\x00\x00\x80\x02\x04\x06\x08\x00\x00", 14);
    assert_dap_command_expect("\x06\x00\x01\x00\x06", "\x06\x01\x00\x01" "\x01\x02\x03\x04");
    assert_dap_emul_clk_cycles(107);

    /* incomplete command request */
    assert_dap_command_expect("\x06", "\xff");
    assert_dap_command_expect("\x06\x00\x01", "\xff");
    assert_dap_command_expect("\x06\x00\x01\x00\x04", "\xff");
    assert_dap_command_expect("\x06\x00\x02\x00\x04\x01\x02\x03\x04", "\xff");
}

ZTEST(dap, test_transfer_abort) {
    /* start in SWD mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    /* 1 turnaround cycle, no data phase on fault */
    assert_dap_command_expect("\x13\x00", "\x13\x00");

    /* 0 idle cycles, 0 wait retries, 0 match retry */
    assert_dap_command_expect("\x04\x08\x00\x00\x00\x00", "\x04\x00");

    /* command doesn't actually do anything currently, and shouldn't have a response */
    assert_dap_command_expect("\x07", "");
    /* but make sure we can run commands after without issue */
    assert_dap_command_expect("\x13\x00", "\x13\x00");
}

ZTEST(dap, test_transfer_write_abort) {
    /* start in SWD mode */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    /* transfer before SWD port selection is an error */
    assert_dap_command_expect("\x08\x00\x01\x02\x03\x04", "\x08\xff");
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* set tck clock frequency */
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    /* 1 turnaround cycle, no data phase on fault */
    assert_dap_command_expect("\x13\x00", "\x13\x00");

    /* 0 idle cycles, 0 wait retries, 0 match retry */
    assert_dap_command_expect("\x04\x08\x00\x00\x00\x00", "\x04\x00");

    dap_emul_start();
    dap_emul_set_tms_swdio_in("\x00\x02\x00\x00\x00\x00\x00", 7);
    assert_dap_command_expect("\x08\x00\x01\x02\x03\x04", "\x08\x00");
    assert_dap_emul_clk_cycles(54);
    assert_dap_emul_tms_swdio_out("\x81\x22\x40\x60\x80\x20\x00");

    /* JTAG instead of SWD */
    dap_emul_end();
    assert_dap_command_expect("\x03", "\x03\x00");
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* one device in chain, 5 bits long */
    assert_dap_command_expect("\x15\x01\x05", "\x15\x00");
    /* index beyond configured value is an error */
    assert_dap_command_expect("\x08\x01\x01\x02\x03\x04", "\x08\xff");
    /* JTAG read */
    dap_emul_start();
    dap_emul_set_tdo_in("\x00\x80\x00\x00\x00\x00\x00\x00", 8);
    assert_dap_command_expect("\x08\x00\x01\x02\x03\x04", "\x08\x00");
    assert_dap_emul_clk_cycles(59);

    /* incomplete command request */
    assert_dap_command_expect("\x08", "\xff");
    assert_dap_command_expect("\x08\x04\x01", "\xff");
}
