import pytest

def test_usb_io_write_read(usb_io_intf):
    io_out_ep = usb_io_intf.endpoints()[0]
    io_in_ep = usb_io_intf.endpoints()[1]

    io_out_ep.write(b'testing')
    data = io_in_ep.read(7)
    assert(data.tobytes() == b'testing')
