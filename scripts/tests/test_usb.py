import re
import usb.util

# makes sure that we can read all the expected values back from the device descriptor
def test_usb_descriptor(
    usb_device,
    usb_dap_intf,
    usb_dap_eps,
    usb_io_intf,
    usb_io_eps,
    usb_vcp_intf,
    usb_vcp_eps
):
    print(usb_device)
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

    assert(usb_dap_intf is not None)
    # doesn't yet support the swo endpoint
    assert(usb_dap_intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(usb_dap_intf.bInterfaceClass == 0xFF)
    assert(usb_dap_intf.bInterfaceSubClass == 0x00)
    assert(usb_dap_intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    (dap_out_ep, dap_in_ep) = usb_dap_eps
    # bulk out endpoint
    assert(dap_out_ep is not None)
    assert((dap_out_ep.bEndpointAddress & 0x80 == 0) and (dap_out_ep.bmAttributes == 0x02))
    # bulk in endpoint
    assert(dap_in_ep is not None)
    assert((dap_in_ep.bEndpointAddress & 0x80 == 0x80) and (dap_in_ep.bmAttributes == 0x02))
    
    assert(usb_io_intf is not None)
    assert(usb_io_intf.bNumEndpoints == 0x02)
    # vendor specific device
    assert(usb_io_intf.bInterfaceClass == 0xFF)
    assert(usb_io_intf.bInterfaceSubClass == 0x00)
    assert(usb_io_intf.bInterfaceProtocol == 0x00)
    # endpoints must be configured in the correct order
    (io_out_ep, io_in_ep) = usb_io_eps
    # bulk out endpoint
    assert(io_out_ep is not None)
    assert((io_out_ep.bEndpointAddress & 0x80 == 0) and (io_out_ep.bmAttributes == 0x02))
    # bulk in endpoint
    assert(io_in_ep is not None)
    assert((io_in_ep.bEndpointAddress & 0x80 == 0x80) and (io_in_ep.bmAttributes == 0x02))

    (vcp_comm_intf, vcp_data_intf) = usb_vcp_intf
    (vcp_intp_ep, vcp_out_ep, vcp_in_ep) = usb_vcp_eps
    assert(vcp_comm_intf is not None)
    assert(vcp_comm_intf.bNumEndpoints == 0x01)
    # interrupt in endpoint
    assert(vcp_intp_ep is not None)
    assert((vcp_intp_ep.bEndpointAddress & 0x80 == 0x80) and (vcp_intp_ep.bmAttributes == 0x03))

    assert(vcp_data_intf is not None)
    assert(vcp_data_intf.bNumEndpoints == 0x02)
    # bulk out endpoint
    assert(vcp_out_ep is not None)
    assert(vcp_out_ep.bmAttributes == 0x02)
    # bulk in endpoint
    assert(vcp_in_ep is not None)
    assert(vcp_in_ep.bmAttributes == 0x02)
