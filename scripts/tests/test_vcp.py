import serial

from conftest import RICEPROBE_VID, RICEPROBE_PID

EDBG_VID = 0x03EB
EDBG_PID = 0x2111

def test_usb_vcp_loopback():
    # use the embedded debugger VCP as the interface to the RICEProbe VCP
    edbg_ser = serial.serial_for_url(f'hwgrep://{EDBG_VID:x}:{EDBG_PID:x}', baudrate=115200, timeout=0.1)
    # using pyserial will ensure that all of actions an OS takes while connecting to a CDC ACM device are also working
    rice_ser = serial.serial_for_url(f'hwgrep://{RICEPROBE_VID:x}:{RICEPROBE_PID:x}', baudrate=115200, timeout=0.1)

    # riceprobe write
    rice_ser.write(b'123456')
    data = edbg_ser.read(7)
    assert(data == b'123456')

    # riceprobe read
    edbg_ser.write(b'abcdef')
    data = rice_ser.read(7)
    assert(data == b'abcdef')
