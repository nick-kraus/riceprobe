import usb.util

def test_write_read(usb_device):
    intf = usb.util.find_descriptor(
        usb_device.get_active_configuration(),
        custom_match=lambda i : usb.util.get_string(usb_device, i.iInterface) == 'Rice I/O v1'
    )
    (out_ep, in_ep) = intf.endpoints()

    out_ep.write(b'testing')
    assert(in_ep.read(512).tobytes() == b'testing')
