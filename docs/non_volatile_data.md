# Non-Volatile Data

## Manufacturing Data

The RICEProbe firmware stores permanent manufacturing data in MCU internal flash. Eventually this should take place in the 512-byte 'User Signature Area', which isn't erased by the hardware ERASE pin or software ERASE command. The following items are stored in the manufacturing data partition.

### Serial Number

The serial number format is currently specified as MMMM-YYWWNNNNC, with the following format specifiers:

- MMMM is an alphanumeric model identifier.
- YY is the last two decimal digits of the year of manufacture.
- WW is the decimal week number of the year of manufacture.
- NNNN is a decimal sequential number of devices produced in the given week of manufacture.
- C is a check digit computed with the [Luhn mod N algorithm](https://en.wikipedia.org/wiki/Luhn_mod_N_algorithm) over all the previous digits (so not including the check digit itself) with the hyphen removed, requiring 36 codepoints (all case-insensitive alphanumeric symbols).

The serial number is intended to be unique across all future device iterations and derivatives, because the serial number is expected to be used to differentiate connections between multiple devices and one host machine.

The serial number format can be adjusted in certain ways without breaking the check digit computation. Because every other digit is doubled in the check digit algorithm, and 0 values get reduced to 0 (as long as 0 is the first codepoint in the mapping used), padding can be added to the NNNN sequential number as long as it is done in sets of two digits. For example, the serial number MMMM-YYWWNNNNC and MMMM-YYWW00NNNNC will have the same check digit and can therefore be identified as the same device, if it becomes necessary to add further sequantial digits in the future.

### UUIDv4

A version 4 UUID will be included in manufacturing data to act as a secondary means of uniquely identifying a device, which can be used in the unlikely scenario that two devices with a shared serial number are found.

### Layout

The data will be layed out in a 512-byte partition as follows, with all data stored in little endian format:

| Start Address | Size (bytes) | Name           | Format Description           |
|---------------|--------------|----------------|------------------------------|
| 0x00          | 4            | Tag            | 4 byte binary tag            |
| 0x04          | 2            | Layout Version | 16-bit version number        |
| 0x06          | 32           | Serial Number  | NULL terminated ASCII string |
| 0x26          | 16           | UUIDv4         | Binary UUIDv4 bytes          |
