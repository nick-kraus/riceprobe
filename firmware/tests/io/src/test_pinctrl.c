#include <zephyr/drivers/pinctrl.h>
#include <zephyr/ztest.h>

#include "io/io.h"
#include "io_ios.h"
#include "io_transport.h"
#include "util/gpio.h"

ZTEST(io, test_pins_capabilities_command) {
    /* io0, gpio0, uart0 */
    assert_io_command_expect("\x09\x00", "\x09\x00\x01\x03\x02\x00\x00\x01\x00");
    /* io1, gpio1, uart0, i2c0 */
    assert_io_command_expect("\x09\x01", "\x09\x00\x01\x03\x03\x00\x01\x01\x00\x02\x00");
    /* io2, gpio2, i2c0 */
    assert_io_command_expect("\x09\x02", "\x09\x00\x01\x03\x02\x00\x02\x02\x00");

    /* unsupported io */
    assert_io_command_expect("\x09\xff", "\x09\xff");

    /* incomplete command requests */
    assert_io_command_expect("\x09", "\xff");
}

ZTEST(io, test_pins_cfg_default_commands) {
    /* io0, gpio0 */
    assert_io_command_expect("\x0b\x00\x00\x00\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(8, SIM_PINMUX_FUNC_GPIO);
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[0], GPIO_OUTPUT);
    /* io0, uart0 */
    assert_io_command_expect("\x0b\x00\x01\x00\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(8, SIM_PINMUX_FUNC_UART);
    
    /* io1, gpio1 */
    assert_io_command_expect("\x0b\x01\x00\x01\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(9, SIM_PINMUX_FUNC_GPIO);
    assert_gpio_emul_has_flag(&io_gpios[1], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[1], GPIO_OUTPUT);
    /* io1, uart0 */
    assert_io_command_expect("\x0b\x01\x01\x00\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(9, SIM_PINMUX_FUNC_UART);
    /* io1, i2c0 */
    assert_io_command_expect("\x0b\x01\x02\x00\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(9, SIM_PINMUX_FUNC_I2C);
    
    /* io2, gpio2 */
    assert_io_command_expect("\x0b\x02\x00\x02\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(10, SIM_PINMUX_FUNC_GPIO);
    assert_gpio_emul_has_flag(&io_gpios[2], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[2], GPIO_OUTPUT);
    /* io2, i2c0 */
    assert_io_command_expect("\x0b\x02\x02\x00\x01\x00", "\x0b\x00");
    assert_pinctrl_sim_func(10, SIM_PINMUX_FUNC_I2C);

    /* unsupported pin */
    assert_io_command_expect("\x0b\xff\x00\xff\x01\x00", "\x0b\xff");
    /* unsupported function */
    assert_io_command_expect("\x0b\x00\xff\x00\x01\x00", "\x0b\xff");
    /* unsupported index */
    assert_io_command_expect("\x0b\x00\x00\x01\x01\x00", "\x0b\xff");

    /* default state is gpio input */
    assert_io_command_expect("\x0a", "\x0a\x00");
    assert_pinctrl_sim_func(8, SIM_PINMUX_FUNC_GPIO);
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[0], GPIO_OUTPUT);
    assert_pinctrl_sim_func(9, SIM_PINMUX_FUNC_GPIO);
    assert_gpio_emul_has_flag(&io_gpios[1], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[1], GPIO_OUTPUT);
    assert_pinctrl_sim_func(10, SIM_PINMUX_FUNC_GPIO);
    assert_gpio_emul_has_flag(&io_gpios[2], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[2], GPIO_OUTPUT);

    /* only support one flag byte */
    assert_io_command_expect("\x0b\x00\x00\x00\x02\x00\x00", "\x0b\xff");

    /* test various pinctrl flags */
    assert_io_command_expect("\x0b\x00\x00\x00\x01\x01", "\x0b\x00");
    assert_pinctrl_sim_func(8, SIM_PINMUX_FUNC_GPIO);
    assert_pinctrl_sim_has_flags(8, SIM_PINFLAG_PULLUP);
    assert_gpio_emul_has_flag(&io_gpios[0], (GPIO_INPUT | GPIO_PULL_UP));
    assert_io_command_expect("\x0b\x00\x00\x00\x01\x02", "\x0b\x00");
    assert_pinctrl_sim_func(8, SIM_PINMUX_FUNC_GPIO);
    assert_pinctrl_sim_has_flags(8, SIM_PINFLAG_PULLDOWN);
    assert_gpio_emul_has_flag(&io_gpios[0], (GPIO_INPUT | GPIO_PULL_DOWN));
    /* posix boards don't support open drain IO, not tested here */

    /* incomplete command requests */
    assert_io_command_expect("\x0b", "\xff");
    assert_io_command_expect("\x0b\x10", "\xff");
    assert_io_command_expect("\x0b\x10\x00", "\xff");
    assert_io_command_expect("\x0b\x10\x00\x00", "\xff");
    assert_io_command_expect("\x0b\x10\x00\x00\x01", "\xff");
}
