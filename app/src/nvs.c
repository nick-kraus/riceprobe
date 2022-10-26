#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

LOG_MODULE_REGISTER(nvs);

#define NVS_MFG_TAG_ADDRESS         0
#define NVS_MFG_VERSION_ADDRESS     2
#define NVS_MFG_SERIAL_ADDRESS      4
#define NVS_MFG_UUID_ADDRESS        36

#define NVS_MFG_TAG         0x7A5A
#define NVS_MFG_VERSION     0x0101

static int32_t nvs_init_status;
static char nvs_serial_number[32];
static uint8_t nvs_uuid[16];

int32_t nvs_get_serial_number(char *sn, uint32_t len) {
    if (nvs_init_status != 0) {
        return -ENODEV;
    } if (strlen(nvs_serial_number) >= len) {
        return -ENOBUFS;
    }

    strncpy(sn, nvs_serial_number, len);
    return 0;
}

int32_t nvs_get_uuid(uint8_t *uuid, uint32_t len) {
    if (nvs_init_status != 0) {
        return -ENODEV;
    } else if (sizeof(nvs_uuid) >= len) {
        return -ENOBUFS;
    }

    memcpy(uuid, nvs_uuid, len);
    return 0;
}

int32_t nvs_init(const struct device *dev) {
    ARG_UNUSED(dev);

    const struct flash_area *mfg_fa;
    int32_t ret = flash_area_open(FIXED_PARTITION_ID(manufacturing_partition), &mfg_fa);
    if (ret < 0) {
        LOG_ERR("flash area open failed with error %d", ret);
        nvs_init_status = -EIO;
        return nvs_init_status;
    }

    uint16_t mfg_tag;
    ret = flash_area_read(mfg_fa, NVS_MFG_TAG_ADDRESS, &mfg_tag, sizeof(mfg_tag));
    if (ret < 0) {
        LOG_ERR("flash area read failed with error %d", ret);
        nvs_init_status = -EIO;
        return nvs_init_status;
    } else if (mfg_tag != NVS_MFG_TAG) {
        LOG_ERR("found incorrect manufacturing tag %u", mfg_tag);
        nvs_init_status = -EINVAL;
        return nvs_init_status;
    }

    uint16_t mfg_version;
    ret = flash_area_read(mfg_fa, NVS_MFG_VERSION_ADDRESS, &mfg_version, sizeof(mfg_version));
    if (ret < 0) {
        LOG_ERR("flash area read failed with error %d", ret);
        nvs_init_status = -EIO;
        return nvs_init_status;
    } else if (mfg_version != NVS_MFG_VERSION) {
        LOG_ERR("found unsupported manufacturing version %u", mfg_version);
        nvs_init_status = -ENOTSUP;
        return nvs_init_status;
    }

    ret = flash_area_read(mfg_fa, NVS_MFG_SERIAL_ADDRESS, &nvs_serial_number, sizeof(nvs_serial_number));
    if (ret < 0) {
        LOG_ERR("flash area read failed with error %d", ret);
        nvs_init_status = -EIO;
        return nvs_init_status;
    } else if (nvs_serial_number[sizeof(nvs_serial_number) - 1] != '\0') {
        LOG_ERR("serial number doesn't end with null terminator");
        nvs_init_status = -EINVAL;
        return nvs_init_status;
    }

    ret = flash_area_read(mfg_fa, NVS_MFG_UUID_ADDRESS, &nvs_uuid, sizeof(nvs_uuid));
    if (ret < 0) {
        LOG_ERR("flash area read failed with error %d", ret);
        nvs_init_status = -EIO;
        return nvs_init_status;
    }

    flash_area_close(mfg_fa);
    nvs_init_status = 0;
    return nvs_init_status;
}

SYS_INIT(nvs_init, POST_KERNEL, 80);
