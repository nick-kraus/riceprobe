#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#include "main.h"
#include "tests/util.h"

ZTEST(dap, test_info_command) {
    /* product vendor name */
    zassert_dap_command_expect("\x00\x01", "\x00\x0b" "Nick Kraus\0");
    /* product description */
    zassert_dap_command_expect("\x00\x02", "\x00\x17" "RICEProbe IO CMSIS-DAP\0");
    /* product serial number */
    zassert_dap_command_expect("\x00\x03", "\x00\x11" "RPB1-2000123456I\0");
    /* CMSIS-DAP protocol version */
    zassert_dap_command_expect("\x00\x04", "\x00\x06" "2.1.1\0");
    /* target device vendor and name, and target board vendor and name are all empty */
    zassert_dap_command_expect("\x00\x05", "\x00\x00");
    zassert_dap_command_expect("\x00\x06", "\x00\x00");
    zassert_dap_command_expect("\x00\x07", "\x00\x00");
    zassert_dap_command_expect("\x00\x08", "\x00\x00");
    /* product firmware version */
    zassert_dap_command_expect("\x00\x09", "\x00\x1f" "v987.654.321-99-ba5eba11-dirty\0");
    /* CMSIS-DAP capabilities */
    zassert_dap_command_expect("\x00\xf0", "\x00\x01\x13");
    /* test domain timer unsupported, uses the default unused value */
    zassert_dap_command_expect("\x00\xf1", "\x00\x08\x00\x00\x00\x00");
    /* uart rx and tx buffer size */
    zassert_dap_command_expect("\x00\xfb", "\x00\x04\x00\x04\x00\x00");
    zassert_dap_command_expect("\x00\xfc", "\x00\x04\x00\x04\x00\x00");
    /* swo trace buffer size */
    zassert_dap_command_expect("\x00\xfd", "\x00\x04\x00\x08\x00\x00");
    /* usb packet size */
    zassert_dap_command_expect("\x00\xff", "\x00\x02\x00\x02");
    /* usb packet count */
    zassert_dap_command_expect("\x00\xfe", "\x00\x01\x04");

    /* unsupported info id returns a length of 0 */
    zassert_dap_command_expect("\x00\xbb", "\x00\x00");
    /* incomplete info command */
    zassert_dap_command_expect("\x00", "\xff");
}

ZTEST_F(dap, test_host_status) {
    /* both LEDs start off */
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_connect), 0);
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_running), 0);

    /* connected LED remains on forever when enabled */
    zassert_dap_command_expect("\x01\x00\x01", "\x01\x00");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_connect), 1);
    zassert_dap_command_expect("\x01\x00\x00", "\x01\x00");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_connect), 0);

    /* running LED blinks at 0.5Hz when enabled */
    zassert_dap_command_expect("\x01\x01\x01", "\x01\x00");
    k_sleep(K_MSEC(100));
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_running), 1);
    k_sleep(K_MSEC(500));
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_running), 0);
    k_sleep(K_MSEC(500));
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_running), 1);
    zassert_dap_command_expect("\x01\x01\x00", "\x01\x00");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.led_running), 0);

    /* unsupported LED type or status will report an error */
    zassert_dap_command_expect("\x01\x02\x00", "\x01\xff");
    zassert_dap_command_expect("\x01\x00\x02", "\x01\xff");

    /* incomplete command request */
    zassert_dap_command_expect("\x01\x01", "\xff");
}

ZTEST(dap, test_delay) {
    uint32_t start, elapsed;

    /* 60,000 uS delay */
    start = k_uptime_get_32();
    zassert_dap_command_expect("\x09\x60\xea", "\x09\x00");
    elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 59, 61);

    /* 30,000 uS delay */
    start = k_uptime_get_32();
    zassert_dap_command_expect("\x09\x30\x75", "\x09\x00");
    elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 29, 31);

    /* incomplete command request */
    zassert_dap_command_expect("\x09\x00", "\xff");
}

ZTEST(dap, test_reset_target) {
    /* currently a no-op, but make sure the command reports success */
    zassert_dap_command_expect("\x0a", "\x0a\x00\x00");
}

ZTEST_F(dap, test_disconnect_connect) {
    gpio_flags_t flags;

    /* when disconnected, all pins should not be configured as outputs */
    zassert_dap_command_expect("\x03", "\x03\x00");
    gpio_emul_flags_get_dt(&fixture->io.tck_swclk, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    gpio_emul_flags_get_dt(&fixture->io.tms_swdio, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    gpio_emul_flags_get_dt(&fixture->io.tdo, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    gpio_emul_flags_get_dt(&fixture->io.tdi, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    gpio_emul_flags_get_dt(&fixture->io.nreset, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);

    /* dap interface should not connect if no voltage is present on vtref */
    gpio_emul_input_set_dt(&fixture->io.vtref, 0);
    zassert_dap_command_expect("\x02\x01", "\x02\x00");
    zassert_dap_command_expect("\x02\x02", "\x02\x00");
    gpio_emul_input_set_dt(&fixture->io.vtref, 1);

    /* SWD mode pin configurations */
    zassert_dap_command_expect("\x02\x01", "\x02\x01");
    /* SWCLK is output */
    gpio_emul_flags_get_dt(&fixture->io.tck_swclk, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* SWDIO is output */
    gpio_emul_flags_get_dt(&fixture->io.tms_swdio, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* TDI is (unused) input */
    gpio_emul_flags_get_dt(&fixture->io.tdi, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* TODO: TDO isn't configured as a GPIO anymore, we should use a mock pinctrl to test this */
    /* reset is output */
    gpio_emul_flags_get_dt(&fixture->io.nreset, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    zassert_dap_command_expect("\x03", "\x03\x00");

    /* JTAG mode pin configurations */
    zassert_dap_command_expect("\x02\x02", "\x02\x02");
    /* TCK is output */
    gpio_emul_flags_get_dt(&fixture->io.tck_swclk, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* TMS is output */
    gpio_emul_flags_get_dt(&fixture->io.tms_swdio, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* TDI is output */
    gpio_emul_flags_get_dt(&fixture->io.tdi, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* TDO is input */
    gpio_emul_flags_get_dt(&fixture->io.tdo, &flags);
    zassert_not_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    /* reset is output */
    gpio_emul_flags_get_dt(&fixture->io.nreset, &flags);
    zassert_equal(flags & GPIO_OUTPUT, GPIO_OUTPUT);
    zassert_dap_command_expect("\x03", "\x03\x00");

    /* ensure the 'default' mode is JTAG */
    zassert_dap_command_expect("\x02\x00", "\x02\x02");
    zassert_dap_command_expect("\x03", "\x03\x00");

    /* incomplete command request */
    zassert_dap_command_expect("\x02", "\xff");
}

ZTEST_F(dap, test_swj_pins) {
    /* start in JTAG mode, where all pins but TDO are output */
    zassert_dap_command_expect("\x02\x02", "\x02\x02");
    /* make sure to start with all pins, input or output, low */
    gpio_emul_input_set_dt(&fixture->io.tdo, 0);
    zassert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");
    
    /* TCK / SWCLK pin */
    zassert_dap_command_expect("\x10\x01\x01\xff\xff\x00\x00", "\x10\x01");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tck_swclk), 1);
    zassert_dap_command_expect("\x10\xfe\xfe\xff\xff\x00\x00", "\x10\x87");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tck_swclk), 1);
    zassert_dap_command_expect("\x10\x00\x01\xff\xff\x00\x00", "\x10\x86");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tck_swclk), 0);
    zassert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* TMS / SWDIO pin */
    zassert_dap_command_expect("\x10\x02\x02\xff\xff\x00\x00", "\x10\x02");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tms_swdio), 1);
    zassert_dap_command_expect("\x10\xfd\xfd\xff\xff\x00\x00", "\x10\x87");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tms_swdio), 1);
    zassert_dap_command_expect("\x10\x00\x02\xff\xff\x00\x00", "\x10\x85");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tms_swdio), 0);
    zassert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* TDI pin */
    zassert_dap_command_expect("\x10\x04\x04\xff\xff\x00\x00", "\x10\x04");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tdi), 1);
    zassert_dap_command_expect("\x10\xfb\xfb\xff\xff\x00\x00", "\x10\x87");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tdi), 1);
    zassert_dap_command_expect("\x10\x00\x04\xff\xff\x00\x00", "\x10\x83");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.tdi), 0);
    zassert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* nRESET pin */
    zassert_dap_command_expect("\x10\x80\x80\xff\xff\x00\x00", "\x10\x80");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.nreset), 1);
    zassert_dap_command_expect("\x10\x7f\x7f\xff\xff\x00\x00", "\x10\x87");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.nreset), 1);
    zassert_dap_command_expect("\x10\x00\x80\xff\xff\x00\x00", "\x10\x07");
    zassert_equal(gpio_emul_output_get_dt(&fixture->io.nreset), 0);
    zassert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x00");

    /* TDO pin */
    gpio_emul_input_set_dt(&fixture->io.tdo, 1);
    zassert_dap_command_expect("\x10\x00\xff\xff\xff\x00\x00", "\x10\x08");

    /* incomplete command request */
    zassert_dap_command_expect("\x10", "\xff");
    zassert_dap_command_expect("\x10\x00", "\xff");
    zassert_dap_command_expect("\x10\x00\xff\xff", "\xff");
}

ZTEST_F(dap, test_swj_clock_sequence) {
    /* TODO: implement this test */
}

ZTEST(dap, test_atomic) {
    /* two copies of vendor info */
    zassert_dap_command_expect(
        "\x7f\x02\x00\x01\x00\x01",
        "\x7f\x02\x00\x0b" "Nick Kraus\0" "\x00\x0b" "Nick Kraus\0"
    );

    /* same as above, but with a 60,000 uS delay, ensure it is taken */
    uint32_t start = k_uptime_get_32();
    zassert_dap_command_expect(
        "\x7f\x03\x00\x01\x09\x60\xea\x00\x01",
        "\x7f\x03\x00\x0b" "Nick Kraus\0" "\x09\x00\x00\x0b" "Nick Kraus\0"
    );
    uint32_t elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 59, 61);

    /* queued commands will have no responses until the next non-queued command */
    zassert_dap_command_expect("\x7e\x01\x09\xff\x00", "");
    zassert_dap_command_expect("\x7e\x02\x00\x01\x09\xff\x00", "");
    /* now all three responses should come */
    zassert_dap_command_expect(
        "\x00\x02",
        "\x7f\x01\x09\x00\x7f\x02\x00\x0b" "Nick Kraus\0" "\x09\x00\x00\x17" "RICEProbe IO CMSIS-DAP\0"
    );

    /* incomplete command request */
    zassert_dap_command_expect("\x7f", "\xff");
}
