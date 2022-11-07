import re
import usb.util

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
