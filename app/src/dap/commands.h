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

/* jtag ir instructions */
#define JTAG_IR_DPACC                       ((uint8_t) 0x0a)
#define JTAG_IR_APACC                       ((uint8_t) 0x0b)
#define JTAG_IR_IDCODE                      ((uint8_t) 0x0e)

/* dap transfer request bits */
#define TRANSFER_REQUEST_APnDP              ((uint8_t) 0x01)
#define TRANSFER_REQUEST_RnW                ((uint8_t) 0x02)
#define TRANSFER_REQUEST_MATCH_VALUE        ((uint8_t) 0x10)
#define TRANSFER_REQUEST_MATCH_MASK         ((uint8_t) 0x20)

#define TRANSFER_REQUEST_RnW_SHIFT          ((uint8_t) 0x01)
#define TRANSFER_REQUEST_A2_SHIFT           ((uint8_t) 0x02)
#define TRANSFER_REQUEST_A3_SHIFT           ((uint8_t) 0x03)

/* debug port addresses */
#define DP_ADDR_RDBUFF                      ((uint8_t) 0x0c)

/* dap transfer response bits */
#define TRANSFER_RESPONSE_ACK_OK            ((uint8_t) 0x01)
#define TRANSFER_RESPONSE_ACK_WAIT          ((uint8_t) 0x02)
#define TRANSFER_RESPONSE_VALUE_MISMATCH    ((uint8_t) 0x10)

void jtag_set_ir(const struct device *dev, uint32_t ir);
uint8_t jtag_transfer(const struct device *dev, uint8_t request, uint32_t *transfer_data);

int32_t dap_handle_command_info(const struct device *dev);
int32_t dap_handle_command_host_status(const struct device *dev);
int32_t dap_handle_command_connect(const struct device *dev);
int32_t dap_handle_command_disconnect(const struct device *dev);
int32_t dap_handle_command_transfer_configure(const struct device *dev);
int32_t dap_handle_command_transfer(const struct device *dev);
int32_t dap_handle_command_delay(const struct device *dev);
int32_t dap_handle_command_reset_target(const struct device *dev);
int32_t dap_handle_command_swj_pins(const struct device *dev);
int32_t dap_handle_command_swj_clock(const struct device *dev);
int32_t dap_handle_command_jtag_sequence(const struct device *dev);
int32_t dap_handle_command_jtag_configure(const struct device *dev);
int32_t dap_handle_command_jtag_idcode(const struct device *dev);

#endif /* __DAP_COMMANDS_PRIV_H__ */
