# Game Boy

Game Boy emulator written in C. Compiles as a shared library so you can include it in another application.

## Requirements

None for the actual library. For the test application you need to install `sdl2`, `libpulse` and have OpenGL support. Everything is usually available through package manager in Linux.


## Running

To compile the library just run `make lib` and the output file can be found at `./lib/libgameboy.a`

There is a sample application with source code located in `./examples/app` that you can use as an example. Run `make` and the application will be built as well and can be found at `./bin/gameboy`. It needs the shader source that comes in `./examples/app/shaders/` and the paths are hardcoded (yep), so make sure to run from the root directory of this project.

## Status

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


### MBC Support

Not really aiming for supporting that many but at least the comon ones:

* [x] MBC0
* [x] MBC1
* [x] MBC3 **Not Timer though**
* [x] MBC5


### TODO

* [ ] Audio support!
* [x] Sprite - BG priority needs to be solved.
* [ ] RTC for MBC3.
* [x] Timer support - need to keep better track of CPU cycles.
