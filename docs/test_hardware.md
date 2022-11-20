# Test Hardware

## Description

The current test hardware for the RICEProbe project is a Nucleo-L4R5ZI development board, with wires jumpered to the SAMV71B Xplained Ultra board which is acting as the RICEProbe itself.

The Nucleo-L4R5ZI board uses the STM32L4R5ZIT6 chip itself, which supports a wide range of peripherals that will help with testing the various IO functionality of the RICEProbe.

The tests currently depend on having a known set of connections between the target and the probe, and are all currently ran on probe and target hardware. In order for the tests to succeed, the signal connections described below must be upheld, and the target must also have the test firmware image programmed (found in the `scripts/tests/firmware` directory).

## SAMV71B-Xplained to Nucleo-L4R5ZI Connections

### Power Connections

| Signal Name          | SAM Xplained IO | ATSAMV71B IO | Nucleo-L4R5ZI IO | STM32L4R5ZI IO |
|----------------------|-----------------|--------------|------------------|----------------|
| Vin                  | J501-8          | N/A          | CN8-15           | N/A            |
| Gnd                  | J501-7          | N/A          | CN8-13           | N/A            |

### DAP Connections

| Signal Name          | SAM Xplained IO | ATSAMV71B IO | Nucleo-L4R5ZI IO | STM32L4R5ZI IO |
|----------------------|-----------------|--------------|------------------|----------------|
| jtag tck / swd swclk | J503-3          | PA0          | CN11-15          | PA14           |
| jtag tms / swd swdio | J503-4          | PA6          | CN11-13          | PA13           |
| jtag tdo / swd swo   | J503-1          | PD28         | CN12-31          | PB3            |
| jtag tdi             | J503-5          | PD27         | CN11-17          | PA15           |
| nreset               | J503-7          | PC19         | CN8-5            | nreset         |
| vref                 | J503-2          | PD30         | CN8-7            | N/A            |
