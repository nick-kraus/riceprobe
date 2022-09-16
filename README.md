# RICEProbe

This repository contains the firmware for the RICEProbe (Remote In-Circuit Emulator Probe) product.

# Building the Firmware

The firmware is built using the Zephyr RTOS Projects build tool, [west](https://docs.zephyrproject.org/latest/develop/west/index.html). In order to prepare the repository for a first time build, it must be initialized, using the commands `west init -l west` and `west update`.

Once the repository is initialized, a clean build can be ran with the command `west build -p`, which will use the default board (rice_samv71b_xult). To specify a non-default board to build for, use the command `west build -p -b={board_name}`.

# Running Tests

The RICEProbe integration tests are written in [Python](https://www.python.org/) and are orchestrated with [Pytest](https://docs.pytest.org/). To run the test suite run the command `pytest tests/`.

# Building Documentation

Project level documentation is built with [MkDocs](https://www.mkdocs.org/). The documentation content is written in simple markdown files and can be read through an editor, or viewed through a local webserver after running the command `mkdocs serve`.