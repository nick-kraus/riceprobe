#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "dap/transport.h"

static int32_t dap_transport_init(void) {
    return 0;
}

static int32_t dap_transport_configure(void) {
    return 0;
}

static uint8_t buf[KB(3)];
static size_t buf_len;

/* a test request is available and the transport_recv function can continue */
static K_SEM_DEFINE(request_available, 0, 1);
static int32_t dap_transport_recv(uint8_t *recv, size_t len) {
    k_sem_take(&request_available, K_FOREVER);

    zassert(len > buf_len, "requested command length greater than available space");
    memcpy(recv, buf, buf_len);

    return buf_len;
}

static K_SEM_DEFINE(response_available, 0, 1);
static int32_t dap_transport_send(uint8_t *send, size_t len) {
    /* we should be all set with the request data in the buffer at this point */
    buf_len = len;
    memcpy(buf, send, len);

    k_sem_give(&response_available);

    return len;
}

DAP_TRANSPORT_DEFINE(
    dap_transport,
    dap_transport_init,
    dap_transport_configure,
    dap_transport_recv,
    dap_transport_send
);

void dap_transport_command(uint8_t *request, size_t request_len, uint8_t **response, size_t *response_len) {
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
