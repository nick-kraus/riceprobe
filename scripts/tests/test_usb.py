import re
import usb.util

from fixtures.dap import Dap

def test_device_descriptor(usb_device):
    # USB composite device
    assert(usb_device.bDeviceClass == 0xEF)
    assert(usb_device.bDeviceSubClass == 0x02)
    assert(usb_device.bDeviceProtocol == 0x01)

    # Only 1 supported configuration
    assert(usb_device.bNumConfigurations == 0x01)

    # String Descriptors
    assert(usb.util.get_string(usb_device, usb_device.iManufacturer) == 'Nick Kraus')
    assert(usb.util.get_string(usb_device, usb_device.iProduct) == 'RICEProbe IO CMSIS-DAP')
    regex = re.compile(r'^RPB1-[23][0-9][0-5][0-9][0-9]{6}[0-9A-Z]$')
    assert(re.match(regex, usb.util.get_string(usb_device, usb_device.iSerialNumber)))

def test_dap_interface_descirptor(usb_device):
    intf = usb.util.find_descriptor(
        usb_device.get_active_configuration(),
        custom_match=lambda i : usb.util.get_string(usb_device, i.iInterface) == 'Rice CMSIS-DAP v2'
    )

    assert(intf is not None)
    # doesn't yet support the swo endpoint
    assert(intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(intf.bInterfaceClass == 0xFF)
    assert(intf.bInterfaceSubClass == 0x00)
    assert(intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    (out_ep, in_ep) = intf.endpoints()
    # bulk out endpoint
    assert(out_ep is not None)
    assert((out_ep.bEndpointAddress & 0x80 == 0) and (out_ep.bmAttributes == 0x02))
    # bulk in endpoint
    assert(in_ep is not None)
    assert((in_ep.bEndpointAddress & 0x80 == 0x80) and (in_ep.bmAttributes == 0x02))

def test_io_interface_descirptor(usb_device):
    intf = usb.util.find_descriptor(
        usb_device.get_active_configuration(),
        custom_match=lambda i : usb.util.get_string(usb_device, i.iInterface) == 'Rice I/O v1'
    )

    assert(intf is not None)
    assert(intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(intf.bInterfaceClass == 0xFF)
    assert(intf.bInterfaceSubClass == 0x00)
    assert(intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    (out_ep, in_ep) = intf.endpoints()
    # bulk out endpoint
    assert(out_ep is not None)
    assert((out_ep.bEndpointAddress & 0x80 == 0) and (out_ep.bmAttributes == 0x02))
    # bulk in endpoint
    assert(in_ep is not None)
    assert((in_ep.bEndpointAddress & 0x80 == 0x80) and (in_ep.bmAttributes == 0x02))

def test_vcp_interface_descriptor(usb_device):
    comm_intf = usb.util.find_descriptor(
        usb_device.get_active_configuration(),
        bInterfaceClass=0x02,
        bInterfaceSubClass=0x02
    )
    data_intf = usb.util.find_descriptor(
        usb_device.get_active_configuration(),
        bInterfaceClass=0x0A,
        bInterfaceSubClass=0x02
    )

    assert(comm_intf is not None)
    assert(comm_intf.bNumEndpoints == 0x01)
    assert(data_intf is not None)
    assert(data_intf.bNumEndpoints == 0x02)

    intp_ep = comm_intf.endpoints()[0]
    out_ep = usb.util.find_descriptor(
        data_intf,
        custom_match=lambda e : usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
    )
    in_ep = usb.util.find_descriptor(
        data_intf,
        custom_match=lambda e : usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN
    )

    # interrupt in endpoint
    assert(intp_ep is not None)
    assert((intp_ep.bEndpointAddress & 0x80 == 0x80) and (intp_ep.bmAttributes == 0x03))
    # bulk out endpoint
    assert(out_ep is not None)
    assert(out_ep.bmAttributes == 0x02)
    # bulk in endpoint
    assert(in_ep is not None)
    assert(in_ep.bmAttributes == 0x02)

def test_dap_interface_transfer_sizes(usb_device):
    # depending on the usb_device fixture here instead of dap directly means this will be skipped
    # if we are running on the tcp transport
    dap = Dap(usb_device=usb_device)

    # send a command with a length exactly 512-bytes (high-speed USB max)
    command = b'\x7f\xab' + b'\x09\x00\x00' * 168 + b'\x00\xff' * 3
    response = b'\x7f\xab' + b'\x09\x00' * 168 + b'\x00\x02\x00\x02' * 3
    assert(len(command) == 512)
    dap.command(command, expect=response)

    # send a command with a response exactly 512-bytes
    command = b'\x7f\x80' + b'\x00\xff' * 127 + b'\x00\x05'
    response = b'\x7f\x80' + b'\x00\x02\x00\x02' * 127 + b'\x00\x00'
    dap.write(command)
    data = dap.read(2049)
    assert(len(data) == 512)
    assert(data == response)

    # send a chain of commands which uses the full 2048 byte buffer size
    command = b'\x7e\xab' + b'\x09\x00\x00' * 168 + b'\x00\xff' * 3
    for i in range(3):
        dap.write(command)
    command = b'\x7f\xab' + b'\x09\x00\x00' * 168 + b'\x00\xff' * 3
    dap.write(command)
    response = (b'\x7f\xab' + b'\x09\x00' * 168 + b'\x00\x02\x00\x02' * 3) * 4
    data = dap.read(2049)
    assert(data == response)

    # send a chain of commands which has a response of the full 2048 byte buffer size
    command = b'\x7e\x80' + b'\x00\xff' * 127 + b'\x00\x05'
    for i in range(3):
        dap.write(command)
    command = b'\x7f\x80' + b'\x00\xff' * 127 + b'\x00\x05'
    dap.write(command)
    response = (b'\x7f\x80' + b'\x00\x02\x00\x02' * 127 + b'\x00\x00')  * 4
    data = dap.read(2049)
    assert(len(data) == 2048)
    assert(data == response)

    # at the end of all this, make sure a regular command still works
    dap.command(b'\x00\xff', expect=b'\x00\x02\x00\x02')
