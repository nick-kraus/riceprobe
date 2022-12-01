#ifndef __IO_COMMANDS_PRIV_H__
#define __IO_COMMANDS_PRIV_H__

/* possible status responses to commands */
#define IO_OK                       ((uint8_t) 0x00)
#define IO_ERROR_UNKNOWN            ((uint8_t) 0x80)
#define IO_ERROR_UNSUPPORTED        ((uint8_t) 0x81)
#define IO_ERROR_INVALID            ((uint8_t) 0x82)

#endif /* __IO_COMMANDS_PRIV_H__ */