# RICEProbe

This repository contains the firmware and host utilities for the RICEProbe, a **R**emote **I**n-**C**ircuit **E**mulator Probe.

## Probe Firmware

The firmware is built off of the Zephyr RTOS, using its meta-tool [west](https://docs.zephyrproject.org/latest/develop/west/index.html).

In order to build the firmware, the steps outlined in the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) must be followed. Once that is complete, the following commands will build the firmware from a fresh repository:

```bash
# initialize the repository and download all zephyr modules
west init -l firmware
west update
# build the firmware for the riceprobe development board
west build -b=rice_samv71b_xult firmware
```

## Documentation

Project level documentation is built with [MkDocs](https://www.mkdocs.org/). The documentation content is written in simple markdown files and can be read through an editor, or viewed through a local webserver after running the command `mkdocs serve`.
