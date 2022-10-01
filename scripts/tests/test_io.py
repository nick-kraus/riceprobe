def test_usb_io_write_read(usb_io_eps):
    (out_ep, in_ep) = usb_io_eps

    out_ep.write(b'testing')
    data = in_ep.read(7)
    assert(data.tobytes() == b'testing')
