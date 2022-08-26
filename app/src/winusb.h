#ifndef __WINUSB_H__
#define __WINUSB_H__

#include <stdint.h>

extern uint8_t *winusb_func0_first_interface;
extern uint8_t *winusb_func1_first_interface;

int32_t winusb_custom_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data);
int32_t winusb_vendor_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data);

#endif /* __WINUSB_H__ */
