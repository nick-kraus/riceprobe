#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "io/io.h"
#include "io/transport.h"
#include "utf8.h"
#include "util.h"

LOG_MODULE_REGISTER(io, CONFIG_IO_LOG_LEVEL);

static struct io_driver io = {
    .pinctrls = IO_PINCTRL_FUNCS_DECLARE(),
    .gpios = IO_GPIOS_DECLARE(),
};

int32_t io_reset(struct io_driver *io) {
    LOG_INF("resetting driver state");
    int32_t ret;

    io->transport = NULL;

    for (uint8_t i = 0; i < ARRAY_SIZE(io->gpios); i++) {
        if ((ret = gpio_pin_configure_dt(&io->gpios[i], GPIO_INPUT)) < 0) {
            LOG_ERR("gpio%u initialize failed (%d)", i, ret);
            return ret;
        }
    }
    for (uint16_t i = 0; i < ARRAY_SIZE(io->pinctrls); i++) {
        if (io->pinctrls[i].function == IO_PIN_FUNC_GPIO && (ret = io_configure_pin(&io->pinctrls[i].pinctrl)) < 0) {
            LOG_ERR("io%u pinctrl configure failed (%d)", io->pinctrls[i].pin, ret);
            return ret;
        }
    }

    ring_buf_reset(&io->buf.request);
    ring_buf_reset(&io->buf.response);

    return 0;
}

int32_t io_handle_request(struct io_driver *io) {
    int32_t ret;
    
    /* this will usually just run once, unless an atomic command is being used */
    uint8_t num_commands = 1;
    bool queued_commands = false;
    do {
        if (queued_commands && num_commands == 0) {
            /* this may be the last command in the chain, if not we will reset this flag later */
            num_commands = 1;
            queued_commands = false;
        }

        /* data should be available at the front of the ring buffer before calling this handler */
        uint32_t command = UINT32_MAX;
        if ((ret = utf8_rbuf_get(&io->buf.request, &command)) < 0) return ret;

        /* multiple commands from separate requests */
        if (command == io_cmd_queue) {
            queued_commands = true;
            /* response of queued command is identical to execute commands, so replace the current
             * command to re-use the existing code path */
            command = io_cmd_multi;
        }
        /* multiple commands within one request */
        if (command == io_cmd_multi) {
            if (ring_buf_get(&io->buf.request, &num_commands, 1) != 1) return -EMSGSIZE;
            if ((ret = utf8_rbuf_put(&io->buf.response, command)) < 0) return ret;
            if (ring_buf_put(&io->buf.response, &num_commands, 1) != 1) return -ENOBUFS;
            /* get the next command for processing */
            if ((ret = utf8_rbuf_get(&io->buf.request, &command)) < 0) return ret;
        }
        
        if (command == io_cmd_info) { ret = io_handle_cmd_info(io); }
        else if (command == io_cmd_delay) { ret = io_handle_cmd_delay(io); }
        else if (command == io_cmd_pins_caps) { ret = io_handle_cmd_pins_caps(io); }
        else if (command == io_cmd_pins_default) { ret = io_handle_cmd_pins_default(io); }
        else if (command == io_cmd_pins_cfg) { ret = io_handle_cmd_pins_cfg(io); }
        else {
            LOG_ERR("unsupported command 0x%x", command);
            ret = -ENOTSUP;
        }

        if (ret < 0) {
            LOG_ERR("handle command 0x%x failed with error %d", command, ret);
            return ret;
        }
        num_commands--;

    } while (num_commands > 0 || queued_commands);

    return 0;
}

void io_thread_fn(void *arg1, void *arg2, void *arg3) {
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct io_driver *io = arg1;
    int32_t ret;

    while (1) {
        if (io->transport == NULL) {
            STRUCT_SECTION_FOREACH(io_transport, transport) {
                if ((ret = transport->configure()) == 0) {
                    LOG_DBG("configured transport %s", transport->name);
                    io->transport = transport;
                    /* just for passing the sleep below */
                    continue;
                } else if (ret < 0 && ret != -EAGAIN) {
                    LOG_ERR("transport configuration failed with error %d", ret);
                }
            }

            k_sleep(K_MSEC(50));
        }

        if (io->transport != NULL) {
            uint8_t *request;
            uint32_t request_len = ring_buf_put_claim(&io->buf.request, &request, IO_MAX_PACKET_SIZE);
            if ((ret = io->transport->recv(request, request_len)) < 0) {
                /* shutdown is an expected condition */
                if (ret != -ESHUTDOWN) LOG_ERR("transport receive failed with error %d", ret);
                io_reset(io);
                continue;
            }
            ring_buf_put_finish(&io->buf.request, ret);

            uint32_t command = UINT_MAX;
            utf8_decode(request, request_len, &command);
            /* not ready to process command, start the next receive */
            if (command == io_cmd_queue) continue;

            /* transport sends rely on having the full length of the response ring buffer from one pointer */
            ring_buf_reset(&io->buf.response);
            if ((ret = io_handle_request(io)) < 0) {
                /* commands that failed or aren't implemented get a simple 0xff (not supported) reponse byte */
                ring_buf_reset(&io->buf.response);
                uint8_t response = io_cmd_response_enotsup;
                FATAL_CHECK(ring_buf_put(&io->buf.response, &response, 1) == 1, "response buf is size 0");
            }

            uint8_t *response;
            uint32_t response_len = ring_buf_get_claim(&io->buf.response, &response, IO_RING_BUF_SIZE);
            if ((ret = io->transport->send(response, response_len)) < 0) {
                /* shutdown is an expected condition */
                if (ret != -ESHUTDOWN) LOG_ERR("transport send failed with error %d", ret);
                io_reset(io);
                continue;
            } else if (ret < response_len) {
                LOG_ERR("transport send dropped %d bytes", response_len - ret);
            }
            ring_buf_get_finish(&io->buf.response, ret);

            /* transport receives rely on having the full length of the request ring buffer from one pointer */
            ring_buf_reset(&io->buf.request);
        }
    }
}

K_THREAD_DEFINE(
    io_thread,
    KB(4),
    io_thread_fn,
    &io,
    NULL,
    NULL,
    CONFIG_MAIN_THREAD_PRIORITY + 1,
    0,
    K_TICKS_FOREVER
);

int32_t io_init(void) {
    int32_t ret;

    for (uint8_t i = 0; i < ARRAY_SIZE(io.gpios); i++) {
        FATAL_CHECK(gpio_is_ready_dt(&io.gpios[i]), "gpio not ready");
    }

    ring_buf_init(&io.buf.request, sizeof(io.buf.request_bytes), io.buf.request_bytes);
    ring_buf_init(&io.buf.response, sizeof(io.buf.response_bytes), io.buf.response_bytes);

    if ((ret = io_reset(&io)) < 0) return ret;

    STRUCT_SECTION_FOREACH(io_transport, transport) {
        if ((ret = transport->init()) < 0) {
            LOG_ERR("transport %s init failed with error %d", transport->name, ret);
            return ret;
        }
    }

    k_thread_start(io_thread);

    return 0;
}
