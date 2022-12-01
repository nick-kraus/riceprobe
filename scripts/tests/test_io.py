def test_unsupported(io):
    # unsupported writes return a command id of 0 and the unsupported error status
    io.command(b'\xff\xff\xff\x7f', expect=b'\x00\x81')
    # writes with an invalid id (too large) will use the ivalid error status
    io.command(b'\xff\xff\xff\xff', expect=b'\x00\x82')
