import pytest
import re
import time

from fixtures.dap import DapTimeoutError

class TestMisc:
    def test_unsupported(self, dap):
        # unsupported writes should return the single hex byte 0xff
        dap.command(b'\xf0', expect=b'\xff')

    def test_info_command(self, dap):
        # incomplete command request
        dap.command(b'\x00', expect=b'\xff')
        # vendor name from info should match USB vendor string
        dap.command(b'\x00\x01', expect=b'\x00\x0bNick Kraus\x00')
        # product name from info should match USB product string
        dap.command(b'\x00\x02', expect=b'\x00\x17RICEProbe IO CMSIS-DAP\x00')
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
        dap.command(b'\x00\xf0', expect=b'\x00\x01\x13')
        # test domain timer should return the default unused value
        dap.command(b'\x00\xf1', expect=b'\x00\x08\x00\x00\x00\x00')
        # uart rx and tx buffer size should match a known value
        dap.command(b'\x00\xfb', expect=b'\x00\x04\x00\x04\x00\x00')
        dap.command(b'\x00\xfc', expect=b'\x00\x04\x00\x04\x00\x00')
        # swo trace buffer size should match a known value
        dap.command(b'\x00\xfd', expect=b'\x00\x04\x00\x08\x00\x00')
        # usb packet count should match a known value
        dap.command(b'\x00\xfe', expect=b'\x00\x01\x04')
        # usb packet size should match a known value
        dap.command(b'\x00\xff', expect=b'\x00\x02\x00\x02')
        # unsupported info id returns length of 0
        dap.command(b'\x00\xbb', expect=b'\x00\x00')

    def test_host_status_command(self, dap):
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

    def test_delay_command(self, dap):
        # incomplete command request
        dap.command(b'\x09\xff', expect=b'\xff')
        start = time.time()
        # delay command with maximum microseconds (65535)
        dap.command(b'\x09\xff\xff', expect=b'\x09\x00')
        end = time.time()
        # should have taken at least 65535 microseconds, with some wiggle room
        delta = (end - start) * 1000000
        assert(delta > 55000)

    def test_reset_target_command(self, dap):
        dap.command(b'\x0a', expect=b'\x0a\x00\x00')

    def test_disconnect_connect_swj_pins_commands(self, dap):
        # incomplete command requests
        dap.command(b'\x10', expect=b'\xff')
        dap.command(b'\x10\x00', expect=b'\xff')
        dap.command(b'\x10\x00\x00\xff\xff', expect=b'\xff')
        dap.configure_jtag()
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

    def test_swj_clock_sequence_commands(self, dap):
        # incomplete clock command request
        dap.command(b'\x11\x00\x00', expect=b'\xff')
        # incomplete sequence command request
        dap.command(b'\x12\x0a\xff', expect=b'\xff')
        # clock rate of 0 should produce an error
        dap.command(b'\x11\x00\x00\x00\x00', expect=b'\x11\xff')
        # anything else should succeed
        dap.command(b'\x11\x87\xd6\x12\x00', expect=b'\x11\x00')

        # read the JTAG idcode using the SWJ sequence command any time we are just changing tap states
        dap.configure_jtag()
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

    def test_atomic_commands(self, dap):
        # incomplete command request
        dap.command(b'\x7f', expect=b'\xff')

        # get vendor name twice, and make sure the value is returned twice as expected, and measure execution time
        dap.command(b'\x7f\x02\x00\x01\x00\x01', expect=b'\x7f\x02\x00\x0bNick Kraus\x00\x00\x0bNick Kraus\x00')

        # repeat the above command with a delay in between, make sure the execution time is as expected
        start = time.time()
        dap.command(
            b'\x7f\x03\x00\x01\x09\xff\xff\x00\x01',
            expect=b'\x7f\x03\x00\x0bNick Kraus\x00\x09\x00\x00\x0bNick Kraus\x00'
        )
        delta = (time.time() - start) * 1000000
        assert(delta > 55000)

        # for queued commands, make sure that there are no responses until the final command is given
        with pytest.raises(DapTimeoutError):
            dap.command(b'\x7e\x01\x09\xff\x00')
        with pytest.raises(DapTimeoutError):
            dap.command(b'\x7e\x02\x00\x01\x09\xff\x00')

        dap.write(b'\x00\x02')
        data = dap.read(2048)
        expected_response = b'\x7f\x01\x09\x00'
        expected_response += b'\x7f\x02\x00\x0bNick Kraus\x00\x09\x00'
        expected_response += b'\x00\x17RICEProbe IO CMSIS-DAP\x00'
        assert(data == expected_response)

class TestJTAG:
    def test_jtag_sequence_configure_command(self, dap):
        # incomplete command requests
        dap.command(b'\x14', expect=b'\xff')
        dap.command(b'\x14\x01\x42', expect=b'\xff')
        dap.command(b'\x14\x02\x42\x00\x02', expect=b'\xff')
        # attempt the jtag sequence command while configured as swd, should be an error
        dap.command(b'\x02\x01', expect=b'\x02\x01')
        dap.command(b'\x14\x03\x02\xaa\x09\xaa\xaa\x00\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa', expect=b'\x14\xff')
        # now for correct usage of the command
        dap.configure_jtag()
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

    def test_jtag_configure_idcode_command(self, dap):
        # incomplete configure command requests
        dap.command(b'\x15\x05\x00', expect=b'\xff')
        dap.command(b'\x15\x02\x00', expect=b'\xff')
        # only support up to 4 devices in the jtag chain, so this should be an error
        dap.command(b'\x15\x05\x01\x01\x01\x01\x01', expect=b'\x15\xff')
        # incomplete idcode command request
        dap.command(b'\x16', expect=b'\xff')
        # now configure the tap chain for the actual target: stm32l4r5zi
        dap.configure_jtag()
        dap.command(b'\x15\x02\x04\x05', expect=b'\x15\x00')
        # should fail with an invalid index, and when not configured as jtag
        dap.command(b'\x16\x08', expect=b'\x16\xff\x00\x00\x00\x00')
        dap.command(b'\x02\x01', expect=b'\x02\x01')
        dap.command(b'\x16\x00', expect=b'\x16\xff\x00\x00\x00\x00')
        dap.command(b'\x02\x02', expect=b'\x02\x02')
        # run the idcode command and make sure we get the same result
        dap.command(b'\x16\x00', expect=b'\x16\x00\x77\x04\xa0\x4b')

    def test_jtag_transfer_commands(self, dap):
        # when port is disconnected or index invalid, transfer and transfer_block
        # should return no sequences and data
        dap.command(b'\x03', expect=b'\x03\x00')
        dap.command(b'\x05\x00\x01\x06', expect=b'\x05\x00\x00')
        dap.command(b'\x06\x00\x01\x00\x06', expect=b'\x06\x00\x00\x00')
        dap.command(b'\x02\x02', expect=b'\x02\x02')
        dap.command(b'\x05\x08\x01\x06', expect=b'\x05\x00\x00')
        dap.command(b'\x06\x08\x01\x00\x06', expect=b'\x06\x00\x00\x00')

        dap.configure_jtag()
        # configure the jtag tap details
        dap.command(b'\x15\x02\x04\x05', expect=b'\x15\x00')
        # configure transfer parameters
        dap.command(b'\x04\x00\x64\x00\x01\x00', expect=b'\x04\x00')

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
        dap.command(b'\x07', expect=b'')

    def test_jtag_write_abort_command(self, dap):
        # it is harder to cause a fault on the JTAG transfers, so for now just test that the command returns
        # the expected result
        dap.configure_jtag()
        dap.command(b'\x15\x02\x04\x05', expect=b'\x15\x00')

        # incomplete command
        dap.command(b'\x08', expect=b'\xff')
        dap.command(b'\x08\x01\x12\x34', expect=b'\xff')
        # if index is invalid make sure that an error is returned
        dap.command(b'\x08\xff\x00\x00\x00\x00', expect=b'\x08\xff')
        # now make sure the real command works
        dap.command(b'\x08\x00\x01\x00\x00\x00', expect=b'\x08\x00')

class TestSWD:
    def test_swd_configure_sequence_commands(self, dap):
        # incomplete command requests
        dap.command(b'\x13', expect=b'\xff')
        dap.command(b'\x1d', expect=b'\xff')
        dap.command(b'\x1d\x08', expect=b'\xff')

        dap.configure_swd()
        # configure swd parameters
        dap.command(b'\x13\x00', expect=b'\x13\x00')

        # get the SW-DP IDCODE, which is now 0x2ba01477 for the SWD interface
        dap.command(b'\x1d\x03\x08\xa5\x84\xa2', expect=b'\x1d\x00\x03\x77\x14\xa0\x2b\x02')
        # retry again to make sure the reads stay consistent
        dap.command(b'\x1d\x03\x08\xa5\x84\xa2', expect=b'\x1d\x00\x03\x77\x14\xa0\x2b\x02')

        # SWD sequence when configured as JTAG should response with an error status, but same length response
        dap.command(b'\x02\x02', expect=b'\x02\x02')
        dap.command(b'\x1d\x03\x08\xa5\x84\xa2', expect=b'\x1d\xff\x00\x00\x00\x00\x00\x00')

    def test_swd_transfer_command(self, dap):
        # incomplete transfer configure command request
        dap.command(b'\x04\x00\x00', expect=b'\xff')
        # incomplete transfer command requests
        dap.command(b'\x05\x00\x01', expect=b'\xff')
        dap.command(b'\x05\x00\x03\x06\x06', expect=b'\xff')
        # incomplete transfer block command requests
        dap.command(b'\x06\x00\x01\x00', expect=b'\xff')

        dap.configure_swd()
        # configure swd parameters
        dap.command(b'\x13\x00', expect=b'\x13\x00')
        # configure transfer parameters
        dap.command(b'\x04\x00\x64\x00\x01\x00', expect=b'\x04\x00')
        # get the SW-DP IDCODE, should be same as above
        dap.command(b'\x05\x00\x01\x02', expect=b'\x05\x01\x01\x77\x14\xa0\x2b')
        # try multiple reads to a DP register, and a read match value
        dap.command(b'\x05\x00\x02\x02\x02', expect=b'\x05\x02\x01\x77\x14\xa0\x2b\x77\x14\xa0\x2b')
        dap.command(b'\x05\x00\x01\x20\xff\xff\xff\xff', expect=b'\x05\x01\x01')
        dap.command(b'\x05\x00\x01\x12\x77\x14\xa0\x2b', expect=b'\x05\x01\x01')

        # set CTRLSEL in DP SELECT to 0
        dap.command(b'\x05\x00\x01\x08\x00\x00\x00\x00', expect=b'\x05\x01\x01')
        # write to DP, CTRL/STAT register (0x4), clear register expected default state,
        # then read from DP, CTRL/STAT, to verify previous read, where most bits (but
        # not all) will be zeroed
        xfer_request = b'\x04\x00\x00\x00\x00' + b'\x06'
        xfer_response = b'\x40\x00\x00\x20'
        dap.command(b'\x05\x00\x02' + xfer_request, expect=b'\x05\x02\x01' + xfer_response)
        # now write CSYSPWRUPREQ and CDBGPWRUPREQ bits and clear STICKYERR from CTRL/STAT, then
        # make multiple reads to check read transfer pipelining
        xfer_request = b'\x04\x10\x00\x00\x50' + b'\x06' + b'\x06' + b'\x20\x00\x00\x00\x00'
        xfer_response = b'\x40\x00\x00\xf0' + b'\x40\x00\x00\xf0'
        dap.command(b'\x05\x00\x04' + xfer_request, expect=b'\x05\x04\x01' + xfer_response)
        # write to DP, SELECT register (0x8)
        # set APSEL to 0 and APBANKSEL to the AP register bank containing the ID register (0xf)
        dap.command(b'\x05\x00\x01\x08\xf0\x00\x00\x00', expect=b'\x05\x01\x01')
        # SELECT register is write only on SWD, so we cannot verify the value written

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
        # perform multiple reads of an AP register to make sure that read pipelining works there as well, since
        # it has a different implementation than the DP registers, all reads should return same value
        resp = dap.command(b'\x05\x00\x03\x0f\x0f\x0f')
        assert(resp[3:7] == resp[7:11] and resp[7:11] == resp[11:15])

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

    def test_swd_write_abort_command(self, dap):
        dap.configure_swd()
        # configure swd parameters
        dap.command(b'\x13\x00', expect=b'\x13\x00')
        # configure transfer parameters
        dap.command(b'\x04\x00\x64\x00\x01\x00', expect=b'\x04\x00')

        # incomplete command
        dap.command(b'\x08', expect=b'\xff')
        dap.command(b'\x08\x01\x12\x34', expect=b'\xff')

        # get the SW-DP IDCODE, must be done before anything else
        dap.command(b'\x05\x00\x01\x02', expect=b'\x05\x01\x01\x77\x14\xa0\x2b')
        # set CTRLSEL in DP SELECT to 0
        dap.command(b'\x05\x00\x01\x08\x00\x00\x00\x00', expect=b'\x05\x01\x01')
        # write to DP, CTRL/STAT register (0x4), clear register expected default state,
        # then read from DP, CTRL/STAT, to verify previous read, where most bits (but
        # not all) will be zeroed
        xfer_request = b'\x04\x00\x00\x00\x00' + b'\x06'
        xfer_response = b'\x40\x00\x00\x20'
        dap.command(b'\x05\x00\x02' + xfer_request, expect=b'\x05\x02\x01' + xfer_response)
        # now write CSYSPWRUPREQ and CDBGPWRUPREQ bits and clear STICKYERR from CTRL/STAT, then
        # make multiple reads to check read transfer pipelining
        xfer_request = b'\x04\x10\x00\x00\x50' + b'\x06' + b'\x06' + b'\x20\x00\x00\x00\x00'
        xfer_response = b'\x40\x00\x00\xf0' + b'\x40\x00\x00\xf0'
        dap.command(b'\x05\x00\x04' + xfer_request, expect=b'\x05\x04\x01' + xfer_response)

        # now purposely cause an SWD fault, by setting up a transfer with incorrect parity on the data,
        # while trying to write the DP CTRL/STAT register
        dap.command(b'\x1d\x03\x08\xa9\x85\x21\x10\x00\x00\x50\x00')
        # the WDATAERR flag in DP CTRL/STAT should now be set
        dap.command(b'\x05\x00\x01\x06', expect=b'\x05\x01\x01\xc0\x00\x00\xf0')
        # performing the write abort should clear the WDATAERR flag
        dap.command(b'\x08\x00\x1e\x00\x00\x00', expect=b'\x08\x00')
        # CTRL/STAT should now be at the original (initialized) value
        dap.command(b'\x05\x00\x01\x06', expect=b'\x05\x01\x01\x40\x00\x00\xf0')
