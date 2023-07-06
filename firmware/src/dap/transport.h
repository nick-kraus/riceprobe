#ifndef __DAP_TRANSPORT_H__
#define __DAP_TRANSPORT_H__

#include "dap/dap.h"

/** @brief Initializes the transport. */
typedef int32_t (*transport_init_t)(void);

/** @brief Checks if a transport is ready to accept data, configures if so, or returns a negative code if not. */
typedef int32_t (*transport_configure_t)(void);

/** @brief Waits for receive data to be available, reads into a buffer, then returns the number of bytes received. */
typedef int32_t (*transport_recv_t)(uint8_t *recv, size_t len);

/** @brief Sends a response, and returns the number of bytes sent. */
typedef int32_t (*transport_send_t)(uint8_t *send, size_t len);

struct dap_transport {
    const char *name;
    transport_init_t init;
    transport_configure_t configure;
    transport_recv_t recv;
    transport_send_t send;
};

#define DAP_TRANSPORT_DEFINE(_name, _init, _configure, _recv, _send)    \
    STRUCT_SECTION_ITERABLE(dap_transport, _name) = {                   \
        .name = #_name,                                                 \
        .init = _init,                                                  \
        .configure = _configure,                                        \
        .recv = _recv,                                                  \
        .send = _send,                                                  \
    }

#endif /* __DAP_TRANSPORT_H__ */
