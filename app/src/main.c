#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void main(void) {
	int32_t ret;

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("failed to enable USB");
		return;
	}

	LOG_INF("Main Initialization Finished, Handling USB Requests Now!");
}
