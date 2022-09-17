import pytest
import re
import usb.util

# used by usb.util.find_descriptor to find the first endpoint of a certain direction from an interface
EP_FROM_DIR_MATCH = lambda d : lambda e : usb.util.endpoint_direction(e.bEndpointAddress) == d

# makes sure that we can read all the expected values back from the device descriptor
def test_usb_descriptor(usb_device, usb_dap_intf, usb_io_intf, usb_vcp_intf):
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

    assert(usb_dap_intf is not None)
    # doesn't yet support the swo endpoint
    assert(usb_dap_intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(usb_dap_intf.bInterfaceClass == 0xFF)
    assert(usb_dap_intf.bInterfaceSubClass == 0x00)
    assert(usb_dap_intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    dap_out_ep = usb_dap_intf.endpoints()[0]
    assert(dap_out_ep is not None)
    # bulk out endpoint
    assert((dap_out_ep.bEndpointAddress & 0x80 == 0) and (dap_out_ep.bmAttributes == 0x02))
    dap_in_ep = usb_dap_intf.endpoints()[1]
    assert(dap_in_ep is not None)
    # bulk in endpoint
    assert((dap_in_ep.bEndpointAddress & 0x80 == 0x80) and (dap_in_ep.bmAttributes == 0x02))
    
    assert(usb_io_intf is not None)
    assert(usb_io_intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(usb_io_intf.bInterfaceClass == 0xFF)
    assert(usb_io_intf.bInterfaceSubClass == 0x00)
    assert(usb_io_intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    io_out_ep = usb_io_intf.endpoints()[0]
    assert(io_out_ep is not None)
    # bulk out endpoint
    assert((io_out_ep.bEndpointAddress & 0x80 == 0) and (io_out_ep.bmAttributes == 0x02))
    io_in_ep = usb_io_intf.endpoints()[1]
    assert(io_in_ep is not None)
    # bulk in endpoint
    assert((io_in_ep.bEndpointAddress & 0x80 == 0x80) and (io_in_ep.bmAttributes == 0x02))

    assert(usb_vcp_intf is not None)
    assert(usb_vcp_intf.bNumEndpoints == 0x01)
    vcp_int_ep = usb_vcp_intf.endpoints()[0]
    assert(vcp_int_ep is not None)
    # in interrupt endpoint
    assert((vcp_int_ep.bEndpointAddress & 0x80 == 0x80) and (vcp_int_ep.bmAttributes == 0x03))
    
    cfg = usb_device.get_active_configuration()
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
