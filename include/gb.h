#ifndef GB_H
#define GB_H

#include "gameboy/io.h"
#include <stdint.h>
#include <stdlib.h>

#define GB_LCD_WIDTH 160
#define GB_LCD_HEIGHT 144

#define GB_CPU_CLOCK 4194304
#define GB_SCANLINE 456
#define GB_SCANLINES 154
#define GB_FRAME 70224

/**
 * Initialize the emulator.
 *
 * Right now this is just for setting the audio sample rate before starting.
 */
void gb_init (int /* sample rate */);

/**
 * Load the file at the given file path.
 *
 * TODO
 * I would like to change this to a FILE pointer instead so the calling application
 * is repsonsible for finding and opening the file for more flexibility.
 */
int gb_load (const char * /* file */) ;

/**
 * Step the emulator for *at least* a given number of CPU cycles. The function returns
 * the *actual* number of cycles that ran.
 *
 * There is no way to guarantee the number of cycles to step because each CPU
 * instruction is different, therefore the input act as a minimum of cycles to
 * iterate.
 */
uint32_t gb_step (uint32_t /* cc */) ;

/**
 * Deinitiliaze the Game Boy emulator.
 */
void gb_quit () ;

/**
 * Get pointer to the screen buffer.
 *
 * Then underlying array has length GB_LCD_WIDTH * GB_LCD_HEIGHT * 3, where the 3
 * represents the three color channels RGB. The pointer can be directly fed as an
 * OpenGL texture.
 *
 * This can change between frames so it is recommended to make a call each time
 * AFTER stepping the emulator and drawing while it is paused before stepping again.
 */
const uint8_t* gb_lcd () ;

// TODO
// I don't like this solution for the buttons.
// I think maybe it would be better to move the definition of the enum of buttons here.

typedef gb_io_button gb_button;

/**
 * Emulate pressing (and holding down) a key.
 */
void gb_press_button (gb_button) ;

/**
 * Emulate releasing a key.
 */
void gb_release_button (gb_button) ;

/**
 * Get audio samples generated.
 */
void gb_audio_samples (float * /* buffer */, size_t * /* n */) ;

#endif /* GB_H */
