#ifndef __NVS_H__
#define __NVS_H__

#include <stdint.h>

int32_t nvs_get_serial_number(char *sn, uint32_t len);
int32_t nvs_get_uuid(uint8_t *uuid, uint32_t len);

#endif /* __NVS_H__ */
