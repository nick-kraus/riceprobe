#ifndef __DAP_COMMANDS_PRIV_H__
#define __DAP_COMMANDS_PRIV_H__

/* possible status responses to commands */
#define DAP_COMMAND_RESPONSE_OK     ((uint8_t) 0x00)
#define DAP_COMMAND_RESPONSE_ERROR  ((uint8_t) 0xff)

/* general command ids */
#define DAP_COMMAND_INFO            ((uint8_t) 0x00)
#define DAP_COMMAND_HOST_STATUS     ((uint8_t) 0x01)
#define DAP_COMMAND_CONNECT         ((uint8_t) 0x02)
#define DAP_COMMAND_DISCONNECT      ((uint8_t) 0x03)
#define DAP_COMMAND_WRITE_ABORT     ((uint8_t) 0x08)
#define DAP_COMMAND_DELAY           ((uint8_t) 0x09)
#define DAP_COMMAND_RESET_TARGET    ((uint8_t) 0x0a)

int32_t dap_handle_command_info(const struct device *dev);
int32_t dap_handle_command_host_status(const struct device *dev);
int32_t dap_handle_command_connect(const struct device *dev);
int32_t dap_handle_command_disconnect(const struct device *dev);
int32_t dap_handle_command_write_abort(const struct device *dev);
int32_t dap_handle_command_delay(const struct device *dev);
int32_t dap_handle_command_reset_target(const struct device *dev);

#endif /* __DAP_COMMANDS_PRIV_H__ */