import pytest
import serial

from conftest import RICEPROBE_VID, RICEPROBE_PID

def test_usb_vcp_loopback():
    # specifically use the pyserial interface to check the loopback, as this should ensure that all of actions an OS
    # takes while connecting to a CDC ACM device are also working correctly, not just raw data loopback
    ser = serial.serial_for_url(f'hwgrep://{RICEPROBE_VID:x}:{RICEPROBE_PID:x}', baudrate=115200, timeout=0.1)

    ser.write(b'testing')
    data = ser.read(7)
    assert(data == b'testing')
