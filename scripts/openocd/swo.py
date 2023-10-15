#!/usr/bin/env python

import argparse
import os
import subprocess

script_dir = os.path.dirname(os.path.realpath(__file__))

argparser = argparse.ArgumentParser(description='open swo server')
argparser.add_argument('--adapter', type=str, default='config/cmsis-dap-swd.cfg', help='adapter openocd config')
argparser.add_argument('--target', type=str, default='config/stm32l4x.cfg', help='target openocd config')
argparser.add_argument('--cpu-freq', type=int, default=80000000, help='cpu frequency in hz')
argparser.add_argument('--swo-freq', type=int, default=500000, help='swo interface frequency in hz')
args = argparser.parse_args()

proc = subprocess.Popen(
    f'openocd -s {script_dir} -f "{args.adapter}" -f "{args.target}" -c "init" -c "reset" -c "itm port 0 on" -c "$_CHIPNAME.tpiu configure -protocol uart -output :5335 -traceclk {args.cpu_freq} -pin-freq {args.swo_freq}" -c "$_CHIPNAME.tpiu enable"'
)
try:
    proc.wait()
except KeyboardInterrupt:
    proc.terminate()
