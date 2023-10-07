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

int32_t dap_tcp_init(struct dap_driver *dap) {
    int32_t ret;

	DNS_SD_REGISTER_TCP_SERVICE(
		dap_dns_sd,
		CONFIG_NET_HOSTNAME,
		"_dap",
		"local",
		nvs_dns_txt_record,
		CONFIG_DAP_TCP_PORT
	);

	/* an IPv6 socket will still allow IPv4 connections using an IPv4-mapped IPv6 address */
	struct sockaddr_in6 sock_addr = {
		.sin6_family = AF_INET6,
		.sin6_addr = IN6ADDR_ANY_INIT,
		.sin6_port = sys_cpu_to_be16(CONFIG_DAP_TCP_PORT),
	};

    dap->tcp.bind_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (dap->tcp.bind_sock < 0) {
		LOG_ERR("socket initialize failed with error %d", *z_errno());
		return -1 * *z_errno();
	}

    ret = zsock_bind(dap->tcp.bind_sock, (struct sockaddr*) &sock_addr, sizeof(sock_addr));
	if (ret < 0) {
	    LOG_ERR("socket bind failed with error %d", *z_errno());
	    return -1 * *z_errno();
	}

    ret = zsock_listen(dap->tcp.bind_sock, 4);
	if (ret < 0) {
	    LOG_ERR("socket listen failed with error %d", *z_errno());
	    return -1 * *z_errno();
	}

    k_event_init(&dap->tcp.event);

    return 0;
}

int32_t dap_tcp_recv_begin(struct dap_driver *dap) {
	k_event_post(&dap->tcp.event, DAP_TCP_EVENT_RECV_BEGIN);
	return 0;
}

int32_t dap_tcp_send(struct dap_driver *dap) {
	k_event_post(&dap->tcp.event, DAP_TCP_EVENT_SEND);
	return 0;
}

void dap_tcp_stop(struct dap_driver *dap) {
	k_event_post(&dap->tcp.event, DAP_TCP_EVENT_STOP);
}

void dap_tcp_sock_disconnect(struct dap_driver *dap, int32_t sock) {
	k_event_set(&dap->tcp.event, 0);
	zsock_close(sock);
	k_event_post(&dap->thread.event, DAP_THREAD_EVENT_DISCONNECT);
}

void dap_tcp_thread_fn(void* arg1, void* arg2, void* arg3) {
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

	struct dap_driver *dap = arg1;
	/* active TCP socket, if > 0 */
	int32_t conn_sock = 0;

    while (1) {
        if (dap->thread.transport == DAP_TRANSPORT_NONE) {
			/* no transport currently connected, search for socket connections */
			struct sockaddr conn_addr;
			int32_t conn_addr_len = sizeof(conn_addr);
			conn_sock = zsock_accept(dap->tcp.bind_sock, &conn_addr, &conn_addr_len);
			if (conn_sock <= 0) {
				LOG_ERR("socket accept failed with error %d", *z_errno());
				continue;
			}

			k_event_post(&dap->thread.event, DAP_THREAD_EVENT_TCP_CONNECT);
			k_timepoint_t end_time = sys_timepoint_calc(K_SECONDS(10));
			while (!sys_timepoint_expired(end_time) && dap->thread.transport == DAP_TRANSPORT_NONE) {
				k_sleep(K_MSEC(10));
			}

			/* if we still haven't connected, just give up */
			if (dap->thread.transport == DAP_TRANSPORT_NONE) {
				LOG_ERR("failed to set tcp transport after socket accept");
				zsock_close(conn_sock);
			}
        } else if (dap->thread.transport != DAP_TRANSPORT_TCP) {
            /* nothing to do until disconnect */
            k_sleep(K_MSEC(250));
            continue;
        }

        uint32_t events = k_event_wait(
            &dap->tcp.event,
            DAP_TCP_EVENT_RECV_BEGIN | DAP_TCP_EVENT_SEND | DAP_TCP_EVENT_STOP,
            false,
            K_FOREVER
        );

		if ((events & DAP_TCP_EVENT_STOP) != 0) {
            /* used to break out of the event wait, the transport should already be set to NONE
             * by the main dap thread */
			k_event_set_masked(&dap->tcp.event, 0, DAP_TCP_EVENT_STOP);
        }

        if ((events & DAP_TCP_EVENT_RECV_BEGIN) != 0) {
			k_event_set_masked(&dap->tcp.event, 0, DAP_TCP_EVENT_RECV_BEGIN);
            /* tcp transport 'packets' are DAP requests preceeded by a 16-bit little endian request length,
			 * to make sure we know exactly when each message ends, which in practice should never
			 * be split between recv calls */
			uint16_t request_len;
			int32_t received = zsock_recv(conn_sock, (uint8_t*) &request_len, sizeof(request_len), ZSOCK_MSG_WAITALL);
			if (received == 0) {
				/* socket was closed on other end, disconnect but not unexpected */
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			} else if (received < 0) {
				LOG_ERR("socket receive failed with error %d", *z_errno());
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			} else if (received < 2) {
				LOG_ERR("failed to receive full request length from socket");
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			}

			uint32_t request_space = ring_buf_put_claim(
				&dap->buf.request,
				&dap->buf.request_tail,
				DAP_MAX_PACKET_SIZE
			);
			if (request_space < request_len) {
				LOG_ERR("not enough space in buffer for request");
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			}

			if (request_len > 0) {
				received = zsock_recv(conn_sock, dap->buf.request_tail, request_len, ZSOCK_MSG_WAITALL);
				if (received == 0) {
					/* socket was closed on other end, disconnect but not unexpected */
					dap_tcp_sock_disconnect(dap, conn_sock);
					continue;
				} else if (received < 0) {
					LOG_ERR("socket receive failed with error %d", *z_errno());
					dap_tcp_sock_disconnect(dap, conn_sock);
					continue;
				} else if (received < request_len) {
					LOG_ERR("failed to receive full request from socket");
					dap_tcp_sock_disconnect(dap, conn_sock);
					continue;
				}
			} else {
				received = 0;
			}

			if (ring_buf_put_finish(&dap->buf.request, received) < 0) {
				LOG_ERR("request buffer write finish failed");
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			}

			/* signal that data is ready to process, except if we receive a DAP Queue Commands request,
			 * in which case we don't immediately process the data */
			if (dap->buf.request_tail[0] != DAP_COMMAND_QUEUE_COMMANDS) {
				k_event_post(&dap->thread.event, DAP_THREAD_EVENT_READ_READY);
			} else {
				/* continue to read data from the transport until we have a full request to process */
				dap_tcp_recv_begin(dap);
			}
        }

        if ((events & DAP_TCP_EVENT_SEND) != 0) {
			k_event_set_masked(&dap->tcp.event, 0, DAP_TCP_EVENT_SEND);
			/* just like the request, tcp response messages will be preceeded by a 16-bit little endian value */
			uint16_t response_len = (uint16_t) ring_buf_size_get(&dap->buf.response);
			/* we will only queue up data here, it should all be sent together after the next send */
			int32_t sent = zsock_send(conn_sock, (uint8_t*) &response_len, sizeof(response_len), ZSOCK_MSG_DONTWAIT);
			if (sent == 0) {
				/* socket was closed on other end, disconnect but not unexpected */
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			} else if (sent < 0) {
				LOG_ERR("socket send failed with error %d", *z_errno());
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			} else if (sent < 2) {
				LOG_ERR("failed to send full response length to socket");
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			}

			/* because the buffer is reset before population, the full length should always be available
			 * from a single buffer pointer */
			uint8_t *ptr;
			uint32_t claim_size = ring_buf_get_claim(&dap->buf.response, &ptr, response_len);
			if (claim_size < response_len) {
				LOG_ERR("only %d bytes available in buffer for response size %d", claim_size, response_len);
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			}

			if (response_len > 0) {
				sent = zsock_send(conn_sock, ptr, response_len, ZSOCK_MSG_WAITALL);
				if (sent == 0) {
					/* socket was closed on other end, disconnect but not unexpected */
					dap_tcp_sock_disconnect(dap, conn_sock);
					continue;
				} else if (sent < 0) {
					LOG_ERR("socket send failed with error %d", *z_errno());
					dap_tcp_sock_disconnect(dap, conn_sock);
					continue;
				} else if (sent < response_len) {
					LOG_ERR("failed to send full response to socket");
					dap_tcp_sock_disconnect(dap, conn_sock);
					continue;
				}
			} else {
				sent = 0;
			}

			if (ring_buf_get_finish(&dap->buf.response, sent) < 0) {
				LOG_ERR("response buffer read finish failed");
				dap_tcp_sock_disconnect(dap, conn_sock);
				continue;
			}

			k_event_post(&dap->thread.event, DAP_THREAD_EVENT_WRITE_COMPLETE);
        }
    }
}
