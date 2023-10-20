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

static struct {
    char serial_number[32];
    uint8_t uuid[16];
} nvs;

/* this value unfortunately can't be in the above struct because the DNS-SD
 * services are registered in a macro. */
char nvs_dns_txt_record[68];

int32_t nvs_get_serial_number(char *buf, size_t buf_len) {
    if (buf_len <= strlen(nvs.serial_number)) {
        return -ENOBUFS;
    }

    strncpy(buf, nvs.serial_number, buf_len);
    return 0;
}

int32_t nvs_get_uuid(char *buf, size_t buf_len) {
    if (buf_len < sizeof(nvs.uuid)) {
        return -ENOBUFS;
    }

    memcpy(buf, nvs.uuid, buf_len);
    return 0;
}

int32_t nvs_get_uuid_str(char *buf, size_t buf_len) {
    /* two characters for each byte, plus four separators and null terminator */
    if (buf_len < sizeof(nvs.uuid) * 2 + 4 + 1) {
        return -ENOBUFS;
    }

    snprintk(
        buf,
        buf_len,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        nvs.uuid[0], nvs.uuid[1], nvs.uuid[2], nvs.uuid[3], 
        nvs.uuid[4], nvs.uuid[5], nvs.uuid[6], nvs.uuid[7], 
        nvs.uuid[8], nvs.uuid[9], nvs.uuid[10], nvs.uuid[11], 
        nvs.uuid[12], nvs.uuid[13], nvs.uuid[14], nvs.uuid[15]
    );
    return 0;
}

int32_t nvs_init(void) {
    const struct flash_area *mfg_fa;
    int32_t ret = flash_area_open(FIXED_PARTITION_ID(manufacturing_partition), &mfg_fa);
    if (ret < 0) {
        LOG_ERR("flash open failed with error %d", ret);
        return -EIO;
    }

    uint16_t mfg_tag;
    ret = flash_area_read(mfg_fa, NVS_MFG_TAG_ADDRESS, &mfg_tag, sizeof(mfg_tag));
    if (ret < 0) {
        LOG_ERR("flash read failed with error %d", ret);
        return -EIO;
    } else if (mfg_tag != NVS_MFG_TAG) {
        LOG_ERR("found incorrect manufacturing tag 0x%x", mfg_tag);
        return -EINVAL;
    }

    uint16_t mfg_version;
    ret = flash_area_read(mfg_fa, NVS_MFG_VERSION_ADDRESS, &mfg_version, sizeof(mfg_version));
    if (ret < 0) {
        LOG_ERR("flash read failed with error %d", ret);
        return -EIO;
    } else if (mfg_version != NVS_MFG_VERSION) {
        LOG_ERR("found unsupported manufacturing version %u", mfg_version);
        return -ENOTSUP;
    }

    ret = flash_area_read(mfg_fa, NVS_MFG_SERIAL_ADDRESS, &nvs.serial_number, sizeof(nvs.serial_number));
    if (ret < 0) {
        LOG_ERR("flash read failed with error %d", ret);
        return -EIO;
    } else if (nvs.serial_number[sizeof(nvs.serial_number) - 1] != '\0') {
        LOG_ERR("serial number doesn't end with null terminator");
        return -EINVAL;
    }

    ret = flash_area_read(mfg_fa, NVS_MFG_UUID_ADDRESS, &nvs.uuid, sizeof(nvs.uuid));
    if (ret < 0) {
        LOG_ERR("flash read failed with error %d", ret);
        return -EIO;
    }

    flash_area_close(mfg_fa);

    /* dns-sd txt records from the various interfaces are all identical, ideal to
	 * create one copy here and share between all services. see rfc 6763 for the
	 * format of the txt record */
    uint8_t entry_idx = 0;
    /* first entry is serial number, length is strlen("serial=") + strlen(serial) */
    snprintk(
        &nvs_dns_txt_record[entry_idx],
        sizeof(nvs_dns_txt_record) - entry_idx,
        "%cserial=%s",
        7 + strlen(nvs.serial_number),
        nvs.serial_number
    );
    entry_idx = strlen(nvs_dns_txt_record);
    /* second entry is uuid, length is strlen("uuid=") + 36 (uuid string length) */
    snprintk(
        &nvs_dns_txt_record[entry_idx],
        sizeof(nvs_dns_txt_record) - entry_idx,
        "%cuuid=%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        5 + 36,
        nvs.uuid[0], nvs.uuid[1], nvs.uuid[2], nvs.uuid[3], 
        nvs.uuid[4], nvs.uuid[5], nvs.uuid[6], nvs.uuid[7], 
        nvs.uuid[8], nvs.uuid[9], nvs.uuid[10], nvs.uuid[11], 
        nvs.uuid[12], nvs.uuid[13], nvs.uuid[14], nvs.uuid[15]
    );

    return 0;
}
