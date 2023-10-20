#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/posix/fcntl.h>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/socket.h>

#include "nvs.h"
#include "transport.h"

LOG_MODULE_REGISTER(dap_tcp, CONFIG_DAP_LOG_LEVEL);

static int32_t tcp_bind_sock;
static int32_t tcp_conn_sock;

int32_t dap_tcp_transport_init(void) {
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

	if ((ret = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		LOG_ERR("socket initialize failed with error %d", errno);
		return -1 * errno;
	}
    tcp_bind_sock = ret;

    if ((ret = zsock_bind(tcp_bind_sock, (struct sockaddr*) &sock_addr, sizeof(sock_addr))) < 0) {
	    LOG_ERR("socket bind failed with error %d", errno);
	    return -1 * errno;
	}

	if ((ret = zsock_listen(tcp_bind_sock, 4)) < 0) {
	    LOG_ERR("socket listen failed with error %d", errno);
	    return -1 * errno;
	}

    return 0;
}

static int32_t tcp_set_nonblocking(int32_t sock, bool enabled) {
    int32_t flags = zsock_fcntl(sock, F_GETFL, 0);
    if (flags < 0) return flags;
    return zsock_fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int32_t dap_tcp_transport_configure(void) {
    int32_t ret;

    if ((ret = tcp_set_nonblocking(tcp_bind_sock, true)) < 0) {
        LOG_ERR("socket nonblocking set failed with error %d", errno);
        return -1 * errno;
    }

    struct sockaddr conn_addr;
    int32_t conn_addr_len = sizeof(conn_addr);
    if ((ret = zsock_accept(tcp_bind_sock, &conn_addr, &conn_addr_len)) < 0) {
        /* most likely situation is returning -EAGAIN here, when no connections are ready */
        return -1 * errno;
    }
    tcp_conn_sock = ret;

    /* if we have a connection, make sure all remaining operations are blocking */
    if ((ret = tcp_set_nonblocking(tcp_bind_sock, true)) < 0) {
        LOG_ERR("socket nonblocking set failed with error %d", errno);
        return -1 * errno;
    }

    return 0;
}

int32_t dap_tcp_transport_recv(uint8_t *read, size_t len) {
    /* tcp transport 'packets' are DAP requests preceeded by a 16-bit little endian request length,
     * to make sure we know exactly when each message ends, which in practice should never
     * be split between recv calls */
    uint16_t request_len;
    int32_t received = zsock_recv(tcp_conn_sock, (uint8_t*) &request_len, sizeof(request_len), ZSOCK_MSG_WAITALL);
    if (received == 0) {
        /* socket was close on other end, an expected disconnect condition */
        zsock_close(tcp_conn_sock);
        return -ESHUTDOWN;
    } else if (received < 0) {
        LOG_ERR("socket receive failed with error %d", errno);
        zsock_close(tcp_conn_sock);
        return -1 * errno;
    } else if (received < 2) {
        LOG_ERR("failed to receive full request length from socket");
        zsock_close(tcp_conn_sock);
        return -ENODATA;
    }

    if (request_len > len) {
        LOG_ERR("not enough space in buffer for full request");
        zsock_close(tcp_conn_sock);
        return -ENOBUFS;
    } else if (request_len > 0) {
        received = zsock_recv(tcp_conn_sock, read, request_len, ZSOCK_MSG_WAITALL);
        if (received == 0) {
            /* socket was close on other end, an expected disconnect condition */
            zsock_close(tcp_conn_sock);
            return -ESHUTDOWN;
        } else if (received < 0) {
            LOG_ERR("socket receive failed with error %d", errno);
            zsock_close(tcp_conn_sock);
            return -1 * errno;
        } else if (received < request_len) {
            LOG_ERR("failed to receive full request length from socket");
            zsock_close(tcp_conn_sock);
            return -ENODATA;
        }
    }

    return received;
}

int32_t dap_tcp_transport_send(uint8_t *send, size_t len) {
    /* just like the request, tcp response messages will be preceeded by a 16-bit little endian value */
    uint16_t response_len = (uint16_t) len;

    struct iovec msg_iov[2];
    msg_iov[0].iov_base = (uint8_t*) &response_len;
    msg_iov[0].iov_len = sizeof(response_len);
    msg_iov[1].iov_base = send;
    msg_iov[1].iov_len = len;
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = msg_iov;
    msg.msg_iovlen = 2;

    int32_t sent = zsock_sendmsg(tcp_conn_sock, &msg, 0);
    if (sent == 0) {
        /* socket was close on other end, an expected disconnect condition */
        zsock_close(tcp_conn_sock);
        return -ESHUTDOWN;
    } else if (sent < 0) {
        LOG_ERR("socket send failed with error %d", errno);
        zsock_close(tcp_conn_sock);
        return -1 * errno;
    } else if (sent < response_len + 2) {
        LOG_ERR("failed to send full response to socket");
        zsock_close(tcp_conn_sock);
        return -ENODATA;
    }

    /* the 2-byte preceeded length must be transparent to the caller */
    return sent - 2;
}

DAP_TRANSPORT_DEFINE(
    dap_tcp,
    dap_tcp_transport_init,
    dap_tcp_transport_configure,
    dap_tcp_transport_recv,
    dap_tcp_transport_send
);
