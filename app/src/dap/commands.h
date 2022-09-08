#ifndef __DAP_COMMANDS_PRIV_H__
#define __DAP_COMMANDS_PRIV_H__

// possible status responses to commands
#define DAP_COMMAND_RESPONSE_OK     ((uint8_t) 0x00)
#define DAP_COMMAND_RESPONSE_ERROR  ((uint8_t) 0xff)

// possible command ids
#define DAP_COMMAND_INFO    ((uint8_t) 0x00)

int32_t dap_handle_command_info(const struct device *dev);

#endif /* __DAP_COMMANDS_PRIV_H__ */
