#!/usr/bin/env python

import argparse
import os
import subprocess

script_dir = os.path.dirname(os.path.realpath(__file__))

argparser = argparse.ArgumentParser(description='program file onto target')
argparser.add_argument('--adapter', type=str, default='config/cmsis-dap-swd.cfg', help='adapter openocd config')
argparser.add_argument('--target', type=str, default='config/stm32l4x.cfg', help='target openocd config')
argparser.add_argument('file', type=str, help='file to program')
args = argparser.parse_args()

proc = subprocess.Popen(
    f'openocd -s {script_dir} -f "{args.adapter}" -f "{args.target}" -c "program {args.file} verify reset exit"'
)
try:
    proc.wait()
except KeyboardInterrupt:
    proc.terminate()
