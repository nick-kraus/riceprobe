#ifndef __WINUSB_H__
#define __WINUSB_H__

extern uint8_t *winusb_func0_first_interface;
extern uint8_t *winusb_func1_first_interface;

int winusb_custom_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data);
int winusb_vendor_handle_req(struct usb_setup_packet *pSetup, int32_t *len, uint8_t **data);

#endif /* __WINUSB_H__ */
