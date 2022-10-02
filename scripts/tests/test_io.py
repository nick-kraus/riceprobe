def test_write_read(usb_io_eps):
    (out_ep, in_ep) = usb_io_eps

    out_ep.write(b'testing')
    assert(in_ep.read(512).tobytes() == b'testing')
