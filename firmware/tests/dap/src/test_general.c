#include <pinctrl_soc.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include "dap_emul.h"
#include "dap_transport.h"
#include "dap_io.h"
#include "util/gpio.h"

ZTEST(dap, test_info_command) {
    /* product vendor name */
    assert_dap_command_expect("\x00\x01", "\x00\x0b" "Nick Kraus\0");
    /* product description */
    assert_dap_command_expect("\x00\x02", "\x00\x17" "RICEProbe IO CMSIS-DAP\0");
    /* product serial number */
    assert_dap_command_expect("\x00\x03", "\x00\x11" "RPB1-2000123456I\0");
    /* CMSIS-DAP protocol version */
    assert_dap_command_expect("\x00\x04", "\x00\x06" "2.1.1\0");
    /* target device vendor and name, and target board vendor and name are all empty */
    assert_dap_command_expect("\x00\x05", "\x00\x00");
    assert_dap_command_expect("\x00\x06", "\x00\x00");
    assert_dap_command_expect("\x00\x07", "\x00\x00");
    assert_dap_command_expect("\x00\x08", "\x00\x00");
    /* product firmware version */
    assert_dap_command_expect("\x00\x09", "\x00\x1f" "v987.654.321-99-ba5eba11-dirty\0");
    /* CMSIS-DAP capabilities */
    assert_dap_command_expect("\x00\xf0", "\x00\x01\x57");
    /* test domain timer unsupported, uses the default unused value */
    assert_dap_command_expect("\x00\xf1", "\x00\x08\x00\x00\x00\x00");
    /* uart rx and tx buffer size */
    assert_dap_command_expect("\x00\xfb", "\x00\x04\x00\x04\x00\x00");
    assert_dap_command_expect("\x00\xfc", "\x00\x04\x00\x04\x00\x00");
    /* swo trace buffer size */
    assert_dap_command_expect("\x00\xfd", "\x00\x04\x00\x08\x00\x00");
    /* usb packet size */
    assert_dap_command_expect("\x00\xff", "\x00\x02\x00\x02");
    /* usb packet count */
    assert_dap_command_expect("\x00\xfe", "\x00\x01\x04");

    /* unsupported info id returns a length of 0 */
    assert_dap_command_expect("\x00\xbb", "\x00\x00");
    /* incomplete info command */
    assert_dap_command_expect("\x00", "\xff");
}

ZTEST(dap, test_host_status) {
    /* both LEDs start off */
    assert_gpio_emul_output_val(dap_io_led_connect, 0);
    assert_gpio_emul_output_val(dap_io_led_running, 0);

    /* connected LED remains on forever when enabled */
    assert_dap_command_expect("\x01\x00\x01", "\x01\x00");
    assert_gpio_emul_output_val(dap_io_led_connect, 1);
    assert_dap_command_expect("\x01\x00\x00", "\x01\x00");
    assert_gpio_emul_output_val(dap_io_led_connect, 0);

    /* running LED blinks at 0.5Hz when enabled */
    assert_dap_command_expect("\x01\x01\x01", "\x01\x00");
    k_sleep(K_MSEC(100));
    assert_gpio_emul_output_val(dap_io_led_running, 1);
    k_sleep(K_MSEC(500));
    assert_gpio_emul_output_val(dap_io_led_running, 0);
    k_sleep(K_MSEC(500));
    assert_gpio_emul_output_val(dap_io_led_running, 1);
    assert_dap_command_expect("\x01\x01\x00", "\x01\x00");
    assert_gpio_emul_output_val(dap_io_led_running, 0);

    /* unsupported LED type or status will report an error */
    assert_dap_command_expect("\x01\x02\x00", "\x01\xff");
    assert_dap_command_expect("\x01\x00\x02", "\x01\xff");

    /* incomplete command request */
    assert_dap_command_expect("\x01\x01", "\xff");
}

ZTEST(dap, test_delay) {
    uint32_t start, elapsed;

    /* 60,000 uS delay */
    start = k_uptime_get_32();
    assert_dap_command_expect("\x09\x60\xea", "\x09\x00");
    elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 59, 61);

    /* 30,000 uS delay */
    start = k_uptime_get_32();
    assert_dap_command_expect("\x09\x30\x75", "\x09\x00");
    elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 29, 31);

    /* incomplete command request */
    assert_dap_command_expect("\x09\x00", "\xff");
}

ZTEST(dap, test_reset_target) {
    /* currently a no-op, but make sure the command reports success */
    assert_dap_command_expect("\x0a", "\x0a\x00\x00");
}

ZTEST(dap, test_disconnect_connect) {
    /* when disconnected, all pins should not be configured as outputs */
    assert_dap_command_expect("\x03", "\x03\x00");
    assert_gpio_emul_not_flag(dap_io_tck_swclk, GPIO_OUTPUT);
    assert_gpio_emul_not_flag(dap_io_tms_swdio, GPIO_OUTPUT);
    assert_gpio_emul_not_flag(dap_io_tdo, GPIO_OUTPUT);
    assert_gpio_emul_not_flag(dap_io_tdi, GPIO_OUTPUT);
    assert_gpio_emul_not_flag(dap_io_nreset, GPIO_OUTPUT);
    /* tdo/swo (io #3) pinctrl function should be GPIO */
    assert_pinctrl_posix_func(3, POSIX_PINMUX_FUNC_GPIO);

    /* dap interface should not connect if no voltage is present on vtref */
    assert_gpio_emul_input_set(dap_io_vtref, 0);
    assert_dap_command_expect("\x02\x01", "\x02\x00");
    assert_dap_command_expect("\x02\x02", "\x02\x00");
    assert_gpio_emul_input_set(dap_io_vtref, 1);

    /* SWD mode pin configurations */
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* SWCLK is output */
    assert_gpio_emul_has_flag(dap_io_tck_swclk, GPIO_OUTPUT);
    /* SWDIO is output */
    assert_gpio_emul_has_flag(dap_io_tms_swdio, GPIO_OUTPUT);
    /* TDI is (unused) input */
    assert_gpio_emul_not_flag(dap_io_tdi, GPIO_OUTPUT);
    /* reset is output */
    assert_gpio_emul_has_flag(dap_io_nreset, GPIO_OUTPUT);
    /* tdo/swo (io #3) pinctrl function should be UART */
    assert_pinctrl_posix_func(3, POSIX_PINMUX_FUNC_UART);
    assert_dap_command_expect("\x03", "\x03\x00");

    /* JTAG mode pin configurations */
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* TCK is output */
    assert_gpio_emul_has_flag(dap_io_tck_swclk, GPIO_OUTPUT);
    /* TMS is output */
    assert_gpio_emul_has_flag(dap_io_tms_swdio, GPIO_OUTPUT);
    /* TDI is output */
    assert_gpio_emul_has_flag(dap_io_tdi, GPIO_OUTPUT);
    /* tdo/swo (io #3) pinctrl function should be GPIO */
    assert_pinctrl_posix_func(3, POSIX_PINMUX_FUNC_GPIO);
    /* TDO is input */
    assert_gpio_emul_not_flag(dap_io_tdo, GPIO_OUTPUT);
    /* reset is output */
    assert_gpio_emul_has_flag(dap_io_nreset, GPIO_OUTPUT);
    assert_dap_command_expect("\x03", "\x03\x00");

    /* some host software (probe-rs is an exapmle) will tell the probe to enter its default mode while
     * expecting it to end in SWD mode, without verifying, so ensure the default mode is SWD */
    assert_dap_command_expect("\x02\x00", "\x02\x01");

    /* unsupported ports should respond with a failure */
    assert_dap_command_expect("\x02\x03", "\x02\x00");

    /* incomplete command request */
    assert_dap_command_expect("\x02", "\xff");
}

ZTEST(dap, test_swj_pins) {
    /* start in JTAG mode, where all pins but TDO are output */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* make sure to start with all pins, input or output, low */
    assert_gpio_emul_input_set(dap_io_tdo, 0);
    assert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");
    
    /* TCK / SWCLK pin */
    assert_dap_command_expect("\x10\x01\x01\xff\xff\x00\x00", "\x10\x01");
    assert_gpio_emul_output_val(dap_io_tck_swclk, 1);
    assert_dap_command_expect("\x10\xfe\xfe\xff\xff\x00\x00", "\x10\x87");
    assert_gpio_emul_output_val(dap_io_tck_swclk, 1);
    assert_dap_command_expect("\x10\x00\x01\xff\xff\x00\x00", "\x10\x86");
    assert_gpio_emul_output_val(dap_io_tck_swclk, 0);
    assert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* TMS / SWDIO pin */
    assert_dap_command_expect("\x10\x02\x02\xff\xff\x00\x00", "\x10\x02");
    assert_gpio_emul_output_val(dap_io_tms_swdio, 1);
    assert_dap_command_expect("\x10\xfd\xfd\xff\xff\x00\x00", "\x10\x87");
    assert_gpio_emul_output_val(dap_io_tms_swdio, 1);
    assert_dap_command_expect("\x10\x00\x02\xff\xff\x00\x00", "\x10\x85");
    assert_gpio_emul_output_val(dap_io_tms_swdio, 0);
    assert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* TDI pin */
    assert_dap_command_expect("\x10\x04\x04\xff\xff\x00\x00", "\x10\x04");
    assert_gpio_emul_output_val(dap_io_tdi, 1);
    assert_dap_command_expect("\x10\xfb\xfb\xff\xff\x00\x00", "\x10\x87");
    assert_gpio_emul_output_val(dap_io_tdi, 1);
    assert_dap_command_expect("\x10\x00\x04\xff\xff\x00\x00", "\x10\x83");
    assert_gpio_emul_output_val(dap_io_tdi, 0);
    assert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* nRESET pin */
    assert_dap_command_expect("\x10\x80\x80\xff\xff\x00\x00", "\x10\x80");
    assert_gpio_emul_output_val(dap_io_nreset, 1);
    assert_dap_command_expect("\x10\x7f\x7f\xff\xff\x00\x00", "\x10\x87");
    assert_gpio_emul_output_val(dap_io_nreset, 1);
    assert_dap_command_expect("\x10\x00\x80\xff\xff\x00\x00", "\x10\x07");
    assert_gpio_emul_output_val(dap_io_nreset, 0);
    assert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* TDO pin */
    assert_gpio_emul_input_set(dap_io_tdo, 1);
    assert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x08");

    /* incomplete command request */
    assert_dap_command_expect("\x10", "\xff");
    assert_dap_command_expect("\x10\x00", "\xff");
    assert_dap_command_expect("\x10\x00\xff\xff", "\xff");
}

ZTEST(dap, test_swj_clock_sequence) {
    /* need to be configured as either swd or jtag for tck/swclk output */
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    /* clock rate of 0 not allowed */
    assert_dap_command_expect("\x11\x00\x00\x00\x00", "\x11\xff");
    /* anything else should be okay */
    assert_dap_command_expect("\x11\xff\xff\xff\xff", "\x11\x00");

    /* make sure tck/swclk actually switches at 100 KHz */
    dap_emul_start();
    assert_dap_command_expect("\x11\xa0\x86\x01\x00", "\x11\x00");
    assert_dap_command_expect("\x12\x10\xab\xcd", "\x12\x00");
    assert_dap_emul_clk_cycles(16);
    assert_dap_emul_clk_period(10000);
    /* check tms/swdio data */
    assert_dap_emul_tms_swdio_out("\xab\xcd");

    /* check same thing for 20 KHz */
    dap_emul_reset();
    assert_dap_command_expect("\x11\x20\x4e\x00\x00", "\x11\x00");
    assert_dap_command_expect("\x12\x20\x13\x57\x9b\xdf", "\x12\x00");
    assert_dap_emul_clk_cycles(32);
    assert_dap_emul_clk_period(50000);
    /* check tms/swdio data */
    assert_dap_emul_tms_swdio_out("\x13\x57");

    /* sequence length of 256 encoded as '0' */
    dap_emul_reset();
    assert_dap_command_expect(
        "\x12\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
        "\x12\x00"
    );
    assert_dap_emul_clk_cycles(256);
    
    /* incomplete command request */
    assert_dap_command_expect("\x11\x00\x00", "\xff");
    assert_dap_command_expect("\x12", "\xff");
    assert_dap_command_expect("\x12\x10", "\xff");
}

ZTEST(dap, test_atomic) {
    /* two copies of vendor info */
    assert_dap_command_expect(
        "\x7f\x02\x00\x01\x00\x01",
        "\x7f\x02\x00\x0b" "Nick Kraus\0" "\x00\x0b" "Nick Kraus\0"
    );

    /* same as above, but with a 60,000 uS delay, ensure it is taken */
    uint32_t start = k_uptime_get_32();
    assert_dap_command_expect(
        "\x7f\x03\x00\x01\x09\x60\xea\x00\x01",
        "\x7f\x03\x00\x0b" "Nick Kraus\0" "\x09\x00\x00\x0b" "Nick Kraus\0"
    );
    uint32_t elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 59, 61);

    /* queued commands will have no responses until the next non-queued command */
    assert_dap_command_expect("\x7e\x01\x09\xff\x00", "");
    assert_dap_command_expect("\x7e\x02\x00\x01\x09\xff\x00", "");
    /* now all three responses should come */
    assert_dap_command_expect(
        "\x00\x02",
        "\x7f\x01\x09\x00\x7f\x02\x00\x0b" "Nick Kraus\0" "\x09\x00\x00\x17" "RICEProbe IO CMSIS-DAP\0"
    );

    /* incomplete command request */
    assert_dap_command_expect("\x7f", "\xff");
}
