import re
import time

def test_unsupported(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # unsupported writes should return the single hex byte 0xff
    out_ep.write(b'\xf0')
    assert(in_ep.read(512).tobytes() == b'\xff')

def test_info_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # vendor name from info should match USB vendor string
    out_ep.write(b'\x00\x01')
    assert(in_ep.read(512).tobytes() == b'\x00\x0bNick Kraus\x00')
    # product name from info should match USB product string
    out_ep.write(b'\x00\x02')
    assert(in_ep.read(512).tobytes() == b'\x00\x0aRICEProbe\x00')
    # serial number from info should match USB serial number
    out_ep.write(b'\x00\x03')
    data = in_ep.read(512)
    assert(len(data) == 19)
    assert(data.tobytes()[0:2] == b'\x00\x11')
    assert(re.match(rb'^RPB1-[23][0-9][0-5][0-9][0-9]{6}[0-9A-Z]\x00$', data.tobytes()[2:]))

    # protocol version should match known string
    out_ep.write(b'\x00\x04')
    assert(in_ep.read(512).tobytes() == b'\x00\x062.1.1\x00')

    # all of target device vendor, target device name, target board vendor, and
    # target board name should return an empty string
    out_ep.write(b'\x00\x05')
    assert(in_ep.read(512).tobytes() == b'\x00\x00')
    out_ep.write(b'\x00\x06')
    assert(in_ep.read(512).tobytes() == b'\x00\x00')
    out_ep.write(b'\x00\x07')
    assert(in_ep.read(512).tobytes() == b'\x00\x00')
    out_ep.write(b'\x00\x08')
    assert(in_ep.read(512).tobytes() == b'\x00\x00')

    # firmware version should match a known pattern
    # assert(False)
    out_ep.write(b'\x00\x09')
    data = in_ep.read(512)
    assert(len(data) >= 20)
    assert(data.tobytes()[0] == ord(b'\x00'))
    assert(data.tobytes()[1] == len(data) - 2)
    assert(re.match(rb'^v\d+\.\d+\.\d+-\d+-g[0-9a-f]{7}(-dirty)?\x00$', data.tobytes()[2:]))

    # capabilities should match a known value
    out_ep.write(b'\x00\xf0')
    assert(in_ep.read(512).tobytes() == b'\x00\x02\x00\x01')
    # test domain timer should return the default unused value
    out_ep.write(b'\x00\xf1')
    assert(in_ep.read(512).tobytes() == b'\x00\x08\x00\x00\x00\x00')
    # uart rx and tx buffer size should match a known value
    out_ep.write(b'\x00\xfb')
    assert(in_ep.read(512).tobytes() == b'\x00\x04\x00\x04\x00\x00')
    out_ep.write(b'\x00\xfc')
    assert(in_ep.read(512).tobytes() == b'\x00\x04\x00\x04\x00\x00')
    # swo trace buffer size should return 0 while unsupported
    out_ep.write(b'\x00\xfd')
    assert(in_ep.read(512).tobytes() == b'\x00\x04\x00\x00\x00\x00')
    # usb packet count should match a known value
    out_ep.write(b'\x00\xfe')
    assert(in_ep.read(512).tobytes() == b'\x00\x01\x02')
    # usb packet size should match a known value
    out_ep.write(b'\x00\xff')
    assert(in_ep.read(512).tobytes() == b'\x00\x02\x00\x02')

    # unsupported info id
    out_ep.write(b'\x00\xbb')
    assert(in_ep.read(512).tobytes() == b'\xff')

def test_host_status_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # enable connected led
    out_ep.write(b'\x01\x00\x01')
    assert(in_ep.read(512).tobytes() == b'\x01\x00')
    # enable running led
    out_ep.write(b'\x01\x01\x01')
    assert(in_ep.read(512).tobytes() == b'\x01\x00')
    # disable running led
    out_ep.write(b'\x01\x01\x00')
    assert(in_ep.read(512).tobytes() == b'\x01\x00')
    # disable connected led
    out_ep.write(b'\x01\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x01\x00')

    # make sure that an unsupported led type or status value produces an error
    out_ep.write(b'\x01\x02\x00')
    assert(in_ep.read(512).tobytes() == b'\x01\xff')
    out_ep.write(b'\x01\x00\x02')
    assert(in_ep.read(512).tobytes() == b'\x01\xff')

def test_delay_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    delay = 65535
    command = bytearray(b'\x09\x00\x00')
    command[1:3] = delay.to_bytes(2, byteorder='little')

    start = time.time()
    out_ep.write(command)
    assert(in_ep.read(512).tobytes() == b'\x09\x00')
    end = time.time()

    # not looking for much accuracy with such a small delay time, just something within reason
    delta = (end - start) * 1000000
    assert(delta > (0.5 * delay) and delta < (1.5 * delay))

def test_reset_target_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    out_ep.write(b'\x0a')
    assert(in_ep.read(512).tobytes() == b'\x0a\x00\x00')

def test_disconnect_connect_swj_pins_commands(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps
    
    # put the target into reset, to start from a known state and not mess with its TAP
    # need to configure swj before performing the reset, configure as jtag
    out_ep.write(b'\x02\x02')
    assert(in_ep.read(512).tobytes() == b'\x02\x02')
    out_ep.write(b'\x10\x00\x80\xff\xff\x00\x00')
    data = in_ep.read(512).tobytes()
    # don't presume to know the state of all the other pins, but make sure it is reset
    assert(data[0] == 0x10)
    assert(data[1] & 0x80 == 0x00)

    # set all outputs low, make sure they read low, tdo will read high
    out_ep.write(b'\x10\x00\x0f\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x08')

    # set each output high one-by-one
    out_ep.write(b'\x10\x01\x01\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x09')
    out_ep.write(b'\x10\x02\x02\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x0b')
    out_ep.write(b'\x10\x04\x04\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x0f')

    # set all outputs low again, then exit reset
    out_ep.write(b'\x10\x00\x0f\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x08')
    out_ep.write(b'\x10\x80\x80\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x88')

    # ensure that configuring as 'default' sets jtag as the configuration, will set outputs high
    out_ep.write(b'\x02\x00')
    assert(in_ep.read(512).tobytes() == b'\x02\x02')
    out_ep.write(b'\x10\x00\x00\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x8f')

    # disconnect dap; tms, tdo, tdi should go high
    out_ep.write(b'\x03')
    assert(in_ep.read(512).tobytes() == b'\x03\x00')
    # make sure outputs can't change when disconnected
    out_ep.write(b'\x10\x00\x8f\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x8e')

    # reconfigure as swd, swclk, swdio, and reset should be high
    out_ep.write(b'\x02\x01')
    assert(in_ep.read(512).tobytes() == b'\x02\x01')
    out_ep.write(b'\x10\x00\x00\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x8f')

    # should only be able to set swclk, swdio, and reset low
    out_ep.write(b'\x10\x00\x8f\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x0c')

    # finish reset and disconnect to go back to known state
    out_ep.write(b'\x10\x80\x80\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x10\x8c')
    out_ep.write(b'\x03')
    assert(in_ep.read(512).tobytes() == b'\x03\x00')

def test_swj_clock_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # clock rate of 0 should produce an error
    out_ep.write(b'\x11\x00\x00\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x11\xff')

    # anything else should succeed
    out_ep.write(b'\x11\x87\xd6\x12\x00')
    assert(in_ep.read(512).tobytes() == b'\x11\x00')

def test_jtag_sequence_configure_idcode_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # configure dap port as jtag
    out_ep.write(b'\x02\x02')
    assert(in_ep.read(512).tobytes() == b'\x02\x02')

    # set a reasonable clock rate (about 1MHz)
    out_ep.write(b'\x11\x00\x0a\x0f\x00')
    assert(in_ep.read(512).tobytes() == b'\x11\x00')

    # jtag state: test-logic-reset
    out_ep.write(b'\x14\x01\x45\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # jtag state: run-test-idle
    out_ep.write(b'\x14\x01\x01\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')

    # here we set the jtag ir to the idcode instruction (0b1110), the target is an stm32l4r5zi
    # which has a boundary scan tap (5-bit ir) followed by a debug tap (4-bit ir)

    # jtag state: select-dr-scan, select-ir-scan
    out_ep.write(b'\x14\x01\x42\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # jtag state: capture-ir, shift-ir
    out_ep.write(b'\x14\x01\x02\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # shift the 4-bit idcode (0b1110)
    out_ep.write(b'\x14\x01\x04\x0e')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # shift the first 4 bits of boundary scan tap bypass
    out_ep.write(b'\x14\x01\x04\x0f')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # shift the last bit of bypass; jtag state: exit-1-ir, update-ir
    out_ep.write(b'\x14\x01\x42\x01')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # jtag state: idle
    out_ep.write(b'\x14\x01\x01\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')

    # we now get the 32-bit idcode from the data register, which will be from the cortex-m4
    # core, r0p1 (0x4ba00477)

    # jtag state: select-dr-scan
    out_ep.write(b'\x14\x01\x41\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # jtag state: capture-dr, shift-dr
    out_ep.write(b'\x14\x01\x02\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # clock out the 32-bit idcode onto tdo
    out_ep.write(b'\x14\x01\xA0\x00\x00\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00\x77\x04\xa0\x4b')
    # jtag state: exit-1-dr, update-dr
    out_ep.write(b'\x14\x01\x42\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')
    # jtag state: idle
    out_ep.write(b'\x14\x01\x01\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')

    # configure the jtag tap details
    out_ep.write(b'\x15\x02\x04\x05')
    assert(in_ep.read(512).tobytes() == b'\x15\x00')

    # now configure run the idcode command and make sure we get the same result
    out_ep.write(b'\x16\x00')
    assert(in_ep.read(512).tobytes() == b'\x16\x00\x77\x04\xa0\x4b')

def test_jtag_transfer_configure_command(usb_dap_eps):
    (out_ep, in_ep) = usb_dap_eps

    # configure dap port as jtag, and jtag tag details
    out_ep.write(b'\x02\x02')
    assert(in_ep.read(512).tobytes() == b'\x02\x02')
    # configure the jtag tap details
    out_ep.write(b'\x15\x02\x04\x05')
    assert(in_ep.read(512).tobytes() == b'\x15\x00')

    # reset target
    out_ep.write(b'\x10\x00\x80\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes()[0] == 0x10)
    time.sleep(0.1)
    out_ep.write(b'\x10\x80\x80\xff\xff\x00\x00')
    assert(in_ep.read(512).tobytes()[0] == 0x10)

    # set a reasonable clock rate (about 1MHz), then reset jtag state to idle
    out_ep.write(b'\x11\x00\x0a\x0f\x00')
    assert(in_ep.read(512).tobytes() == b'\x11\x00')
    out_ep.write(b'\x14\x02\x48\x00\x01\x00')
    assert(in_ep.read(512).tobytes() == b'\x14\x00')

    # write to DP, CTRL/STAT register (0x4)
    # clear the register fully to return to an expected default state
    out_ep.write(b'\x05\x00\x01\x04\x00\x00\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01')
    # read from DP, CTRL/STAT register (0x4), verify previous write
    out_ep.write(b'\x05\x00\x01\x06')
    # the value never fully clears for the given target, but most bits do
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01\x00\x00\x00\x20')
    # write to DP, CTRL/STAT register (0x4)
    # set the CSYSPWRUPREQ and CDBGPWRUPREQ bits to enable debug power-up, and clear
    # the STICKYERR flag in case is was previously set
    out_ep.write(b'\x05\x00\x01\x04\x10\x00\x00\x50')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01')
    # read from DP, CTRL/STAT register (0x4), verify previous write
    out_ep.write(b'\x05\x00\x01\x06')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01\x00\x00\x00\xf0')

    # write to DP, SELECT register (0x8)
    # set APSEL to 0 and APBANKSEL to the AP register bank containing the ID register (0xf)
    out_ep.write(b'\x05\x00\x01\x08\xf0\x00\x00\x00')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01')
    # read from DP, SELECT register (0x8), verify previous write
    out_ep.write(b'\x05\x00\x01\x0a')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01\xf0\x00\x00\x00')

    # DAP AP 0 should be a MEM-AP for the given target, designed by ARM Limited, with a 
    # JEP-106 identity code of 0x3b

    # write match mask, for JEP-106 identity code bits
    out_ep.write(b'\x05\x00\x01\x20\x00\x00\xfe\x00')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01')
    # read match value from AP, ID register (0xc)
    out_ep.write(b'\x05\x00\x01\x1f\x00\x00\x76\x00')
    assert(in_ep.read(512).tobytes() == b'\x05\x01\x01')

    # read match value from AP, ID register (0xc)
    # wrong value on purpose, to verify a value mismatch error is created
    out_ep.write(b'\x05\x00\x01\x1f\x00\x00\x76\x01')
    assert(in_ep.read(512).tobytes() == b'\x05\x00\x11')
