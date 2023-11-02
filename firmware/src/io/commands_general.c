#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/ring_buffer.h>

#include "io/io.h"
#include "nvs.h"
#include "utf8.h"
#include "util.h"

LOG_MODULE_DECLARE(io, CONFIG_IO_LOG_LEVEL);

/* request:
 * | UTF8 | UTF8 | 
 * | 0x01 | ID   |
 * 
 * response:
 * | UTF8 | U8     | U8  | U8+  | 
 * | 0x01 | Status | Len | Data |
 */
int32_t io_handle_cmd_info(struct io_driver *io) {
    int32_t ret;

    /* info subcommands */
    const uint32_t info_max_packet_size = 0x01;
    const uint32_t info_max_buffer_size = 0x02;
    const uint32_t info_vendor_name = 0x03;
    const uint32_t info_product_name = 0x04;
    const uint32_t info_serial_number = 0x05;
    const uint32_t info_uuid = 0x06;
    const uint32_t info_firmware_version = 0x07;
    const uint32_t info_protocol_version = 0x08;
    const uint32_t info_supported_subsystems = 0x09;
    const uint32_t info_num_gpio = 0x0a;

    /* supported subsystems byte 0 (capabilities for each subsystem exist as their own command) */
    const uint8_t subsys_no_gpio_support = 0x00;
    const uint8_t subsys_no_uart_support = 0x00;
    const uint8_t subsys_no_i2c_support = 0x00;
    const uint8_t subsys_no_spi_support = 0x00;
    const uint8_t subsys_no_pwm_support = 0x00;
    const uint8_t subsys_no_adc_support = 0x00;

    /* supported protocol version (TODO: should be 1.x.x before real release) */
    const char io_protocol_version[] = "0.1.0";

    uint32_t id = 0;
    if ((ret = utf8_rbuf_get(&io->buf.request, &id)) < 0) return ret;
    if ((ret = utf8_rbuf_put(&io->buf.response, io_cmd_info)) < 0) return ret;

    uint8_t *ptr;
    uint32_t space;
    if (id == info_max_packet_size) {
        uint8_t response[4] = { io_cmd_response_ok, 0x02, 0x00, 0x00 };
        sys_put_le16((IO_MAX_PACKET_SIZE), &response[2]);
        if (ring_buf_put(&io->buf.response, response, 4) != 4) return -ENOBUFS;
    } else if (id == info_max_buffer_size) {
        uint8_t response[4] = { io_cmd_response_ok, 0x02, 0x00, 0x00 };
        sys_put_le16(IO_RING_BUF_SIZE, &response[2]);
        if (ring_buf_put(&io->buf.response, response, 4) != 4) return -ENOBUFS;
    } else if (id == info_vendor_name) {
        const char *vendor_name = CONFIG_PRODUCT_MANUFACTURER;
        const uint8_t str_len = sizeof(CONFIG_PRODUCT_MANUFACTURER);
        uint8_t response[2] = { io_cmd_response_ok, str_len };
        if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&io->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, vendor_name, space);
        if (ring_buf_put_finish(&io->buf.response, space) < 0) return -ENOBUFS;
    } else if (id == info_product_name) {
        const char *product_name = CONFIG_PRODUCT_DESCRIPTOR;
        const uint8_t str_len = sizeof(CONFIG_PRODUCT_DESCRIPTOR);
        uint8_t response[2] = { io_cmd_response_ok, str_len };
        if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&io->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, product_name, space);
        if (ring_buf_put_finish(&io->buf.response, space) < 0) return -ENOBUFS;
    } else if (id == info_serial_number) {
        char serial[sizeof(CONFIG_PRODUCT_SERIAL_FORMAT)];
        if (nvs_get_serial_number(serial, sizeof(serial)) < 0) return -EINVAL;
        const uint8_t str_len = sizeof(CONFIG_PRODUCT_SERIAL_FORMAT);
        uint8_t response[2] = { io_cmd_response_ok, str_len };
        if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&io->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, serial, space);
        if (ring_buf_put_finish(&io->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_uuid) {
        char uuid[37];
        if (nvs_get_uuid_str(uuid, sizeof(uuid)) < 0) return -EINVAL;
        const uint8_t str_len = sizeof(uuid);
        uint8_t response[2] = { io_cmd_response_ok, str_len };
        if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&io->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, uuid, space);
        if (ring_buf_put_finish(&io->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_firmware_version) {
        const char *firmware_version = CONFIG_REPO_VERSION_STRING;
        const uint8_t str_len = sizeof(CONFIG_REPO_VERSION_STRING);
        uint8_t response[2] = { io_cmd_response_ok, str_len };
        if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&io->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, firmware_version, space);
        if (ring_buf_put_finish(&io->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_protocol_version) {
        const uint8_t str_len = sizeof(io_protocol_version);
        uint8_t response[2] = { io_cmd_response_ok, str_len };
        if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
        if ((space = ring_buf_put_claim(&io->buf.response, &ptr, str_len)) != str_len) return -ENOBUFS;
        strncpy(ptr, io_protocol_version, space);
        if (ring_buf_put_finish(&io->buf.response, space) != 0) return -ENOBUFS;
    } else if (id == info_supported_subsystems) {
        const uint8_t subsys_len = 1;
        const uint8_t subsys0 = subsys_no_gpio_support |
                                subsys_no_uart_support |
                                subsys_no_i2c_support |
                                subsys_no_spi_support |
                                subsys_no_pwm_support |
                                subsys_no_adc_support;
        uint8_t response[3] = { io_cmd_response_ok, subsys_len, subsys0 };
        if (ring_buf_put(&io->buf.response, response, 3) != 3) return -ENOBUFS;
    } else if (id == info_num_gpio) {
        uint8_t response[4] = { io_cmd_response_ok, 2, 0, 0 };
        sys_put_le16(IO_GPIOS_CNT(), &response[2]);
        if (ring_buf_put(&io->buf.response, response, 4) != 4) return -ENOBUFS;
    } else {
        if (ring_buf_put(&io->buf.response, &io_cmd_response_enotsup, 1) != 1) return -ENOBUFS;
    }

    return 0;
}

/* request:
 * | UTF8 | U32        |
 * | 0x04 | Delay (uS) |
 * 
 * response:
 * | UTF8 | U8     |
 * | 0x04 | Status |
 */
int32_t io_handle_cmd_delay(struct io_driver *io) {
    uint32_t delay_us = 0;
    if (ring_buf_get_le32(&io->buf.request, &delay_us) < 0) return -EMSGSIZE;
    k_sleep(K_USEC(delay_us));
    
    uint8_t response[] = { io_cmd_delay, io_cmd_response_ok };
    if (ring_buf_put(&io->buf.response, response, 2) != 2) return -ENOBUFS;
    return 0;
}
