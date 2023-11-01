# Game Boy

Game Boy emulator written in C. Compiles as a shared library so you can include it in another application.


## Running

As stated, the emulator itself is a shared library, so there is nothing to run, but in `examples/app` there is a sample application on how to use the library as a running application.


### Requirements

None for the actual library. For the test application you need to install `sdl2`, `libpulse` and have OpenGL support. Everything is usually available through the package manager in Linux.


### Building

To compile the library just run `make lib` and the output file can be found at `./lib/libgameboy.a`. Running `make` will build the library and the sample application. There are some debug flags you can send, but it becomes very spammy so I don't recommend it.


### Running

There is a sample application with source code located in `./examples/app` that you can use as an example. Run `make` and the application will be built as well and can be found at `./bin/gameboy`. It needs the shader source that comes in `./examples/app/shaders/` and the paths are hardcoded (yep), so make sure to run from the root directory of this project.

You can then run a game with:

```sh
$ ./bin/gameboy [path to ROM]
```

Keys `W`, `A`, `S` and `D` are up, left, down and right respectively, while `J` and `K` are the `A` and `B` buttons, and `SPACE` and `X` are `START` and `SELECT`.


## TODO

The sample app is not the best. The main loop runs one frame and then retrieves pixel and audio data. Between each iteration of the main loop not the same number of CPU cycles are run, which creates a variable amount of audio samples. Even though it is roughly equal between iterations, the call to playback the samples is blocking which makes the emulator feel laggy. This needs to be re-engineered to something a little better that makes it feel smoother. The audio playback is what emulates the gameboy clock, so it needs some thought.
