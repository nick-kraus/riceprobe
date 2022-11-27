import time
import usb.util

class Dap:    

    MAX_RESPONSE_LENGTH = 2048

    def __init__(self, usb_device):
        self.usb_device = usb_device
        cfg = self.usb_device.get_active_configuration()
        intf = usb.util.find_descriptor(
            cfg,
            custom_match=lambda i : usb.util.get_string(self.usb_device, i.iInterface) == "Rice CMSIS-DAP v2"
        )
        self.out_ep, self.in_ep = intf.endpoints()

    def write(self, data):
        return self.out_ep.write(data)

    def read(self, len, timeout=None):
        return self.in_ep.read(len, timeout).tobytes()

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
        self.usb_device.reset()
