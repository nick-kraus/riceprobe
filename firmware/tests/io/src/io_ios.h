#ifndef __TEST_IO_IOS_H__
#define __TEST_IO_IOS_H__

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "io/io.h"

static const struct gpio_dt_spec io_gpios[] = IO_GPIOS_DECLARE();

#endif /* __TEST_IO_IOS_H__ */
