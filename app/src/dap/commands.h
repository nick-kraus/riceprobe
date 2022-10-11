#ifndef __DAP_COMMANDS_PRIV_H__
#define __DAP_COMMANDS_PRIV_H__

/* possible status responses to commands */
#define DAP_COMMAND_RESPONSE_OK     ((uint8_t) 0x00)
#define DAP_COMMAND_RESPONSE_ERROR  ((uint8_t) 0xff)

/* general command ids */
#define DAP_COMMAND_INFO                    ((uint8_t) 0x00)
#define DAP_COMMAND_HOST_STATUS             ((uint8_t) 0x01)
#define DAP_COMMAND_CONNECT                 ((uint8_t) 0x02)
#define DAP_COMMAND_DISCONNECT              ((uint8_t) 0x03)
#define DAP_COMMAND_TRANSFER_CONFIGURE      ((uint8_t) 0x04)
#define DAP_COMMAND_TRANSFER                ((uint8_t) 0x05)
#define DAP_COMMAND_TRANSFER_BLOCK          ((uint8_t) 0x06)
#define DAP_COMMAND_TRANSFER_ABORT          ((uint8_t) 0x07)
#define DAP_COMMAND_WRITE_ABORT             ((uint8_t) 0x08)
#define DAP_COMMAND_DELAY                   ((uint8_t) 0x09)
#define DAP_COMMAND_RESET_TARGET            ((uint8_t) 0x0a)
#define DAP_COMMAND_SWJ_PINS                ((uint8_t) 0x10)
#define DAP_COMMAND_SWJ_CLOCK               ((uint8_t) 0x11)
#define DAP_COMMAND_SWJ_SEQUENCE            ((uint8_t) 0x12)
#define DAP_COMMAND_SWD_CONFIGURE           ((uint8_t) 0x13)
#define DAP_COMMAND_JTAG_SEQUENCE           ((uint8_t) 0x14)
#define DAP_COMMAND_JTAG_CONFIGURE          ((uint8_t) 0x15)
#define DAP_COMMAND_JTAG_IDCODE             ((uint8_t) 0x16)
#define DAP_COMMAND_SWO_TRANSPORT           ((uint8_t) 0x17)
#define DAP_COMMAND_SWO_MODE                ((uint8_t) 0x18)
#define DAP_COMMAND_SWO_BAUDRATE            ((uint8_t) 0x19)
#define DAP_COMMAND_SWO_CONTROL             ((uint8_t) 0x1a)
#define DAP_COMMAND_SWO_STATUS              ((uint8_t) 0x1b)
#define DAP_COMMAND_SWO_DATA                ((uint8_t) 0x1c)
#define DAP_COMMAND_SWD_SEQUENCE            ((uint8_t) 0x1d)
#define DAP_COMMAND_SWO_EXTENDED_STATUS     ((uint8_t) 0x1e)
#define DAP_COMMAND_UART_TRANSPORT          ((uint8_t) 0x1f)
#define DAP_COMMAND_UART_CONFIGURE          ((uint8_t) 0x20)
#define DAP_COMMAND_UART_TRANSFER           ((uint8_t) 0x21)
#define DAP_COMMAND_UART_CONTROL            ((uint8_t) 0x22)
#define DAP_COMMAND_UART_STATUS             ((uint8_t) 0x23)
#define DAP_COMMAND_QUEUE_COMMANDS          ((uint8_t) 0x7e)
#define DAP_COMMAND_EXECUTE_COMMANDS        ((uint8_t) 0x7f)

int32_t dap_handle_command_info(const struct device *dev);
int32_t dap_handle_command_host_status(const struct device *dev);
int32_t dap_handle_command_connect(const struct device *dev);
int32_t dap_handle_command_disconnect(const struct device *dev);
int32_t dap_handle_command_write_abort(const struct device *dev);
int32_t dap_handle_command_delay(const struct device *dev);
int32_t dap_handle_command_reset_target(const struct device *dev);
int32_t dap_handle_command_swj_pins(const struct device *dev);
int32_t dap_handle_command_swj_clock(const struct device *dev);
int32_t dap_handle_command_jtag_configure(const struct device *dev);
int32_t dap_handle_command_jtag_sequence(const struct device *dev);
int32_t dap_handle_command_jtag_idcode(const struct device *dev);

#endif /* __DAP_COMMANDS_PRIV_H__ */
