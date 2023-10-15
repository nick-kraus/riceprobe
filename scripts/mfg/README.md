# Generating Manufacturing Data Programming Hex Files

The `gen_manufacturing_data_hex.py` script creates a hex file filled with a devices manufacturing data, suitable for programming using openocd.

The script must be given the base address of the memory mapped flash region for the given MCU, as well as the particular offset into this region that the manufacturing data is located. For an atsamv71b MCU, the base address is 0x400000 and the offset address can be found in the board devicetree file.

```shell
# generate hex file filled with manufacturing data, at offset 0x5f0000
./scripts/mfg/gen_manufacturing_data_hex.py --flash-baseaddr=0x400000 --flash-mfg-offset=0x1f0000 --serial=RPB1-2352000001
# program file to riceprobe flash
./scripts/openocd/program.py --adapter=interface/cmsis-dap.cfg --target=board/atmel_samv71_xplained_ultra.cfg mfg.hex
```
