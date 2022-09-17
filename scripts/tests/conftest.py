import pytest
import usb.core
import usb.util

RICEPROBE_VID = 0xFFFE
RICEPROBE_PID = 0xFFD1

# used by usb.util.find_descriptor to match an interface using its interface descriptor string
IFACE_FROM_STR_MATCH = lambda d, s : lambda i : usb.util.get_string(d, i.iInterface) == s

@pytest.fixture
def usb_device():
    dev = usb.core.find(idVendor=RICEPROBE_VID, idProduct=RICEPROBE_PID)
    yield dev
    dev.reset()

@pytest.fixture
def usb_dap_intf(usb_device):
    cfg = usb_device.get_active_configuration()
    intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice CMSIS-DAP v2'))
    yield intf

@pytest.fixture
def usb_io_intf(usb_device):
    cfg = usb_device.get_active_configuration()
    intf = usb.util.find_descriptor(cfg, custom_match=IFACE_FROM_STR_MATCH(usb_device, 'Rice I/O v1'))
    yield intf

@pytest.fixture
def usb_vcp_intf(usb_device):
    cfg = usb_device.get_active_configuration()
    intf = usb.util.find_descriptor(cfg, bInterfaceClass=0x02, bInterfaceSubClass=0x02)
    yield intf
