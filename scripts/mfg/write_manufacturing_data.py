import argparse
import math
import re
import serial
import streamexpect
import uuid

MFG_DATA_TAG = 0x7a5a
MFG_DATA_MAJOR_VERSION = 0x01
MFG_DATA_MINOR_VERSION = 0x01

MFG_DATA_SERIAL_LEN = 32
MFG_DATA_UUID_LEN = 16
# tag and version each 2 bytes
MFG_DATA_TOTAL_LEN = 2 + 2 + MFG_DATA_SERIAL_LEN + MFG_DATA_UUID_LEN

MFG_SERIAL_REGEX = re.compile(r'^RPB1-[23][0-9][0-5][0-9][0-9]{6}$')

def luhn_mod36_check_gen(string):
    factor = 2
    sum = 0
    n = 36

    char_to_code = lambda c : \
        ord(c) - ord('0') if c >= '0' and c <= '9' else \
            ord(c) - ord('A') + 10 if c >= 'A' and c <= 'Z' else None
    code_to_char = lambda c : \
        chr(ord('0') + c) if c >= 0 and c <= 9 else \
            chr(ord('A') + c - 10) if c >= 10 and c <= 35 else None

    for i in range(len(string) - 1, -1, -1):
        code = char_to_code(string[i])
        addend = factor * code
        factor = 1 if factor == 2 else 2
        addend = math.floor(addend / n) + (addend % n)
        sum += addend
    
    remainder = sum % n
    return code_to_char((n - remainder) % n)

if __name__ == '__main__':
    arg_parse = argparse.ArgumentParser(description='tool to program manufacturing data to non-volatile storage')
    arg_parse.add_argument('--serial', type=str.upper, required=True, help='serial number without luhn check digit')
    arg_parse.add_argument('--uuid', type=str, help='version 4 uuid')

    args = arg_parse.parse_args()

    # ensure proper format and generate the check digit for serial number
    if re.match(MFG_SERIAL_REGEX, args.serial) is None:
        raise ValueError('given serial number does not match required format')
    luhn_check_serial = args.serial.replace('-', '')
    check = luhn_mod36_check_gen(luhn_check_serial)
    args.serial += check
    print(f'serial number "{args.serial}" after check digit "{check}" added')

    if args.uuid is None:
        id = uuid.uuid4()
    else:
        id = uuid.UUID(args.uuid)
    print(f'uuid "{id}"')

    # the shell flash write command operates on 4-byte words, and the flash subsystem wants to write
    # in chunks of four words, so make sure our data alignment matches
    data_size = MFG_DATA_TOTAL_LEN
    if data_size % 16 != 0:
        data_size = MFG_DATA_TOTAL_LEN + (16 - (MFG_DATA_TOTAL_LEN % 16))

    data = bytearray(data_size)
    data[0:2] = MFG_DATA_TAG.to_bytes(2, byteorder='little')
    data[2:3] = MFG_DATA_MAJOR_VERSION.to_bytes(1, byteorder='little')
    data[3:4] = MFG_DATA_MAJOR_VERSION.to_bytes(1, byteorder='little')
    data[4:36] = args.serial.ljust(MFG_DATA_SERIAL_LEN, '\x00').encode('utf-8')
    data[36:52] = id.bytes
    # pad remainder with byte 0xff
    data[52:] = b'\xff' * len(data[52:])

    # the shell flash write command requires each word to be in little-endian format, so make sure
    # to convert each 4-byte chunk before writing
    for i in range(0, data_size, 4):
        chunk = data[i:i+4]
        chunk.reverse()
        data[i:i+4] = chunk

    # manufacturing firmware uses the zephyr cdc-acm vid:pid 
    ser = serial.serial_for_url(f'hwgrep://2fe3:0001', timeout=0.1)
    with streamexpect.wrap(ser) as stream:
        # clear all existing input
        stream.reset_input_buffer()

        # make sure we have a working prompt
        stream.write(b'\n')
        stream.expect_bytes(b'mfg:~$ ')

        stream.write(b'partition_info manufacturing\n')
        pattern = rb'.*device name = ([-@\w]*).*partition size = (\w*).*partition offset = (\w*).*'
        match = stream.expect_regex(pattern, regex_options=re.DOTALL)
        flash_dev = match.groups[0].decode('ascii')
        mfg_part_offset = int(match.groups[2], 16)

        stream.write(f'flash erase {flash_dev} {mfg_part_offset:x}\n'.encode('utf-8'))
        stream.expect_bytes(b'Erase success.')

        for i in range(0, data_size, 16):
            offset = mfg_part_offset + i
            word1 = data[i:i+4].hex()
            word2 = data[i+4:i+8].hex()
            word3 = data[i+8:i+12].hex()
            word4 = data[i+12:i+16].hex()
            command = f'flash write {flash_dev} {offset:x} {word1} {word2} {word3} {word4}\n'.encode('utf-8')

            stream.write(command)
            stream.expect_bytes(b'Write OK.')
            stream.expect_bytes(b'Verified.')
