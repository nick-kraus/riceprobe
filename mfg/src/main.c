#include <shell/shell.h>
#include <sys/types.h>
#include <usb/usb_device.h>
#include <zephyr.h>

#define PARTITION_GET_INFO(part)                                    \
    {                                                               \
        .part_label = DT_LABEL(part),                               \
        .dev_name = DT_LABEL(DT_MTD_FROM_FIXED_PARTITION(part)),    \
        .part_size = DT_REG_SIZE(part),                             \
        .part_offset = DT_REG_ADDR(part),                           \
    },
#define FOREACH_PARTITION_GET_INFO(part) DT_FOREACH_CHILD(part, PARTITION_GET_INFO)

const struct {
    const char *part_label;
    const char *dev_name;
    size_t part_size;
    off_t part_offset;
} partition_info[] = {
    DT_FOREACH_STATUS_OKAY(fixed_partitions, FOREACH_PARTITION_GET_INFO)
};

static int cmd_partition_info(const struct shell *shell, size_t argc, char **argv) {
    const char *part_label = argv[1];
    for (int i = 0; i < ARRAY_SIZE(partition_info); i++) {
        if (strcmp(part_label, partition_info[i].part_label) == 0) {
            shell_print(shell, "partition \"%s\":", partition_info[i].part_label);
            shell_print(shell, "\tdevice name = %s", partition_info[i].dev_name);
            shell_print(shell, "\tpartition size = 0x%x", partition_info[i].part_size);
            shell_print(shell, "\tpartition offset = 0x%x", partition_info[i].part_offset);

            return 0;
        }
    }

    shell_error(shell, "partition with name \"%s\" not found", part_label);
    return -ENOENT;
}

SHELL_CMD_ARG_REGISTER(partition_info, NULL, "Show info for a given flash partition label", cmd_partition_info, 2, 0);

void main(void) {
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(dev) || usb_enable(NULL)) {
		return;
	}    
}
