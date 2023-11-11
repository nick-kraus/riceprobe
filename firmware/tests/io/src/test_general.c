#include <zephyr/ztest.h>

#include "io_transport.h"

ZTEST(io, test_info_command) {
    /* transport packet size */
    assert_io_command_expect("\x01\x01", "\x01\x00\x02\x00\x02");
    /* transport buffer size */
    assert_io_command_expect("\x01\x02", "\x01\x00\x02\x00\x08");
    /* product vendor name */
    assert_io_command_expect("\x01\x03", "\x01\x00\x0b" "Nick Kraus\0");
    /* product description */
    assert_io_command_expect("\x01\x04", "\x01\x00\x17" "RICEProbe IO CMSIS-DAP\0");
    /* serial number */
    assert_io_command_expect("\x01\x05", "\x01\x00\x11" "RPB1-2000123456I\0");
    /* uuid */
    assert_io_command_expect("\x01\x06", "\x01\x00\x25" "20a02fc2-a791-414b-bb66-c6407e78a710\0");
    /* firmware version */
    assert_io_command_expect("\x01\x07", "\x01\x00\x1f" "v987.654.321-99-ba5eba11-dirty\0");
    /* protocol version */
    assert_io_command_expect("\x01\x08", "\x01\x00\x06" "0.1.0\0");
    /* supported subsystems */
    assert_io_command_expect("\x01\x09", "\x01\x00\x01\x00");
    /* number of available pins */
    assert_io_command_expect("\x01\x0a", "\x01\x00\x02\x0a\x00");

    /* don't currently support anything */
    assert_io_command_expect("\x01\x09", "\x01\x00\x01\x00");

    /* unsupported id returns enotsup status */
    assert_io_command_expect("\x01\x00", "\x01\xff");
    /* incomplete command requests */
    assert_io_command_expect("\x01", "\xff");
}

ZTEST(io, test_multi_queue_delay_command) {
    /* two copies of packet size */
    assert_io_command_expect(
        "\x02\x02" "\x01\x01" "\x01\x01",
        "\x02\x02" "\x01\x00\x02\x00\x02" "\x01\x00\x02\x00\x02"
    );

    /* same as above, but with a delay, ensure it is taken */
    uint32_t start = k_uptime_get_32();
    assert_io_command_expect(
        "\x02\x03" "\x01\x01" "\x04\x10\x27\x00\x00" "\x01\x01",
        "\x02\x03" "\x01\x00\x02\x00\x02" "\x04\x00" "\x01\x00\x02\x00\x02"
    );
    uint32_t elapsed = k_uptime_get_32() - start;
    zassert_between_inclusive(elapsed, 9, 11);

    /* queued commands will have no response until the next non-queued command */
    assert_io_command_expect("\x03\x02" "\x01\x01" "\x04\x01\x00\x00\x00", "");
    assert_io_command_expect("\x03\x01" "\x01\x02", "");
    /* now all three responses should come */
    assert_io_command_expect(
        "\x01\x09",
        "\x02\x02" "\x01\x00\x02\x00\x02" "\x04\x00" "\x02\x01" "\x01\x00\x02\x00\x08" "\x01\x00\x01\x00"
    );

    /* incomplete command requests */
    assert_io_command_expect("\x03", "\xff");
    assert_io_command_expect("\x02", "\xff");
}

ZTEST(io, test_unsupported_command) {
    assert_io_command_expect("\xf4\x8f\xa0\x80", "\xff");
}
