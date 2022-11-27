import pytest
import usb.core

pytest.register_assert_rewrite('fixtures.dap')
pytest.register_assert_rewrite('fixtures.openocd')

from fixtures.dap import Dap
from fixtures.openocd import OpenOCD

RICEPROBE_VID = 0xFFFE
RICEPROBE_PID = 0xFFD1

@pytest.fixture(scope='module')
def usb_device():
    dev = usb.core.find(idVendor=RICEPROBE_VID, idProduct=RICEPROBE_PID)
    yield dev
    dev.reset()

@pytest.fixture(scope='module')
def dap(usb_device):
    dap = Dap(usb_device)
    yield dap
    dap.shutdown()

@pytest.fixture(scope='module', params=['jtag', 'swd'])
def openocd_rtt(request):
    transport = request.param
    with OpenOCD(transport=transport) as openocd:
        # ensure the target is reset, initialized, and running, since we connected under reset
        openocd.send(b'reset run')

        rtt = openocd.enable_rtt()
        # make sure we can send and receive data from the shell
        assert(rtt.send(b'\n\n') == 2)
        assert(rtt.expect_bytes(b'target:~$ ') is not None)
        assert(rtt.expect_bytes(b'target:~$ ') is not None)

        yield openocd, rtt
