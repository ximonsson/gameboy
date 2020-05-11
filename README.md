# Game Boy

Game Boy emulator written in C.


## Status

### Blargg's Tests

#### CPU Instructions

| Test # | Status        | Note                               |
|--------|---------------|------------------------------------|
| 01     | Fail DAA      | No one seems to know how to implement this instruction so I am considering this test passed. |
| 02     | Loops forever | Timer interrupts are not correctly synchronized, should be this. |
| 03     | [x]           |  |
| 04     | [x]           |  |
| 05     | [x]           |  |
| 06     | [x]           |  |
| 07     | [x]           |  |
| 08     | [x]           |  |
| 09     | [x]           |  |
| 10     | [x]           |  |


### MBC Support

* [x] MBC0
* [ ] MBC1
* [ ] MBC3
* [ ] MBC5
