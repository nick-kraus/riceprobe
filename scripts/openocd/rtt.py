#!/usr/bin/env python

import argparse
import os
import subprocess

script_dir = os.path.dirname(os.path.realpath(__file__))

argparser = argparse.ArgumentParser(description='open rtt server')
argparser.add_argument('--adapter', type=str, default='config/cmsis-dap-swd.cfg', help='adapter openocd config')
argparser.add_argument('--target', type=str, default='config/stm32l4x.cfg', help='target openocd config')
argparser.add_argument('--rtt-start', type=int, default=0x20000000, help='rtt control block search start address')
argparser.add_argument('--rtt-size', type=int, default=0x100000, help='rtt control block search size')
args = argparser.parse_args()

proc = subprocess.Popen(
    f'openocd -d2 -s {script_dir} -f "{args.adapter}" -f "{args.target}" -c "init" -c "reset" -c "sleep 100" -c "rtt setup {args.rtt_start} {args.rtt_size} \\"SEGGER RTT\\"" -c "rtt start" -c "rtt channels" -c "rtt server start 5335 0"'
)
try:
    proc.wait()
except KeyboardInterrupt:
    proc.terminate()
