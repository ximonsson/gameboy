# Game Boy

Game Boy emulator written in C. Compiles as a shared library so you can include it in another application.

## Requirements

None for the actual library. For the test application you need to install `sdl2`, `libpulse` and have OpenGL support. Everything is usually available through package manager in Linux.


## Running

To compile the library just run `make lib` and the output file can be found at `./lib/libgameboy.a`

There is a sample application with source code located in `./examples/app` that you can use as an example. Run `make` and the application will be built as well and can be found at `./bin/gameboy`. It needs the shader source that comes in `./examples/app/shaders/` and the paths are hardcoded (yep), so make sure to run from the root directory of this project.

## Status

### Known Issues

* [x] *Solved* Timing: Seems like timing of the emulator is super off. I have no idea why though... I do think this is the reason for the games that are not working properly.
* [x] Behavior when LCD disabled: from what I understand the PPU should do nothing and stay in VBLANK mode (maybe). However a lot games are not working if the PPU is not ticking even while off. No idea what the error can be here. If I let the PPU tick even when it is disabled other games will crash. I sticking to keeping it off and try to find other solutions. <span style="color:FF0000">Not sure this is solved but setting default values of LCDC at least gets a lot of games started. Some games still have some isses.</span>

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

*Passed*.


### MBC Support

Not really aiming for supporting that many but at least the comon ones:

* [x] MBC0
* [x] MBC1 **But there might be some error in bank switching**
* [x] MBC3 **Not Timer though**
* [x] MBC5

### Problematic games

I think there is an error with the implementation of MBC1 at the moment because there are many games there that seem to have errors. There is of course the possibility that these games are trying to interract with the APU and not succeeding which is causing issues.

* Pokemon Red/Blue: experience bar is not showing.
* Asterix: stops working after title screen.
* Earthworm Jim: Does not get past credits.
* Dragon Ball Z: skips intro clip.
* Street Fighter II: a lot of flickering.
* Bomberman: freezes when selecting mode.
* Super Mario Land 2 - 6 Golden Coins: Mario is invisble.
* Pokemon Gold/Silver: goes back to title screen randomly when changing location (exiting/entering houses) - I think this is because of RTC support not available.


### TODO

* [ ] Audio support!
* [x] Sprite - BG priority needs to be solved.
* [ ] RTC for MBC3.
* [x] Timer support - need to keep better track of CPU cycles.
