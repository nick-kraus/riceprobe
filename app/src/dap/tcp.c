#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/socket.h>

#include "dap/dap.h"
#include "nvs.h"

LOG_MODULE_DECLARE(dap, CONFIG_DAP_LOG_LEVEL);

#define DAP_TCP_EVENT_RECV_BEGIN            (BIT(0))
#define DAP_TCP_EVENT_SEND                  (BIT(1))
#define DAP_TCP_EVENT_STOP                  (BIT(2))

/* TODO: for now this supports only IPv4, but we want to support IPv6 as well in the future */
int32_t dap_tcp_init(const struct device *dev) {
    struct dap_data *data = dev->data;
    int32_t ret;

	DNS_SD_REGISTER_TCP_SERVICE(
		dap_dns_sd,
		CONFIG_NET_HOSTNAME,
		"_dap",
		"local",
		nvs_dns_sd_txt_record,
		CONFIG_DAP_TCP_PORT
	);

    struct sockaddr_in sock_addr = {
		.sin_family = AF_INET,
		.sin_addr = INADDR_ANY_INIT,
		.sin_port = sys_cpu_to_be16(CONFIG_DAP_TCP_PORT),
	};

    data->tcp.bind_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (data->tcp.bind_sock < 0) {
		LOG_ERR("socket initialize failed with error %d", *z_errno());
		return -1 * *z_errno();
	}

    ret = zsock_bind(data->tcp.bind_sock, (struct sockaddr*) &sock_addr, sizeof(sock_addr));
	if (ret < 0) {
	    LOG_ERR("socket bind failed with error %d", *z_errno());
	    return -1 * *z_errno();
	}

    ret = zsock_listen(data->tcp.bind_sock, 4);
	if (ret < 0) {
	    LOG_ERR("socket listen failed with error %d", *z_errno());
	    return -1 * *z_errno();
	}

    k_event_init(&data->tcp.event);

    return 0;
}

int32_t dap_tcp_recv_begin(const struct device *dev) {
    struct dap_data *data = dev->data;

	k_event_post(&data->tcp.event, DAP_TCP_EVENT_RECV_BEGIN);
	return 0;
}

int32_t dap_tcp_send(const struct device *dev) {
    struct dap_data *data = dev->data;

	k_event_post(&data->tcp.event, DAP_TCP_EVENT_SEND);
	return 0;
}

void dap_tcp_stop(const struct device *dev) {
	struct dap_data *data = dev->data;

	k_event_post(&data->tcp.event, DAP_TCP_EVENT_STOP);
}

void dap_tcp_sock_disconnect(const struct device *dev, int32_t sock) {
	struct dap_data *data = dev->data;

	k_event_set(&data->tcp.event, 0);
	zsock_close(sock);
	k_event_post(&data->thread.event, DAP_THREAD_EVENT_DISCONNECT);
}

void dap_tcp_thread_fn(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

	const struct device *dev = (const struct device*) arg1;
    struct dap_data *data = dev->data;
	/* active TCP socket, if > 0 */
	int32_t conn_sock = 0;

    while (1) {
        if (data->thread.transport == DAP_TRANSPORT_NONE) {
			/* no transport currently connected, search for socket connections */
			struct sockaddr conn_addr;
			int32_t conn_addr_len = sizeof(conn_addr);
			conn_sock = zsock_accept(data->tcp.bind_sock, &conn_addr, &conn_addr_len);
			if (conn_sock <= 0) {
				LOG_ERR("socket accept failed with error %d", *z_errno());
				continue;
			}

			k_event_post(&data->thread.event, DAP_THREAD_EVENT_TCP_CONNECT);
			uint64_t end = sys_clock_timeout_end_calc(K_SECONDS(10));
			while (end - sys_clock_tick_get() > 0 && data->thread.transport == DAP_TRANSPORT_NONE) {
				k_sleep(K_MSEC(10));
			}

			/* if we still haven't connected, just give up */
			if (data->thread.transport == DAP_TRANSPORT_NONE) {
				LOG_ERR("failed to set tcp transport after socket accept");
				zsock_close(conn_sock);
			}
        } else if (data->thread.transport != DAP_TRANSPORT_TCP) {
            /* nothing to do until disconnect */
            k_sleep(K_MSEC(250));
            continue;
        }

        uint32_t events = k_event_wait(
            &data->tcp.event,
            DAP_TCP_EVENT_RECV_BEGIN | DAP_TCP_EVENT_SEND | DAP_TCP_EVENT_STOP,
            false,
            K_FOREVER
        );

		if ((events & DAP_TCP_EVENT_STOP) != 0) {
            /* used to break out of the event wait, the transport should already be set to NONE
             * by the main dap thread */
			k_event_set_masked(&data->tcp.event, 0, DAP_TCP_EVENT_STOP);
        }

        if ((events & DAP_TCP_EVENT_RECV_BEGIN) != 0) {
			k_event_set_masked(&data->tcp.event, 0, DAP_TCP_EVENT_RECV_BEGIN);
            /* tcp transport 'packets' are DAP requests preceeded by a 16-bit little endian request length,
			 * to make sure we know exactly when each message ends, which in practice should never
			 * be split between recv calls */
			uint16_t request_len;
			int32_t received = zsock_recv(conn_sock, (uint8_t*) &request_len, sizeof(request_len), ZSOCK_MSG_WAITALL);
			if (received == 0) {
				/* socket was closed on other end, disconnect but not unexpected */
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			} else if (received < 0) {
				LOG_ERR("socket receive failed with error %d", *z_errno());
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			} else if (received < 2) {
				LOG_ERR("failed to receive full request length from socket");
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			}

			uint32_t request_space = ring_buf_put_claim(
				&data->buf.request,
				&data->buf.request_tail,
				DAP_MAX_PACKET_SIZE
			);
			if (request_space < request_len) {
				LOG_ERR("not enough space in buffer for request");
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			}

			if (request_len > 0) {
				received = zsock_recv(conn_sock, data->buf.request_tail, request_len, ZSOCK_MSG_WAITALL);
				if (received == 0) {
					/* socket was closed on other end, disconnect but not unexpected */
					dap_tcp_sock_disconnect(dev, conn_sock);
					continue;
				} else if (received < 0) {
					LOG_ERR("socket receive failed with error %d", *z_errno());
					dap_tcp_sock_disconnect(dev, conn_sock);
					continue;
				} else if (received < request_len) {
					LOG_ERR("failed to receive full request from socket");
					dap_tcp_sock_disconnect(dev, conn_sock);
					continue;
				}
			} else {
				received = 0;
			}

			if (ring_buf_put_finish(&data->buf.request, received) < 0) {
				LOG_ERR("request buffer write finish failed");
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			}

			/* signal that data is ready to process, except if we receive a DAP Queue Commands request,
			 * in which case we don't immediately process the data */
			if (data->buf.request_tail[0] != DAP_COMMAND_QUEUE_COMMANDS) {
				k_event_post(&data->thread.event, DAP_THREAD_EVENT_READ_READY);
			} else {
				/* continue to read data from the transport until we have a full request to process */
				dap_tcp_recv_begin(dev);
			}
        }

        if ((events & DAP_TCP_EVENT_SEND) != 0) {
			k_event_set_masked(&data->tcp.event, 0, DAP_TCP_EVENT_SEND);
			/* just like the request, tcp response messages will be preceeded by a 16-bit little endian value */
			uint16_t response_len = (uint16_t) ring_buf_size_get(&data->buf.response);
			/* we will only queue up data here, it should all be sent together after the next send */
			int32_t sent = zsock_send(conn_sock, (uint8_t*) &response_len, sizeof(response_len), ZSOCK_MSG_DONTWAIT);
			if (sent == 0) {
				/* socket was closed on other end, disconnect but not unexpected */
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			} else if (sent < 0) {
				LOG_ERR("socket send failed with error %d", *z_errno());
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			} else if (sent < 2) {
				LOG_ERR("failed to send full response length to socket");
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			}

			/* because the buffer is reset before population, the full length should always be available
			 * from a single buffer pointer */
			uint8_t *ptr;
			uint32_t claim_size = ring_buf_get_claim(&data->buf.response, &ptr, response_len);
			if (claim_size < response_len) {
				LOG_ERR("only %d bytes available in buffer for response size %d", claim_size, response_len);
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			}

			if (response_len > 0) {
				sent = zsock_send(conn_sock, ptr, response_len, ZSOCK_MSG_WAITALL);
				if (sent == 0) {
					/* socket was closed on other end, disconnect but not unexpected */
					dap_tcp_sock_disconnect(dev, conn_sock);
					continue;
				} else if (sent < 0) {
					LOG_ERR("socket send failed with error %d", *z_errno());
					dap_tcp_sock_disconnect(dev, conn_sock);
					continue;
				} else if (sent < response_len) {
					LOG_ERR("failed to send full response to socket");
					dap_tcp_sock_disconnect(dev, conn_sock);
					continue;
				}
			} else {
				sent = 0;
			}

			if (ring_buf_get_finish(&data->buf.response, sent) < 0) {
				LOG_ERR("response buffer read finish failed");
				dap_tcp_sock_disconnect(dev, conn_sock);
				continue;
			}

			k_event_post(&data->thread.event, DAP_THREAD_EVENT_WRITE_COMPLETE);
        }
    }
}
