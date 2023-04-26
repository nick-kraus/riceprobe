#include <zephyr/drivers/gpio.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "dap/transport.h"
#include "main.h"
#include "tests/util.h"

/* non volatile storage setup */

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

/* dap driver setup */

static int32_t transport_init(void) {
    return 0;
}

static bool configured = true;
static int32_t transport_configure(void) {
    return configured ? 0 : -EAGAIN;
}

static uint8_t buf[KB(3)];
static size_t buf_len;

/* a test request is available and the transport_recv function can continue */
static K_SEM_DEFINE(request_available, 0, 1);
static int32_t transport_recv(uint8_t *recv, size_t len) {
    k_sem_take(&request_available, K_FOREVER);

    zassert(len > buf_len, "requested command length greater than available space");
    memcpy(recv, buf, buf_len);

    return buf_len;
}

static K_SEM_DEFINE(response_available, 0, 1);
static int32_t transport_send(uint8_t *send, size_t len) {
    /* we should be all set with the request data in the buffer at this point */
    buf_len = len;
    memcpy(buf, send, len);

    k_sem_give(&response_available);

    return len;
}

DAP_TRANSPORT_DEFINE(
    transport,
    transport_init,
    transport_configure,
    transport_recv,
    transport_send
);

/* dap driver control and command */

void dap_command(uint8_t *request, size_t request_len, uint8_t **response, size_t *response_len) {
    buf_len = request_len;
    memcpy(buf, request, request_len);
    k_sem_give(&request_available);

    /* queued commands will never call the send function, since no response has been created, but we
     * want to make sure it isn't called, so wait for a reasonable timeout and then return no data. */
    if (k_sem_take(&response_available, K_SECONDS(1)) == -EAGAIN) {
        *response_len = 0;
    } else {
        *response_len = buf_len;
    }
    *response = buf;
}

void dap_configure(bool enable) {
    configured = enable ? true : false;
}

/* fixture and test setup */

static struct dap_fixture fixture = {
    .io = {
        .tck_swclk = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tck_swclk_gpios),
        .tms_swdio = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tms_swdio_gpios),
        .tdo = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tdo_gpios),
        .tdi = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), tdi_gpios),
        .nreset = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), nreset_gpios),
        .vtref = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), vtref_gpios),
        .led_connect = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), led_connect_gpios),
        .led_running = GPIO_DT_SPEC_GET(DT_NODELABEL(dap), led_running_gpios),
    },
};

int32_t nvs_init(void);
int32_t dap_init(void);

static void *dap_tests_setup(void) {
    const struct flash_area *fa;
    zassert_ok(flash_area_open(FIXED_PARTITION_ID(manufacturing_partition), &fa));
    zassert_ok(flash_area_erase(fa, 0, sizeof(nvs_data)));
    zassert_ok(flash_area_write(fa, 0, nvs_data, sizeof(nvs_data)));
    flash_area_close(fa);

    zassert_ok(nvs_init());
    zassert_ok(dap_init());

    /* make sure the test transport has been selected by the time the test runs */
    k_sleep(K_MSEC(100));

    return &fixture;
}

ZTEST_SUITE(dap, NULL, dap_tests_setup, NULL, NULL, NULL);
