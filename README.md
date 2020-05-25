# Game Boy

Game Boy emulator written in C. Compiles as a shared library so you can include it in another application.

## Requirements

None for the actual library. For the test application you need to install `sdl2`, `libpulse` and have OpenGL support. Everything is usually available through package manager in Linux.


## Running

To compile the library just run `make lib` and the output file can be found at `./lib/libgameboy.a`

There is a sample application with source code located in `./examples/app` that you can use as an example. Run `make` and the application will be built as well and can be found at `./bin/gameboy`. It needs the shader source that comes in `./examples/app/shaders/` and the paths are hardcoded (yep), so make sure to run from the root directory of this project.

## Status

### Known Issues

* Timing: Seems like timing of the emulator is super off. I have no idea why though... I do think this is the reason for the games that are not working properly.

### Blargg's Tests

#### CPU Instructions

| Test # | Status        |
|--------|---------------|
| 01     | Pass          |
| 02     | Pass          |
| 03     | Pass          |
| 04     | Pass          |
| 05     | Pass          |
| 06     | Pass          |
| 07     | Pass          |
| 08     | Pass          |
| 09     | Pass          |
| 10     | Pass          |
| 11     | Pass          |

#### Instruction timing

Just gives me *Failed #255*. I don't even know what it means. Something might be very wrong with my timing but I don't understand what it could be.


### MBC Support

Not really aiming for supporting that many but at least the comon ones:

* [x] MBC0
* [x] MBC1 **But there might be some error in bank switching**
* [x] MBC3 **Not Timer though**
* [x] MBC5


### Problematic games

I think there is an error with the implementation of MBC1 at the moment because there are many games there that seem to have errors. Seeing how the timing tests are failing also there could be something there but I have no idea. There is of course the possibility that these games are trying to interract with the APU and not succeeding which is causing issues.

* Pokemon Gold/Silver: goes back to title screen randomly when changing location (exiting/entering houses) - I think this is because of RTC support not available.
* Earthworm Jim: Losing a life as soon as the game starts.
* The Simpsons, Bart vs the Juggernauts: does not start at all just randomness on screen.
* Dragon Ball Z: Skips intro movie and jumps a lot in what happens - MBC1.
* Street Fighter II: Flickers a lot during fighting with Chun Li for example.
* Bomberman: seems to freeze when choosing mode.
* Gargoyle's Quest: crashes because it jumps somewhere it shouldn't.
* Yoshi & Mario: jibberish on title screen.
* Super Mario Land 2 - 6 Golden Coins: jumps to invalid memory addresses.


### TODO

* [ ] Audio support!
* [x] Sprite - BG priority needs to be solved.
* [ ] RTC for MBC3.
* [x] Timer support - need to keep better track of CPU cycles.o
