#ifndef __NVS_H__
#define __NVS_H__

#include <stdint.h>

/** @brief Initializes the non-volatile storage data. */
int32_t nvs_init(void);

/** @brief Retrieves the device serial number from non-volatile storage. */
int32_t nvs_get_serial_number(char *buf, size_t buf_len);

/** @brief Retrieves the device UUID from non-volatile storage. */
int32_t nvs_get_uuid(uint8_t *buf, size_t buf_len);

/** @brief Retrieves and formats the device UUID from non-volatile storage to a string. */
int32_t nvs_get_uuid_str(char *buf, size_t buf_len);

/** @brief A string of DNS-SD text records. */
extern const char nvs_dns_txt_record[68];

#endif /* __NVS_H__ */
