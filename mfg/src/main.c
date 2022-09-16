#include <logging/log.h>
#include <usb/usb_device.h>
#include <zephyr.h>

void main(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(dev) || usb_enable(NULL)) {
		return;
	}
}
