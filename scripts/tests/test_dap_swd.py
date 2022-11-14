def test_swd_configure_sequence_commands(dap):
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

def test_swd_transfer_command(dap):
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

def test_swd_write_abort_command(dap):
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
