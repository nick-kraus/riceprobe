import time
import usb.util

class IO:

    MAX_RESPONSE_LENGTH = 2048

    def __init__(self, usb_device):
        self.usb_device = usb_device
        cfg = self.usb_device.get_active_configuration()
        intf = usb.util.find_descriptor(
            cfg,
            custom_match=lambda i : usb.util.get_string(self.usb_device, i.iInterface) == "Rice I/O v1"
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

    def shutdown(self):
        self.usb_device.reset()
