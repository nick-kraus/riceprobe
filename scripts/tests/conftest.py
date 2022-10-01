import pytest
import usb.core
import usb.util

RICEPROBE_VID = 0xFFFE
RICEPROBE_PID = 0xFFD1

# used by usb.util.find_descriptor to match an interface using its interface descriptor string
IFACE_FROM_STR_MATCH = lambda d, s : lambda i : usb.util.get_string(d, i.iInterface) == s
# used by usb.util.find_descriptor to find the first endpoint of a certain direction from an interface
EP_FROM_DIR_MATCH = lambda d : lambda e : usb.util.endpoint_direction(e.bEndpointAddress) == d

@pytest.fixture(scope="module")
def usb_device():
    dev = usb.core.find(idVendor=RICEPROBE_VID, idProduct=RICEPROBE_PID)
    yield dev
    dev.reset()

@pytest.fixture(scope="module")
def usb_dap_intf(usb_device):
    cfg = usb_device.get_active_configuration()
    intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice CMSIS-DAP v2'))
    yield intf

@pytest.fixture(scope="module")
def usb_dap_eps(usb_dap_intf):
    yield (usb_dap_intf.endpoints()[0], usb_dap_intf.endpoints()[1])

@pytest.fixture(scope="module")
def usb_io_intf(usb_device):
    cfg = usb_device.get_active_configuration()
    intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice I/O v1'))
    yield intf

@pytest.fixture(scope="module")
def usb_io_eps(usb_io_intf):
    yield (usb_io_intf.endpoints()[0], usb_io_intf.endpoints()[1])

@pytest.fixture(scope="module")
def usb_vcp_intf(usb_device):
    cfg = usb_device.get_active_configuration()
    comm_intf = usb.util.find_descriptor(cfg, bInterfaceClass=0x02, bInterfaceSubClass=0x02)
    data_intf = usb.util.find_descriptor(cfg, bInterfaceClass=0x0A, bInterfaceSubClass=0x02)
    yield (comm_intf, data_intf)

@pytest.fixture(scope="module")
def usb_vcp_eps(usb_vcp_intf):
    (comm_intf, data_intf) = usb_vcp_intf
    intp_ep = comm_intf.endpoints()[0]
    out_ep = usb.util.find_descriptor(data_intf, custom_match=EP_FROM_DIR_MATCH(usb.util.ENDPOINT_OUT))
    in_ep = usb.util.find_descriptor(data_intf, custom_match=EP_FROM_DIR_MATCH(usb.util.ENDPOINT_IN))
    yield (intp_ep, out_ep, in_ep)
