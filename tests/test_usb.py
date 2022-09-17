import pytest
import re
import serial
import usb.core
import usb.util

RICEPROBE_VID = 0xFFFE
RICEPROBE_PID = 0xFFD1

# used by usb.util.find_descriptor to match an interface using its interface descriptor string
IFACE_FROM_STR_MATCH = lambda d, s : lambda i : usb.util.get_string(d, i.iInterface) == s
# used by usb.util.find_descriptor to find the first endpoint of a certain direction from an interface
EP_FROM_DIR_MATCH = lambda d : lambda e : usb.util.endpoint_direction(e.bEndpointAddress) == d

@pytest.fixture
def usb_device():
    dev = usb.core.find(idVendor=RICEPROBE_VID, idProduct=RICEPROBE_PID)
    yield dev
    dev.reset()

# makes sure that we can read all the expected values back from the device descriptor
def test_usb_descriptor(usb_device):
    # USB composite device
    assert(usb_device.bDeviceClass == 0xEF)
    assert(usb_device.bDeviceSubClass == 0x02)
    assert(usb_device.bDeviceProtocol == 0x01)

    # Only 1 supported configuration
    assert(usb_device.bNumConfigurations == 0x01)

    # String Descriptors
    assert(usb.util.get_string(usb_device, usb_device.iManufacturer) == 'Nick Kraus')
    assert(usb.util.get_string(usb_device, usb_device.iProduct) == 'RICEProbe')
    regex = re.compile(r'^RPB1-[23][0-9][0-5][0-9][0-9]{6}[0-9A-Z]$')
    assert(re.match(regex, usb.util.get_string(usb_device, usb_device.iSerialNumber)))

    cfg = usb_device.get_active_configuration()

    dap_intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice CMSIS-DAP v2'))
    assert(dap_intf is not None)
    # doesn't yet support the swo endpoint
    assert(dap_intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(dap_intf.bInterfaceClass == 0xFF)
    assert(dap_intf.bInterfaceSubClass == 0x00)
    assert(dap_intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    dap_out_ep = dap_intf.endpoints()[0]
    assert(dap_out_ep is not None)
    # bulk out endpoint
    assert((dap_out_ep.bEndpointAddress & 0x80 == 0) and (dap_out_ep.bmAttributes == 0x02))
    dap_in_ep = dap_intf.endpoints()[1]
    assert(dap_in_ep is not None)
    # bulk in endpoint
    assert((dap_in_ep.bEndpointAddress & 0x80 == 0x80) and (dap_in_ep.bmAttributes == 0x02))
    
    io_intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice I/O v1'))
    assert(io_intf is not None)
    assert(io_intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(io_intf.bInterfaceClass == 0xFF)
    assert(io_intf.bInterfaceSubClass == 0x00)
    assert(io_intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    io_out_ep = io_intf.endpoints()[0]
    assert(io_out_ep is not None)
    # bulk out endpoint
    assert((io_out_ep.bEndpointAddress & 0x80 == 0) and (io_out_ep.bmAttributes == 0x02))
    io_in_ep = io_intf.endpoints()[1]
    assert(io_in_ep is not None)
    # bulk in endpoint
    assert((io_in_ep.bEndpointAddress & 0x80 == 0x80) and (io_in_ep.bmAttributes == 0x02))

    vcp_comms_intf = usb.util.find_descriptor(cfg, bInterfaceClass=0x02, bInterfaceSubClass=0x02)
    assert(vcp_comms_intf is not None)
    assert(vcp_comms_intf.bNumEndpoints == 0x01)
    vcp_int_ep = vcp_comms_intf.endpoints()[0]
    assert(vcp_int_ep is not None)
    # in interrupt endpoint
    assert((vcp_int_ep.bEndpointAddress & 0x80 == 0x80) and (vcp_int_ep.bmAttributes == 0x03))
    
    vcp_data_intf = usb.util.find_descriptor(cfg, bInterfaceClass=0x0A, bInterfaceSubClass=0x02)
    assert(vcp_data_intf is not None)
    assert(vcp_data_intf.bNumEndpoints == 0x02)
    vcp_out_ep = usb.util.find_descriptor(
        vcp_data_intf,
        custom_match=EP_FROM_DIR_MATCH(usb.util.ENDPOINT_OUT)
    )
    assert(vcp_out_ep is not None)
    assert(vcp_out_ep.bmAttributes == 0x02)
    vcp_in_ep = usb.util.find_descriptor(
        vcp_data_intf,
        custom_match=EP_FROM_DIR_MATCH(usb.util.ENDPOINT_IN)
    )
    assert(vcp_in_ep is not None)
    assert(vcp_in_ep.bmAttributes == 0x02)

def test_usb_dap_write_read(usb_device):
    cfg = usb_device.get_active_configuration()
    dap_intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice CMSIS-DAP v2'))
    dap_out_ep = dap_intf.endpoints()[0]
    dap_in_ep = dap_intf.endpoints()[1]

    # unsupported writes should return the single hex byte 0xff
    dap_out_ep.write(b'\xf0')
    data = dap_in_ep.read(512)
    assert(len(data) == 1)
    assert(data.tobytes() == b'\xff')

    ##
    ## Info Command
    ##

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

def test_usb_io_write_read(usb_device):
    cfg = usb_device.get_active_configuration()
    io_intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice I/O v1'))
    io_out_ep = io_intf.endpoints()[0]
    io_in_ep = io_intf.endpoints()[1]

    io_out_ep.write(b'testing')
    data = io_in_ep.read(7)
    assert(data.tobytes() == b'testing')

def test_usb_vcp_loopback():
    # specifically use the pyserial interface to check the loopback, as this should ensure that all of actions an OS
    # takes while connecting to a CDC ACM device are also working correctly, not just raw data loopback
    ser = serial.serial_for_url(f'hwgrep://{RICEPROBE_VID:x}:{RICEPROBE_PID:x}', baudrate=115200, timeout=0.1)

    ser.write(b'testing')
    data = ser.read(7)
    assert(data == b'testing')
