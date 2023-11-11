#include <zephyr/drivers/gpio.h>
#include <zephyr/ztest.h>

#include "io/io.h"
#include "io_ios.h"
#include "io_transport.h"
#include "util/gpio.h"

ZTEST(io, test_gpio_caps_cfg) {
    /* default pinctrl state */
    assert_io_command_expect("\x0a", "\x0a\x00");

    /* supported capabilities and flags, for posix boards open drain not supported */
    assert_io_command_expect("\x10",  "\x10\x00\x01\x03\x01\x0f");

    /* config command only supports a single flag byte */
    assert_io_command_expect("\x11\x00\x02\x00\x00",  "\x11\xff");
    /* unsupported GPIO produces an error */
    assert_io_command_expect("\x11\xff\x01\x00",  "\x11\xff");

    /* input only */
    assert_io_command_expect("\x11\x00\x01\x00",  "\x11\x00");
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_INPUT);
    assert_gpio_emul_not_flag(&io_gpios[0], GPIO_OUTPUT);
    /* output, but also input */
    assert_io_command_expect("\x11\x00\x01\x01",  "\x11\x00");
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_INPUT);
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_OUTPUT);

    /* pull up */
    assert_io_command_expect("\x11\x00\x01\x04",  "\x11\x00");
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_PULL_UP);
    /* pull down */
    assert_io_command_expect("\x11\x00\x01\x08",  "\x11\x00");
    assert_gpio_emul_has_flag(&io_gpios[0], GPIO_PULL_DOWN);
    /* pull up and down together isn't supported */
    assert_io_command_expect("\x11\x00\x01\x0c",  "\x11\xff");
    /* open drain and non-output also not supported */
    assert_io_command_expect("\x11\x00\x01\x10",  "\x11\xff");

    /* open drain not available on posix boards, will result in an error */
    assert_io_command_expect("\x11\x00\x01\x11",  "\x11\xfe");

    /* incomplete command requests */
    assert_io_command_expect("\x11",  "\xff");
    assert_io_command_expect("\x11\x00",  "\xff");
    assert_io_command_expect("\x11\x00\x01",  "\xff");
}

/* sets gpio0 to '1' if it is an input */
void gpio0_set_timer_handler(struct k_timer *timer) {
    assert_gpio_emul_not_flag(&io_gpios[0], GPIO_OUTPUT);
    assert_gpio_emul_input_set(&io_gpios[0], 1);
    k_timer_stop(timer);
}
K_TIMER_DEFINE(gpio0_set_timer, gpio0_set_timer_handler, NULL);

ZTEST(io, test_gpio_ctrl) {
    /* default pinctrl state */
    assert_io_command_expect("\x0a", "\x0a\x00");

    /* gpio0 output, active low */
    assert_io_command_expect("\x11\x00\x01\x03",  "\x11\x00");
    assert_io_command_expect("\x12\x00\x03", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 0);
    assert_io_command_expect("\x12\x00\x02", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 1);

    /* gpio0 output, active high */
    assert_io_command_expect("\x11\x00\x01\x01",  "\x11\x00");
    assert_io_command_expect("\x12\x00\x03", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 1);
    assert_io_command_expect("\x12\x00\x02", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 0);

    /* gpio toggle */
    assert_io_command_expect("\x12\x00\x04", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 1);
    assert_io_command_expect("\x12\x00\x04", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 0);

    /* gpio0 input, active low */
    assert_io_command_expect("\x11\x00\x01\x02",  "\x11\x00");
    assert_gpio_emul_input_set(&io_gpios[0], 1);
    assert_io_command_expect("\x12\x00\x08", "\x12\x00\x00");
    assert_gpio_emul_input_set(&io_gpios[0], 0);
    assert_io_command_expect("\x12\x00\x08", "\x12\x00\x01");

    /* gpio0 input, active high */
    assert_io_command_expect("\x11\x00\x01\x00",  "\x11\x00");
    assert_gpio_emul_input_set(&io_gpios[0], 1);
    assert_io_command_expect("\x12\x00\x08", "\x12\x00\x01");
    assert_gpio_emul_input_set(&io_gpios[0], 0);
    assert_io_command_expect("\x12\x00\x08", "\x12\x00\x00");

    /* waiting for an already set input should be instant */
    uint32_t start = k_uptime_get_32();
    assert_gpio_emul_input_set(&io_gpios[0], 1);
    assert_io_command_expect("\x12\x00\x10\xff\x00\x00\x00", "\x12\x00");
    zassert_equal((k_uptime_get_32() - start), 0);
    /* waiting should eventually timeout */
    start = k_uptime_get_32();
    assert_io_command_expect("\x12\x00\x10\xfe\x00\x00\x00", "\x12\xfd");
    zassert_equal((k_uptime_get_32() - start), 127);
    /* should take the specified amount of time in the timer to return */
    assert_gpio_emul_input_set(&io_gpios[0], 0);
    start = k_uptime_get_32();
    k_timer_start(&gpio0_set_timer, K_MSEC(750), K_MSEC(750));
    assert_io_command_expect("\x12\x00\x10\xff\xff\xff\xff", "\x12\x00");
    zassert_between_inclusive((k_uptime_get_32() - start), 749, 751);

    /* writing happens before toggling */
    assert_io_command_expect("\x11\x00\x01\x01",  "\x11\x00");
    assert_io_command_expect("\x12\x00\x07", "\x12\x00");
    assert_gpio_emul_output_val(&io_gpios[0], 0);

    /* toggling happens before reading */
    assert_gpio_emul_output_val(&io_gpios[0], 0);
    assert_io_command_expect("\x12\x00\x0c", "\x12\x00\x01");
    assert_gpio_emul_output_val(&io_gpios[0], 1);

    /* reading happens before waiting */
    assert_io_command_expect("\x11\x00\x01\x00",  "\x11\x00");
    assert_gpio_emul_input_set(&io_gpios[0], 0);
    k_timer_start(&gpio0_set_timer, K_MSEC(750), K_MSEC(750));
    assert_io_command_expect("\x12\x00\x18\xff\xff\xff\xff", "\x12\x00\x00");

    /* incomplete command requests */
    assert_io_command_expect("\x12",  "\xff");
    assert_io_command_expect("\x12\x00",  "\xff");
    assert_io_command_expect("\x12\x00\x10",  "\xff");
}
