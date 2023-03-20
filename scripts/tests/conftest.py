import os
import pytest
import usb.core

pytest.register_assert_rewrite('fixtures.dap')
pytest.register_assert_rewrite('fixtures.openocd')

from fixtures.dap import Dap
from fixtures.openocd import OpenOCD

RICEPROBE_VID = 0xFFFE
RICEPROBE_PID = 0xFFD1

@pytest.fixture(scope='class')
def usb_device():
    dev = usb.core.find(idVendor=RICEPROBE_VID, idProduct=RICEPROBE_PID)
    if dev is None:
        pytest.skip('test requires riceprobe usb device')

    yield dev
    dev.reset()

@pytest.fixture(scope='class')
def dap():
    dap = Dap(ip_addr=os.environ.get('RICEPROBE_IP'))
    yield dap
    dap.shutdown()

@pytest.fixture(scope='class', params=['jtag', 'swd'])
def openocd_rtt(request, usb_device):
    # openocd only supports the usb transport, including the device fixture will skip all tests
    # if usb probe isn't present
    usb_device.reset()

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
