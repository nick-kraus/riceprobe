#!/usr/bin/env python

import argparse
import intelhex
import math
import re
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

argparser = argparse.ArgumentParser(description='create an intel hex file of the manufacturing data')
argparser.add_argument('--flash-baseaddr', type=str, required=True, help='start of memory mapped flash region')
argparser.add_argument('--flash-mfg-offset', type=str, required=True, help='start of manufacturing patition in flash')
argparser.add_argument('--serial', type=str.upper, required=True, help='serial number without luhn check digit')
args = argparser.parse_args()

# confirm serial number is in proper format
if re.match(MFG_SERIAL_REGEX, args.serial) is None:
    raise ValueError('given serial number does not match required format')

# append luhn check character
luhn_check_serial = args.serial.replace('-', '')
check = luhn_mod36_check_gen(luhn_check_serial)
args.serial += check
print(f'serial number "{args.serial}" after check digit "{check}" added')

# uuid v4 generation
id = uuid.uuid4()
print(f'uuid "{id}"')

# generate the intel hex file
ihex = intelhex.IntelHex()

offset = int(args.flash_baseaddr, base=0) + int(args.flash_mfg_offset, base=0)
ihex.puts(offset + 0, MFG_DATA_TAG.to_bytes(2, byteorder='little'))
ihex.puts(offset + 2, MFG_DATA_MAJOR_VERSION.to_bytes(1, byteorder='little'))
ihex.puts(offset + 3, MFG_DATA_MINOR_VERSION.to_bytes(1, byteorder='little'))
ihex.puts(offset + 4, args.serial.ljust(MFG_DATA_SERIAL_LEN, '\x00').encode('utf-8'))
ihex.puts(offset + 36, id.bytes)

ihex.tofile('mfg.hex', format='hex')
print('\nwrote out to file "mfg.hex"')
