#!/usr/bin/env python

import argparse
import os
import subprocess

script_dir = os.path.dirname(os.path.realpath(__file__))

argparser = argparse.ArgumentParser(description='get stm32l4x chip and unique id')
argparser.add_argument('--adapter', type=str, default='config/cmsis-dap-swd.cfg', help='adapter openocd config')
args = argparser.parse_args()

proc = subprocess.Popen(
    f'openocd -s {script_dir} -f "{args.adapter}" -f "config/stm32l4x.cfg" -c "init" -c "reset halt" -c "echo \\"stm32 chip id:\\"" -c "mdw 0xe0042000 1" -c "echo \\"stm32 unique id:\\"" -c "mdw 0x1fff7590 3" -c "exit"'
)
try:
    proc.wait()
except KeyboardInterrupt:
    proc.terminate()
