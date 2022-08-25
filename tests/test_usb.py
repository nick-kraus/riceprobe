import pytest
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
    # USB Serial Number is taken from the hardware unique ID, we don't know much about all specific serial numbers,
    # but it should be serializable into a number and fairly large (at least as big as a uint32_t)
    serial_number = int(usb.util.get_string(usb_device, usb_device.iSerialNumber))
    assert(serial_number >= (2 ** 32) - 1)

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

    # first write / read may not actually work due to a zephyr driver bug, so don't panic on failure
    dap_out_ep.write(b'\x00')
    dap_in_ep.read(1)

    dap_out_ep.write(b'testing')
    data = dap_in_ep.read(7)
    assert(data.tobytes() == b'testing')

def test_usb_io_write_read(usb_device):
    cfg = usb_device.get_active_configuration()
    io_intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice I/O v1'))
    io_out_ep = io_intf.endpoints()[0]
    io_in_ep = io_intf.endpoints()[1]

    # first write / read may not actually work due to a zephyr driver bug, so don't panic on failure
    io_out_ep.write(b'\x00')
    io_in_ep.read(1)

    io_out_ep.write(b'testing')
    data = io_in_ep.read(7)
    assert(data.tobytes() == b'testing')

def test_usb_vcp_loopback():
    # specifically use the pyserial interface to check the loopback, as this should ensure that all of actions an OS
    # takes while connecting to a CDC ACM device are also working correctly, not just raw data loopback
    ser = serial.serial_for_url(f'hwgrep://{RICEPROBE_VID:x}:{RICEPROBE_PID:x}', baudrate=115200, timeout=0.1)

    # first write / read may not actually work due to a zephyr driver bug, so don't panic on failure
    ser.write(b'\x00')
    ser.read(1)

    ser.write(b'testing')
    data = ser.read(7)
    assert(data == b'testing')
