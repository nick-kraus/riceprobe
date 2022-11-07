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
