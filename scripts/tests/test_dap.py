import re
import time

class Dap:
    def __init__(self, endpoints):
        self.out_ep, self.in_ep = endpoints

    def write(self, data):
        return self.out_ep.write(data)

    def read(self, len, timeout=None):
        return self.in_ep.read(len, timeout).tobytes()

    def command(self, data, expect=None):
        self.write(data)
        read = self.read(512)
        if expect is not None:
            assert(read == expect)
        return read

    # reset the dap probe and target to a default known state for the jtag interface
    def jtag_default(self):
        # configure dap port as jtag
        self.command(b'\x02\x02', expect=b'\x02\x02')
        # set a reasonable clock rate (1MHz)
        self.command(b'\x11\x40\x42\x0f\x00', expect=b'\x11\x00')
        # reset target
        data = self.command(b'\x10\x00\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)
        time.sleep(0.01)
        data = self.command(b'\x10\x80\x80\xff\xff\x00\x00')
        assert(data[0] == 0x10)
        # set jtag tap state to reset then idle
        self.command(b'\x14\x02\x48\x00\x01\x00', expect=b'\x14\x00')        

def test_unsupported(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # unsupported writes should return the single hex byte 0xff
    dap.command(b'\xf0', expect=b'\xff')

def test_info_command(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete command request
    dap.command(b'\x00', expect=b'\xff')
    # vendor name from info should match USB vendor string
    dap.command(b'\x00\x01', expect=b'\x00\x0bNick Kraus\x00')
    # product name from info should match USB product string
    dap.command(b'\x00\x02', expect=b'\x00\x0aRICEProbe\x00')
    # serial number from info should match USB serial number
    data = dap.command(b'\x00\x03')
    assert(len(data) == 19 and data[0:2] == b'\x00\x11')
    assert(re.match(rb'^RPB1-[23][0-9][0-5][0-9][0-9]{6}[0-9A-Z]\x00$', data[2:]))
    # protocol version should match known string
    dap.command(b'\x00\x04', expect=b'\x00\x062.1.1\x00')
    # all of target device vendor, target device name, target board vendor, and
    # target board name should return an empty string
    dap.command(b'\x00\x05', expect=b'\x00\x00')
    dap.command(b'\x00\x06', expect=b'\x00\x00')
    dap.command(b'\x00\x07', expect=b'\x00\x00')
    dap.command(b'\x00\x08', expect=b'\x00\x00')
    # firmware version should match a known pattern
    data = dap.command(b'\x00\x09')
    assert(len(data) >= 20)
    assert(data[0] == ord(b'\x00') and data[1] == len(data) - 2)
    assert(re.match(rb'^v\d+\.\d+\.\d+-\d+-g[0-9a-f]{7}(-dirty)?\x00$', data[2:]))
    # capabilities should match a known value
    dap.command(b'\x00\xf0', expect=b'\x00\x02\x00\x01')
    # test domain timer should return the default unused value
    dap.command(b'\x00\xf1', expect=b'\x00\x08\x00\x00\x00\x00')
    # uart rx and tx buffer size should match a known value
    dap.command(b'\x00\xfb', expect=b'\x00\x04\x00\x04\x00\x00')
    dap.command(b'\x00\xfc', expect=b'\x00\x04\x00\x04\x00\x00')
    # swo trace buffer size should return 0 while unsupported
    dap.command(b'\x00\xfd', expect=b'\x00\x04\x00\x00\x00\x00')
    # usb packet count should match a known value
    dap.command(b'\x00\xfe', expect=b'\x00\x01\x02')
    # usb packet size should match a known value
    dap.command(b'\x00\xff', expect=b'\x00\x02\x00\x02')
    # unsupported info id
    dap.command(b'\x00\xbb', expect=b'\xff')

def test_host_status_command(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete command request
    dap.command(b'\x01\x00', expect=b'\xff')
    # enable connected led
    dap.command(b'\x01\x00\x01', expect=b'\x01\x00')
    # enable running led
    dap.command(b'\x01\x01\x01', expect=b'\x01\x00')
    # disable connected led
    dap.command(b'\x01\x00\x00', expect=b'\x01\x00')
    # disable running led
    dap.command(b'\x01\x01\x00', expect=b'\x01\x00')
    # make sure that an unsupported led type or status value produces an error
    dap.command(b'\x01\x02\x00', expect=b'\x01\xff')
    dap.command(b'\x01\x00\x02', expect=b'\x01\xff')

def test_delay_command(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete command request
    dap.command(b'\x09\xff', expect=b'\xff')
    start = time.time()
    # delay command with maximum microseconds (65535)
    dap.command(b'\x09\xff\xff', expect=b'\x09\x00')
    end = time.time()
    # not looking for much accuracy with such a small delay time, just something within reason
    delta = (end - start) * 1000000
    assert(delta > 30000 and delta < 100000)

def test_reset_target_command(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    dap.command(b'\x0a', expect=b'\x0a\x00\x00')

def test_disconnect_connect_swj_pins_commands(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete command requests
    dap.command(b'\x10', expect=b'\xff')
    dap.command(b'\x10\x00', expect=b'\xff')
    dap.command(b'\x10\x00\x00\xff\xff', expect=b'\xff')
    dap.jtag_default()
    # set all outputs low, make sure they read low, tdo will read high
    dap.command(b'\x10\x00\x8f\xff\xff\x00\x00', expect=b'\x10\x08')
    # set each output high one-by-one
    dap.command(b'\x10\x01\x01\xff\xff\x00\x00', expect=b'\x10\x09')
    dap.command(b'\x10\x02\x02\xff\xff\x00\x00', expect=b'\x10\x0b')
    dap.command(b'\x10\x04\x04\xff\xff\x00\x00', expect=b'\x10\x0f')
    # set all outputs low again, then exit reset
    dap.command(b'\x10\x00\x0f\xff\xff\x00\x00', expect=b'\x10\x08')
    dap.command(b'\x10\x80\x80\xff\xff\x00\x00', expect=b'\x10\x88')
    # ensure that configuring as 'default' sets jtag as the configuration, will set outputs high
    dap.command(b'\x02\x00', expect=b'\x02\x02')
    dap.command(b'\x10\x00\x00\xff\xff\x00\x00', expect=b'\x10\x8f')
    # disconnect dap; tms, tdo, tdi should go high
    dap.command(b'\x03', expect=b'\x03\x00')
    # make sure outputs can't change when disconnected
    dap.command(b'\x10\x00\x8f\xff\xff\x00\x00', expect=b'\x10\x8e')
    # reconfigure as swd, swclk, swdio, and reset should be high
    dap.command(b'\x02\x01', expect=b'\x02\x01')
    dap.command(b'\x10\x00\x00\xff\xff\x00\x00', expect=b'\x10\x8f')
    # should only be able to set swclk, swdio, and reset low
    dap.command(b'\x10\x00\x8f\xff\xff\x00\x00', expect=b'\x10\x0c')
    # finish reset and disconnect to go back to known state
    dap.command(b'\x10\x80\x80\xff\xff\x00\x00', expect=b'\x10\x8c')
    dap.command(b'\x03', expect=b'\x03\x00')

def test_swj_clock_sequence_commands(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete clock command request
    dap.command(b'\x11\x00\x00', expect=b'\xff')
    # incomplete sequence command request
    dap.command(b'\x12\x0a\xff', expect=b'\xff')
    # clock rate of 0 should produce an error
    dap.command(b'\x11\x00\x00\x00\x00', expect=b'\x11\xff')
    # anything else should succeed
    dap.command(b'\x11\x87\xd6\x12\x00', expect=b'\x11\x00')

    # read the JTAG idcode using the SWJ sequence command any time we are just changing tap states
    dap.jtag_default()
    # select-dr-scan, select-ir-scan, capture-ir, shift-ir
    dap.command(b'\x12\x04\x03', expect=b'\x12\x00')
    # shift the 4-bit idcode (0b1110)
    dap.command(b'\x14\x01\x04\x0e', expect=b'\x14\x00')
    # shift the first 4 bits of boundary scan tap bypass
    dap.command(b'\x14\x01\x04\x0f', expect=b'\x14\x00')
    # shift the last bit of bypass; exit-1-ir, update-ir
    dap.command(b'\x14\x01\x42\x01', expect=b'\x14\x00')
    # idle, select-dr-scan, capture-dr, shift-dr
    dap.command(b'\x12\x04\x02', expect=b'\x12\x00')
    # clock out the 32-bit idcode onto tdo
    dap.command(b'\x14\x01\xa0\x00\x00\x00\x00', expect=b'\x14\x00\x77\x04\xa0\x4b')
    # exit-1-dr, update-dr, idle
    dap.command(b'\x12\x03\x03', expect=b'\x12\x00')

def test_jtag_sequence_configure_idcode_command(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete command requests
    dap.command(b'\x14', expect=b'\xff')
    dap.command(b'\x14\x01\x42', expect=b'\xff')
    dap.command(b'\x14\x02\x42\x00\x02', expect=b'\xff')
    # attempt the jtag sequence command while configured as swd, should be an error
    dap.command(b'\x02\x01', expect=b'\x02\x01')
    dap.command(b'\x14\x03\x02\xaa\x09\xaa\xaa\x00\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa', expect=b'\x14\xff')
    # now for correct usage of the command
    dap.jtag_default()
    # here we set the jtag ir to the idcode instruction (0b1110), the target is an stm32l4r5zi
    # which has a boundary scan tap (5-bit ir) followed by a debug tap (4-bit ir)
    #
    # jtag state: select-dr-scan, select-ir-scan
    dap.command(b'\x14\x01\x42\x00', expect=b'\x14\x00')
    # jtag state: capture-ir, shift-ir
    dap.command(b'\x14\x01\x02\x00', expect=b'\x14\x00')
    # shift the 4-bit idcode (0b1110)
    dap.command(b'\x14\x01\x04\x0e', expect=b'\x14\x00')
    # shift the first 4 bits of boundary scan tap bypass
    dap.command(b'\x14\x01\x04\x0f', expect=b'\x14\x00')
    # shift the last bit of bypass; jtag state: exit-1-ir, update-ir
    dap.command(b'\x14\x01\x42\x01', expect=b'\x14\x00')
    # jtag state: idle
    dap.command(b'\x14\x01\x01\x00', expect=b'\x14\x00')
    # we now get the 32-bit idcode from the data register, which will be from the cortex-m4
    # core, r0p1 (0x4ba00477), and use a single sequence command
    #
    # jtag state: select-dr-scan
    sequence = b'\x41\x00'
    # jtag state: capture-dr, shift-dr
    sequence += b'\x02\x00'
    # clock out the 32-bit idcode onto tdo
    sequence += b'\xA0\x00\x00\x00\x00'
    # jtag state: exit-1-dr, update-dr
    sequence += b'\x42\x00'
    # jtag state: idle
    sequence += b'\x01\x00'
    # now send the 5-sequence sequence
    dap.command(b'\x14\x05' + sequence, expect=b'\x14\x00\x77\x04\xa0\x4b')

def test_jtag_configure_idcode_command(usb_dap_eps):
    dap = Dap(usb_dap_eps)
    # incomplete configure command requests
    dap.command(b'\x15\x05\x00', expect=b'\xff')
    dap.command(b'\x15\x02\x00', expect=b'\xff')
    # only support up to 4 devices in the jtag chain, so this should be an error
    dap.command(b'\x15\x05\x01\x01\x01\x01\x01', expect=b'\x15\xff')
    # incomplete idcode command request
    dap.command(b'\x16', expect=b'\xff')
    # now configure the tap chain for the actual target: stm32l4r5zi
    dap.jtag_default()
    dap.command(b'\x15\x02\x04\x05', expect=b'\x15\x00')
    # should fail with an invalid index, and when not configured as jtag
    dap.command(b'\x16\x08', expect=b'\x16\xff\x00\x00\x00\x00')
    dap.command(b'\x02\x01', expect=b'\x02\x01')
    dap.command(b'\x16\x00', expect=b'\x16\xff\x00\x00\x00\x00')
    dap.command(b'\x02\x02', expect=b'\x02\x02')
    # run the idcode command and make sure we get the same result
    dap.command(b'\x16\x00', expect=b'\x16\x00\x77\x04\xa0\x4b')

def test_jtag_transfer_commands(usb_dap_eps):
    dap = Dap(usb_dap_eps)

    # when port is disconnected or index invalid, transfer and transfer_block
    # should return no sequences and data
    dap.command(b'\x03', expect=b'\x03\x00')
    dap.command(b'\x05\x00\x01\x06', expect=b'\x05\x00\x00')
    dap.command(b'\x06\x00\x01\x00\x06', expect=b'\x06\x00\x00\x00')
    dap.command(b'\x02\x02', expect=b'\x02\x02')
    dap.command(b'\x05\x08\x01\x06', expect=b'\x05\x00\x00')
    dap.command(b'\x06\x08\x01\x00\x06', expect=b'\x06\x00\x00\x00')

    dap.jtag_default()
    # configure the jtag tap details
    dap.command(b'\x15\x02\x04\x05', expect=b'\x15\x00')

    # incomplete transfer configure command request
    dap.command(b'\x04\x00\x00', expect=b'\xff')
    # incomplete transfer command requests
    dap.command(b'\x05\x00\x01', expect=b'\xff')
    dap.command(b'\x05\x00\x03\x06\x06', expect=b'\xff')
    # incomplete transfer block command requests
    dap.command(b'\x06\x00\x01\x00', expect=b'\xff')

    # write to DP, CTRL/STAT register (0x4), clear register expected default state,
    # then read from DP, CTRL/STAT, to verify previous read, where most bits (but
    # not all) will be zeroed
    xfer_request = b'\x04\x00\x00\x00\x00' + b'\x06'
    xfer_response = b'\x00\x00\x00\x20'
    dap.command(b'\x05\x00\x02' + xfer_request, expect=b'\x05\x02\x01' + xfer_response)
    # now write CSYSPWRUPREQ and CDBGPWRUPREQ bits and clear STICKYERR from CTRL/STAT, then
    # make multiple reads to check read transfer pipelining
    xfer_request = b'\x04\x10\x00\x00\x50' + b'\x06' + b'\x06' + b'\x20\x00\x00\x00\x00'
    xfer_response = b'\x00\x00\x00\xf0' + b'\x00\x00\x00\xf0'
    dap.command(b'\x05\x00\x04' + xfer_request, expect=b'\x05\x04\x01' + xfer_response)

    # write to DP, SELECT register (0x8)
    # set APSEL to 0 and APBANKSEL to the AP register bank containing the ID register (0xf)
    dap.command(b'\x05\x00\x01\x08\xf0\x00\x00\x00', expect=b'\x05\x01\x01')
    # read from DP, SELECT register (0x8), verify previous write
    dap.command(b'\x05\x00\x01\x0a', expect=b'\x05\x01\x01\xf0\x00\x00\x00')

    # DAP AP 0 should be a MEM-AP for the given target (stm32l4r5zi), designed by ARM Limited,
    # with a JEP-106 continuation code of 0x4 and a JEP-106 identity code of 0x3b
    #
    # write match mask for continuation code
    xfer_request = b'\x20\x00\x00\x00\x0f'
    # read match value of continuation code
    xfer_request += b'\x1f\x00\x00\x00\x04'
    # write match mask for identity code
    xfer_request += b'\x20\x00\x00\xfe\x00'
    # read match value of identity code
    xfer_request += b'\x1f\x00\x00\x76\x00'
    dap.command(b'\x05\x00\x04' + xfer_request, expect=b'\x05\x04\x01')

    # verify a wrong value produces a value mismatch error
    dap.command(b'\x05\x00\x01\x1f\x00\x00\x76\x01', expect=b'\x05\x00\x11')
    # verify an error clears the remaining request bytes
    dap.command(b'\x05\x00\x03\x1f\x00\x00\x76\x01\x1f\x00\x00\x76\x00\x06', expect=b'\x05\x00\x11')
    dap.command(b'\x05\x00\x01\x1f\x00\x00\x76\x00', expect=b'\x05\x01\x01')

    # set APBANKSEL to the AP register bank containing the CSW, TAR, and RDW registers (0x0)
    xfer_request = b'\x08\x00\x00\x00\x00'
    # set AP #0 CSW to default useful value (from openocd arm_adi_v5.h), read back
    xfer_request += b'\x01\x12\x00\x00\x22' + b'\x03'
    xfer_response = b'\x52\x00\x00\x23'
    dap.command(b'\x05\x00\x03' + xfer_request, expect=b'\x05\x03\x01' + xfer_response)

    # write multiple words of data to the TAR register, then read them back multiple times,
    # making sure all reads return the same value
    dap.command(b'\x06\x00\x02\x00\x05\x12\x34\x45\x78\x12\x34\x45\x78', expect=b'\x06\x02\x00\x01')
    dap.command(b'\x06\x00\x02\x00\x07', expect=b'\x06\x02\x00\x01\x12\x34\x45\x78\x12\x34\x45\x78')

    # basic (no-op) check of the transfer abort command
    dap.write(b'\x07')
    data = dap.read(1, timeout=10)
    # command doesn't return anything
    assert(len(data) == 0)
