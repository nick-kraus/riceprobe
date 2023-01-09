#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>		/* TODO: unsure if keeping this */
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* TODO: unsure if keeping this */
static struct net_mgmt_event_callback net_mgmt_cb;
static void net_mgmt_handler(
	struct net_mgmt_event_callback *cb,
	uint32_t mgmt_event,
	struct net_if *iface
) {
	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) { return; }

	for (uint32_t i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].addr_type != NET_ADDR_DHCP) {
			continue;
		}

		LOG_WRN(
			"Your address: %s",
			net_addr_ntop(
				AF_INET,
				&iface->config.ip.ipv4->unicast[i].address.in_addr,
				buf,
				sizeof(buf)
			)
		);
		LOG_WRN(
			"Lease time: %u seconds",
			iface->config.dhcpv4.lease_time
		);
		LOG_WRN(
			"Subnet: %s",
			net_addr_ntop(
				AF_INET,
				&iface->config.ip.ipv4->netmask,
				buf,
				sizeof(buf)
			)
		);
		LOG_WRN(
			"Router: %s",
			net_addr_ntop(
				AF_INET,
				&iface->config.ip.ipv4->gw,
				buf,
				sizeof(buf)
			)
		);
	}
}

void main(void) {
	int32_t ret;

	const struct device *dap_device = DEVICE_DT_GET(DT_NODELABEL(dap));
	if (!device_is_ready(dap_device)) {
		LOG_ERR("failed to ready dap interface");
		return;
	}

	const struct device *io_device = DEVICE_DT_GET(DT_NODELABEL(io));
	if (!device_is_ready(io_device)) {
		LOG_ERR("failed to ready io interface");
		return;
	}

	const struct device *vcp_device = DEVICE_DT_GET(DT_NODELABEL(vcp));
	if (!device_is_ready(vcp_device)) {
		LOG_ERR("failed to ready vcp interface");
		return;
	}

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("failed to enable USB");
		return;
	}

	/* TODO: unsure if keeping this */
	net_mgmt_init_event_callback(&net_mgmt_cb, net_mgmt_handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&net_mgmt_cb);

	struct net_if *iface = net_if_get_first_up();
	net_dhcpv4_start(iface);

	LOG_INF("Main Initialization Finished, Handling USB Requests Now!");
}
