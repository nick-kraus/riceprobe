import pytest
import re
import time

@pytest.fixture
def dap_ep(usb_dap_intf):
    yield (usb_dap_intf.endpoints()[0], usb_dap_intf.endpoints()[1])

def test_usb_dap_unsupported(dap_ep):
    (dap_out_ep, dap_in_ep) = dap_ep

    # unsupported writes should return the single hex byte 0xff
    dap_out_ep.write(b'\xf0')
    data = dap_in_ep.read(512)
    assert(len(data) == 1)
    assert(data.tobytes() == b'\xff')

def test_usb_dap_info_command(dap_ep):
    (dap_out_ep, dap_in_ep) = dap_ep

    # vendor name from info should match USB vendor string
    dap_out_ep.write(b'\x00\x01')
    data = dap_in_ep.read(512)
    assert(len(data) == 13)
    assert(data.tobytes() == b'\x00\x0bNick Kraus\x00')

    # product name from info should match USB product string
    dap_out_ep.write(b'\x00\x02')
    data = dap_in_ep.read(512)
    assert(len(data) == 12)
    assert(data.tobytes() == b'\x00\x0aRICEProbe\x00')

    # serial number from info should match USB serial number
    dap_out_ep.write(b'\x00\x03')
    data = dap_in_ep.read(512)
    assert(len(data) == 19)
    assert(data.tobytes()[0:2] == b'\x00\x11')
    assert(re.match(rb'^RPB1-[23][0-9][0-5][0-9][0-9]{6}[0-9A-Z]\x00$', data.tobytes()[2:]))

    # protocol version should match known string
    dap_out_ep.write(b'\x00\x04')
    data = dap_in_ep.read(512)
    assert(len(data) == 8)
    assert(data.tobytes() == b'\x00\x062.1.1\x00')

    # all of target device vendor, target device name, target board vendor, and
    # target board name should return an empty string
    dap_out_ep.write(b'\x00\x05')
    data = dap_in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')
    dap_out_ep.write(b'\x00\x06')
    data = dap_in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')
    dap_out_ep.write(b'\x00\x07')
    data = dap_in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')
    dap_out_ep.write(b'\x00\x08')
    data = dap_in_ep.read(512)
    assert(len(data) == 2)
    assert(data.tobytes() == b'\x00\x00')

    # firmware version should match a known pattern
    dap_out_ep.write(b'\x00\x09')
    data = dap_in_ep.read(512)
    assert(len(data) >= 20)
    assert(data.tobytes()[0] == ord(b'\x00'))
    assert(data.tobytes()[1] == len(data) - 2)
    assert(re.match(rb'^v\d+\.\d+\.\d+-\d+-g[0-9a-f]{7}(-dirty)?\x00$', data.tobytes()[2:]))

    # capabilities should match a known value
    dap_out_ep.write(b'\x00\xf0')
    data = dap_in_ep.read(512)
    assert(len(data) == 4)
    assert(data.tobytes() == b'\x00\x02\x00\x01')

    # test domain timer should return the default unused value
    dap_out_ep.write(b'\x00\xf1')
    data = dap_in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x08\x00\x00\x00\x00')

    # uart rx and tx buffer size should match a known value
    dap_out_ep.write(b'\x00\xfb')
    data = dap_in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x04\x00\x04\x00\x00')
    dap_out_ep.write(b'\x00\xfc')
    data = dap_in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x04\x00\x04\x00\x00')

    # swo trace buffer size should return 0 while unsupported
    dap_out_ep.write(b'\x00\xfd')
    data = dap_in_ep.read(512)
    assert(len(data) == 6)
    assert(data.tobytes() == b'\x00\x04\x00\x00\x00\x00')

    # usb packet count should match a known value
    dap_out_ep.write(b'\x00\xfe')
    data = dap_in_ep.read(512)
    assert(len(data) == 3)
    assert(data.tobytes() == b'\x00\x01\x02')

    # usb packet size should match a known value
    dap_out_ep.write(b'\x00\xff')
    data = dap_in_ep.read(512)
    assert(len(data) == 4)
    assert(data.tobytes() == b'\x00\x02\x00\x02')

    # unsupported info id
    dap_out_ep.write(b'\x00\xbb')
    data = dap_in_ep.read(512)
    assert(len(data) == 1)
    assert(data.tobytes() == b'\xff')

def test_usb_dap_delay_command(dap_ep):
    (dap_out_ep, dap_in_ep) = dap_ep

    delay = 65535
    command = bytearray(b'\x09\x00\x00')
    command[1:3] = delay.to_bytes(2, byteorder='little')

    start = time.time()
    dap_out_ep.write(command)
    data = dap_in_ep.read(512)
    end = time.time()

    assert(data.tobytes() == b'\x09\x00')
    # not looking for much accuracy with such a small delay time, just something within reason
    delta = (end - start) * 1000000
    assert(delta > (0.5 * delay) and delta < (1.5 * delay))
