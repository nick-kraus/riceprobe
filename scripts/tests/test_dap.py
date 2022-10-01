import re
import time

def test_usb_dap_unsupported(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # unsupported writes should return the single hex byte 0xff
    out_ep.write(b'\xf0')
    data = in_ep.read(512)
    assert(len(data) == 1)
    assert(data.tobytes() == b'\xff')

def test_usb_dap_info_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # vendor name from info should match USB vendor string
    out_ep.write(b'\x00\x01')
    data = in_ep.read(512)
    assert(len(data) == 13)
    assert(data.tobytes() == b'\x00\x0bNick Kraus\x00')

    # product name from info should match USB product string
    out_ep.write(b'\x00\x02')
    data = in_ep.read(512)
    assert(len(data) == 12)
    assert(data.tobytes() == b'\x00\x0aRICEProbe\x00')

    # serial number from info should match USB serial number
    out_ep.write(b'\x00\x03')
    data = in_ep.read(512)
    assert(len(data) == 19)
    assert(data.tobytes()[0:2] == b'\x00\x11')
    assert(re.match(rb'^RPB1-[23][0-9][0-5][0-9][0-9]{6}[0-9A-Z]\x00$', data.tobytes()[2:]))

    # protocol version should match known string
    out_ep.write(b'\x00\x04')
    data = in_ep.read(512)
    assert(len(data) == 8)
    assert(data.tobytes() == b'\x00\x062.1.1\x00')

    # all of target device vendor, target device name, target board vendor, and
    # target board name should return an empty string
    out_ep.write(b'\x00\x05')
    data = in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')
    out_ep.write(b'\x00\x06')
    data = in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')
    out_ep.write(b'\x00\x07')
    data = in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')
    out_ep.write(b'\x00\x08')
    data = in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')

    # firmware version should match a known pattern
    out_ep.write(b'\x00\x09')
    data = in_ep.read(512)
    assert(len(data) >= 20)
    assert(data.tobytes()[0] == ord(b'\x00'))
    assert(data.tobytes()[1] == len(data) - 2)
    assert(re.match(rb'^v\d+\.\d+\.\d+-\d+-g[0-9a-f]{7}(-dirty)?\x00$', data.tobytes()[2:]))

    # capabilities should match a known value
    out_ep.write(b'\x00\xf0')
    data = in_ep.read(512)
    assert(len(data) == 4)
    assert(data.tobytes() == b'\x00\x02\x00\x01')

    # test domain timer should return the default unused value
    out_ep.write(b'\x00\xf1')
    data = in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x08\x00\x00\x00\x00')

    # uart rx and tx buffer size should match a known value
    out_ep.write(b'\x00\xfb')
    data = in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x04\x00\x04\x00\x00')
    out_ep.write(b'\x00\xfc')
    data = in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x04\x00\x04\x00\x00')

    # swo trace buffer size should return 0 while unsupported
    out_ep.write(b'\x00\xfd')
    data = in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x04\x00\x00\x00\x00')

    # usb packet count should match a known value
    out_ep.write(b'\x00\xfe')
    data = in_ep.read(512)
    assert(len(data) == 3)
    assert(data.tobytes() == b'\x00\x01\x02')

    # usb packet size should match a known value
    out_ep.write(b'\x00\xff')
    data = in_ep.read(512)
    assert(len(data) == 4)
    assert(data.tobytes() == b'\x00\x02\x00\x02')

    # unsupported info id
    out_ep.write(b'\x00\xbb')
    data = in_ep.read(512)
    assert(len(data) == 1)
    assert(data.tobytes() == b'\xff')

def test_usb_dap_host_status_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # enable connected led
    out_ep.write(b'\x01\x00\x01')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x01\x00')
    # enable running led
    out_ep.write(b'\x01\x01\x01')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x01\x00')
    # disable running led
    out_ep.write(b'\x01\x01\x00')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x01\x00')
    # disable connected led
    out_ep.write(b'\x01\x00\x00')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x01\x00')

    # make sure that an unsupported led type or status value produces an error
    out_ep.write(b'\x01\x02\x00')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x01\xff')
    out_ep.write(b'\x01\x00\x02')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x01\xff')

def test_usb_dap_delay_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    delay = 65535
    command = bytearray(b'\x09\x00\x00')
    command[1:3] = delay.to_bytes(2, byteorder='little')

    start = time.time()
    out_ep.write(command)
    data = in_ep.read(512)
    end = time.time()

    assert(data.tobytes() == b'\x09\x00')
    # not looking for much accuracy with such a small delay time, just something within reason
    delta = (end - start) * 1000000
    assert(delta > (0.5 * delay) and delta < (1.5 * delay))

def test_usb_dap_reset_target(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    out_ep.write(b'\x0a')
    data = in_ep.read(512)
    assert(data.tobytes() == b'\x0a\x00\x00')
