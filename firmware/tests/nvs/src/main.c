#include <zephyr/storage/flash_map.h>
#include <zephyr/ztest.h>

#include "nvs.h"

static const struct flash_area *fa;

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

ZTEST(nvs, test_nvs_init) {
    /* flash contents are good, should be able to initialize without issue */
    zassert_ok(nvs_init());

    /* invalid tag should produce an -EINVAL error */
    zassert_ok(flash_area_erase(fa, 0, 1));
    zassert_ok(flash_area_write(fa, 0, (uint8_t[]){0xFF}, 1));
    zassert_equal(nvs_init(), -EINVAL);

    /* invalid version should produce an -ENOTSUP error */
    zassert_ok(flash_area_erase(fa, 0, 3));
    zassert_ok(flash_area_write(fa, 0, (uint8_t[]){0x5A, 0x7A, 0xFF}, 3));
    zassert_equal(nvs_init(), -ENOTSUP);

    /* serial numbers which leave no room for a null terminator are also errors */
    zassert_ok(flash_area_erase(fa, 0, 36));
    zassert_ok(flash_area_write(fa, 0, (uint8_t[]){0x5A, 0x7A, 0x01, 0x01}, 4));
    for (int i = 0; i < 32; i++) {
        zassert_ok(flash_area_write(fa, 4 + i, (uint8_t[]){'Z'}, 1));
    }
    zassert_equal(nvs_init(), -EINVAL);
}

ZTEST(nvs, test_nvs_serial) {
    zassert_ok(nvs_init());

    /* too small buffer causes an error */
    uint8_t small_buf[8] = {0};
    zassert_equal(nvs_get_serial_number(small_buf, sizeof(small_buf)), -ENOBUFS);

    uint8_t buf[17] = {0};
    zassert_ok(nvs_get_serial_number(buf, sizeof(buf)));
    zassert_mem_equal(buf, &nvs_data[4], sizeof(buf));
}

ZTEST(nvs, test_nvs_uuid) {
    zassert_ok(nvs_init());

    /* too small buffer causes an error */
    uint8_t small_buf[8] = {0};
    zassert_equal(nvs_get_uuid(small_buf, sizeof(small_buf)), -ENOBUFS);

    uint8_t buf[16] = {0};
    zassert_ok(nvs_get_uuid(buf, sizeof(buf)));
    zassert_mem_equal(buf, &nvs_data[36], sizeof(buf));
}

ZTEST(nvs, test_nvs_dns_sd) {
    zassert_ok(nvs_init());

    uint8_t expected_dns_sd[] = "\x17serial=RPB1-2000123456I"
                                "\x29uuid=20a02fc2-a791-414b-bb66-c6407e78a710";
    zassert_mem_equal(nvs_dns_txt_record, expected_dns_sd, strlen(expected_dns_sd));
}

static void *nvs_setup(void) {
    zassert_ok(flash_area_open(FIXED_PARTITION_ID(manufacturing_partition), &fa));
    return NULL;
}

static void nvs_before(void *fixture) {
    zassert_ok(flash_area_erase(fa, 0, sizeof(nvs_data)));
    zassert_ok(flash_area_write(fa, 0, nvs_data, sizeof(nvs_data)));
}

ZTEST_SUITE(nvs, NULL, nvs_setup, nvs_before, NULL, NULL);
