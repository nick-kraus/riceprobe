#include <zephyr/storage/flash_map.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "dap_io.h"
#include "dap_target_emul.h"
#include "dap_transport.h"
#include "util/gpio.h"

static uint8_t nvs_data[] = {
    /* manufacturing tag (0x7A5A, little-endian) */
    0x5A, 0x7A,
    /* manufacturing data version (0x0101, little-endian) */
    0x01, 0x01,
    /* serial number */
    'R', 'P', 'B', '1', '-', '2', '0', '0',
    '0', '1', '2', '3', '4', '5', '6', 'I',
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 
    /* uuid */
    0x20, 0xA0, 0x2F, 0xC2, 0xA7, 0x91, 0x41, 0x4B,
    0xBB, 0x66, 0xC6, 0x40, 0x7E, 0x78, 0xA7, 0x10,
};

int32_t nvs_init(void);
int32_t dap_init(void);

static void *dap_tests_setup(void) {
    const struct flash_area *fa;
    zassert_ok(flash_area_open(FIXED_PARTITION_ID(manufacturing_partition), &fa));
    zassert_ok(flash_area_erase(fa, 0, sizeof(nvs_data)));
    zassert_ok(flash_area_write(fa, 0, nvs_data, sizeof(nvs_data)));
    flash_area_close(fa);

    dap_target_emul_init();

    zassert_ok(nvs_init());
    zassert_ok(dap_init());

    /* make sure the test transport has been selected by the time the test runs */
    k_sleep(K_MSEC(100));

    return NULL;
}

static void dap_tests_before(void *fixture) {
    /* no mode (swd / jtag) configured */
    assert_dap_command_expect("\x03", "\x03\x00");
    /* target reference voltage low */
    assert_gpio_emul_input_set(dap_io_vtref, 0);
    /* target emulator disabled */
    dap_target_emul_end();
}

ZTEST_SUITE(dap, NULL, dap_tests_setup, dap_tests_before, NULL, NULL);
