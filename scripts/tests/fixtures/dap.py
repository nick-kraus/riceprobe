import socket
import time
import usb.util

from usb.core import USBTimeoutError

class DapTimeoutError(Exception):
    """Raised when a read operation timeout occurs."""

class Dap:    

    USB_VID = 0xFFFE
    USB_PID = 0xFFD1

    TCP_PORT = 30047

    MAX_RESPONSE_LENGTH = 2048

    def __init__(self, serial=None, usb_device=None, ip_addr=None):
        # usb connections are preferred, since the riceprobe won't search for tcp connections when usb is configured
        self.usb_device = usb_device if usb_device else self._find_usb_device(serial)

        if self.usb_device:
            self.transport = 'usb'
            cfg = self.usb_device.get_active_configuration()
            intf = usb.util.find_descriptor(
                cfg,
                custom_match=lambda i : usb.util.get_string(self.usb_device, i.iInterface) == 'Rice CMSIS-DAP v2'
            )
            self.out_ep, self.in_ep = intf.endpoints()
            return

        # if no usb device is configured, try to open up a tcp connection.
        # 
        # TODO: for now just use the given IP or throw an error, the python zeroconf libraries seem to have issues and 
        # can only find device local mdns services on windows.
        if not ip_addr:
            raise ValueError('mDNS not currently supported, IP address must be provided for tcp connections')

        self.transport = 'tcp'
        self.sock = socket.create_connection((ip_addr, self.TCP_PORT), timeout=5.0)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    def _find_usb_device(self, serial):
        if serial:
            dev = usb.core.find(custom_match=lambda d: d.serial_number == serial)
            if dev.idVendor != self.USB_VID or dev.idProduct != self.USB_PID:
                raise ValueError(f"device with serial {serial} does not have expected USB VID:PID")
        else:
            dev = usb.core.find(idVendor=self.USB_VID, idProduct=self.USB_PID)
        return dev

    def write(self, data):
        if self.transport == 'usb':
            return self.out_ep.write(data)
        elif self.transport == 'tcp':
            return self.sock.send(len(data).to_bytes(2, 'little') + data)

    def read(self, len):
        if self.transport == 'usb':
            try:
                received = self.in_ep.read(len, 500).tobytes()
            except USBTimeoutError:
                raise DapTimeoutError
            return received

        elif self.transport == 'tcp':
            try:
                read_len = int.from_bytes(self.sock.recv(2), 'little')
                if read_len > 0:
                    received = self.sock.recv(min(len, read_len))
                else:
                    received = b''
            except socket.timeout:
                raise DapTimeoutError
            return received
        
    def command(self, data, expect=None):
        self.write(data)
        read = self.read(self.MAX_RESPONSE_LENGTH + 1)
        if expect is not None:
            assert(read == expect)
        return read

    def configure_jtag(self):
        # set a reasonable clock rate (1MHz)
        self.command(b'\x11\x40\x42\x0f\x00', expect=b'\x11\x00')
        # configure dap port as jtag
        self.command(b'\x02\x02', expect=b'\x02\x02')
        # reset target
        data = self.command(b'\x10\x00\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)
        time.sleep(0.01)
        data = self.command(b'\x10\x80\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)

        # ensure both SWD and JTAG in reset states
        self.command(b'\x12\x38\xff\xff\xff\xff\xff\xff\xff', expect=b'\x12\x00')
        # issue JTAG-to-SWD sequence
        self.command(b'\x12\x10\x3c\xe7', expect=b'\x12\x00')
        # jtag reset
        self.command(b'\x12\x08\xff', expect=b'\x12\x00')
        # set jtag tap state to reset then idle
        self.command(b'\x14\x02\x48\x00\x01\x00', expect=b'\x14\x00')

    def configure_swd(self):
        # set a reasonable clock rate (1MHz)
        self.command(b'\x11\x40\x42\x0f\x00', expect=b'\x11\x00')
        # configure dap port as swd
        self.command(b'\x02\x01', expect=b'\x02\x01')
        # reset target
        data = self.command(b'\x10\x00\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)
        time.sleep(0.01)
        data = self.command(b'\x10\x80\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)

        # ensure both SWD and JTAG in reset states
        self.command(b'\x12\x38\xff\xff\xff\xff\xff\xff\xff', expect=b'\x12\x00')
        # issue JTAG-to-SWD sequence
        self.command(b'\x12\x10\x9e\xe7', expect=b'\x12\x00')
        # SWD reset
        self.command(b'\x12\x38\xff\xff\xff\xff\xff\xff\xff', expect=b'\x12\x00')
        # at least 2 idle cycles
        self.command(b'\x12\x08\x00', expect=b'\x12\x00')

    def shutdown(self):
        # ensure both SWD and JTAG in reset states
        self.command(b'\x12\x38\xff\xff\xff\xff\xff\xff\xff', expect=b'\x12\x00')
        # reset target
        data = self.command(b'\x10\x00\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)
        time.sleep(0.01)
        data = self.command(b'\x10\x80\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)
        # disconnect probe from target
        self.command(b'\x03', expect=b'\x03\x00')
        if self.transport == 'usb':
            self.usb_device.reset()
        elif self.transport == 'tcp':
            self.sock.close()
