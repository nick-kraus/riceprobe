#include <pinctrl_soc.h>
#include <zephyr/drivers/serial/uart_emul.h>
#include <zephyr/ztest.h>

#include "dap_io.h"
#include "dap_emul.h"
#include "dap_transport.h"
#include "util/gpio.h"

ZTEST(dap, test_swo) {
    assert_gpio_emul_input_set(dap_io_vtref, 1);
    /* enabling capture control should fail in jtag mode */
    assert_dap_command_expect("\x02\x02", "\x02\x02");
    assert_dap_command_expect("\x1a\x01", "\x1a\xff");
    /* now go to swd mode, swo should work */
    assert_dap_command_expect("\x03", "\x03\x00");
    assert_dap_command_expect("\x02\x01", "\x02\x01");
    /* tdo/swo (io #3) pinctrl function should be UART */
    assert_pinctrl_sim_func(3, SIM_PINMUX_FUNC_UART);

    /* usb bulk endpoint is not a supported swo transport */
    assert_dap_command_expect("\x17\x02", "\x17\xff");
    /* reserved values are not valid swo transports */
    assert_dap_command_expect("\x17\x03", "\x17\xff");
    /* dap swo data command is a supported transport */
    assert_dap_command_expect("\x17\x01", "\x17\x00");
    /* none is also supported */
    assert_dap_command_expect("\x17\x00", "\x17\x00");
    assert_dap_command_expect("\x17\x01", "\x17\x00");

    /* manchester mode is not supported */
    assert_dap_command_expect("\x18\x02", "\x18\xff");
    /* reserved values are not supported */
    assert_dap_command_expect("\x18\x03", "\x18\xff");
    /* uart mode is supported */
    assert_dap_command_expect("\x18\x01", "\x18\x00");
    /* off is also supported */
    assert_dap_command_expect("\x18\x00", "\x18\x00");
    assert_dap_command_expect("\x18\x01", "\x18\x00");

    /* before enabling capture control, status should show as disabled, with no bytes in buffer */
    assert_dap_command_expect("\x1b", "\x1b\x00\x00\x00\x00\x00");
    /* set common baudrate value and enable capture control */
    assert_dap_command_expect("\x19\x00\xc2\x01\x00", "\x19\x00\xc2\x01\x00");
    assert_dap_command_expect("\x1a\x01", "\x1a\x00");
    /* status should now show as enabled */
    assert_dap_command_expect("\x1b", "\x1b\x01\x00\x00\x00\x00");

    /* re-configuring the mode should disable capture */
    assert_dap_command_expect("\x18\x01", "\x18\x00");
    assert_dap_command_expect("\x1b", "\x1b\x00\x00\x00\x00\x00");
    assert_dap_command_expect("\x1a\x01", "\x1a\x00");
    assert_dap_command_expect("\x1b", "\x1b\x01\x00\x00\x00\x00");

    /* write data to the swo uart and make sure we can read it back */
    uart_emul_put_rx_data(dap_swo_uart, "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
    assert_dap_command_expect("\x1b", "\x1b\x01\x08\x00\x00\x00");
    /* try with extended status, which currently isn't much different than regular status */
    assert_dap_command_expect("\x1e\x03", "\x1e\x01\x08\x00\x00\x00");
    /* only grabbing the first 4 bytes */
    assert_dap_command_expect("\x1c\x04\x00", "\x1c\x01\x04\x00\x01\x02\x03\x04");
    /* try to grab 8 bytes, but we should only get back the full remaining 4 */
    assert_dap_command_expect("\x1c\x08\x00", "\x1c\x01\x04\x00\x05\x06\x07\x08");

    /* swo trace buffer size should be 2048 bytes */
    assert_dap_command_expect("\x00\xfd", "\x00\x04\x00\x08\x00\x00");
    /* the swo buffer is currently empty, so we should be able to write the full 2048 bytes */
    for (uint16_t i = 0; i < 2048; i++) { uart_emul_put_rx_data(dap_swo_uart, "\x55", 1); }
    assert_dap_command_expect("\x1b", "\x1b\x01\x00\x08\x00\x00");
    /* adding more bytes now shouldn't increase the buffer size, since it is full */
    uart_emul_put_rx_data(dap_swo_uart, "\x55\x55\x55\x55", 4);
    /* this will also set the overrun status bit */
    assert_dap_command_expect("\x1b", "\x1b\x81\x00\x08\x00\x00");
    /* see if we can grab all that data */
    for (uint16_t i = 0; i < (2048 / 16); i++) {
        assert_dap_command_expect(
            "\x1c\x10\x00",
            "\x1c\x81\x10\x00" "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55"
        );
    }
    /* should be down to 0 bytes now */
    assert_dap_command_expect("\x1b", "\x1b\x81\x00\x00\x00\x00");
    /* and turning control on and off should clear the overrun status */
    assert_dap_command_expect("\x1a\x00", "\x1a\x00");
    assert_dap_command_expect("\x1a\x01", "\x1a\x00");
    assert_dap_command_expect("\x1b", "\x1b\x01\x00\x00\x00\x00");

    /* turning control on and off should also reset the buffer contents */
    uart_emul_put_rx_data(dap_swo_uart, "\x01\x02\x03\x04\x05\x06\x07\x08", 8);
    assert_dap_command_expect("\x1b", "\x1b\x01\x08\x00\x00\x00");
    assert_dap_command_expect("\x1a\x00", "\x1a\x00");
    assert_dap_command_expect("\x1a\x01", "\x1a\x00");
    assert_dap_command_expect("\x1b", "\x1b\x01\x00\x00\x00\x00");

    /* incomplete command requests */
    assert_dap_command_expect("\x17", "\xff");
    assert_dap_command_expect("\x18", "\xff");
    assert_dap_command_expect("\x19\x00\xc2", "\xff");
    assert_dap_command_expect("\x1a", "\xff");
    assert_dap_command_expect("\x1e", "\xff");
    assert_dap_command_expect("\x1c\x00", "\xff");
}
