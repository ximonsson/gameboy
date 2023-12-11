# Game Boy

Game Boy emulator written in C.

The code that emulates the hardware compiles as a shared library that one can include in an application. The library is designed to take of emulating the functionality of the hardware, and a calling application acts as a wrapper to playback video and audio, as well as user interface. There is a sample application in [examples/app](./examples/app).


## Usage

### Requirements

None.

### Building

To compile the library just run `make lib` and the output file can be found at `./lib/libgameboy.a`. Running `make` will build the library and the sample application. There are some debug flags you can send, but it becomes very spammy so I don't recommend it.


### API

Below is a basic example of how to use the library for playing a game:

```c
#include "gb.h"

/*
    your init code for video and audio playback
*/

const uint8_t *rom = ... ;  // content of your ROM file.

gb_init (audio_sample_rate);  // your application takes care of audio playback and what freq.

size_t ram_size;
uint8_t *ram = NULL;

if (gb_load (rom, &ram, &ram_size) != 0)
    exit(1);

// You need to define a number of CPU cycles that you want step.
const uint32_t STEP_SIZE = ... ;
uint32_t cc = 0;  // keep track of CPU cycles.

while (1)
{
    cc += gb_step (STEP_SIZE);

    if (cc >= GB_FRAME)
    {
        // get pointer to pixel data
        const uint16_t *gb_screen = gb_lcd ();

        /* your draw code */

        cc -= GB_FRAME;
    }

    // get audio samples
    size_t audio_samples_size;
    float *audio_samples;
    gb_audio_samples (audio_samples, &audio_samples_size);

    /* your audio playback code */

    /*
        your code for handling events */
    event = ... ;
    gb_press_button ( /* depending on the event */ );  // or gb_release_button
}

gb_quit ();
```

Above you can notice that you need to specify a sampling rate for `gb_init` method. When later choosing a step size you will probably want something that is proportional to the sampling rate, because you will most likely want to sync by audio. I recommend something similar to `GB_CPU_CLOCK / SAMPLE_RATE * BUFFER_SIZE`, where `BUFFER_SIZE` is the number of audio samples you would like to buffer before sending to the playback device.


### Color Format

You will note that `gb_lcd` returns a pointer to an array of unsigned 16-bit integers. Each integer contains *one* color; first 5-bits are the red channel, next five are the green channel, next five the blue, and ultimately the last bit is ignored. Same format as the [palettes are stored on the CGB](https://gbdev.io/pandocs/Palettes.html#lcd-color-palettes-cgb-only).

Tip: If you use OpenGL to render the LCD, you can send the pointer directly as a texture using internal format `GL_RGB5_A1` and data type `GL_UNSIGNED_SHORT_5_5_5_1`.


### Emulating battery support

Some cartridges have a small battery in them to keep save data and highscore within the RAM for next run.

You will notice that you need to send a reference to a pointer for RAM for the `gb_load` function. This is for battery support. In the example above a `NULL` pointer is sent and then the library will allocate a new memory space according to the cartridge header. If the MBC has battery support you can store the data within this memory space. In a later run you can load the data and supply it instead of the `NULL` pointer, to load save dat. For example:

```c
// ...

uint8_t *ram = /* your loaded data from the previous session */;
size_t ram_size = /* this should be set to the same length of `ram` */;

if (gb_load (rom, &ram, &ram_size) != 0)
    exit (1);

// ...
```


## Sample application

As stated, the emulator itself is a shared library, so there is nothing to run, but in [`examples/app`](./examples/app) there is a sample application on how to use the library as a running application.


### Requirements

For the test application you need to install `sdl2` and have OpenGL support. Everything is usually available through the package manager in Linux.


### Building

```sh
make bin
```


### Running

There is a sample application with source code located in `./examples/app` that you can use as an example. Run `make` and the application will be built as well and can be found at `./bin/gameboy`. It needs the shader source that comes in `./examples/app/shaders/` and the paths are hardcoded (yep), so make sure to run from the root directory of this project.

You can then run a game with:

```sh
$ ./bin/gb [path to ROM]
```

Keys `W`, `A`, `S` and `D` are up, left, down and right respectively, while `J` and `K` are the `A` and `B` buttons, and `SPACE` and `X` are `START` and `SELECT`.

If you are running a game that has battery support you can supply a third argument which is where to store save data.

```sh
$ ./bin/gb [path to ROM] [path to RAM]
```

If the save data does not exist on startup, RAM will be allocated as if a `NULL` pointer is sent to `gb_load` and in the end the application will store the RAM to this file path.


## TODO

The sample app is not the best. The main loop runs one frame and then retrieves pixel and audio data. Between each iteration of the main loop not the same number of CPU cycles are run, which creates a variable amount of audio samples. Even though it is roughly equal between iterations, the call to playback the samples is blocking which makes the emulator feel laggy. This needs to be re-engineered to something a little better that makes it feel smoother. The audio playback is what emulates the gameboy clock, so it needs some thought.
