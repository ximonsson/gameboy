#ifndef GB_H
#define GB_H

#include <stdint.h>

#define GB_LCD_WIDTH 160
#define GB_LCD_HEIGHT 144

/**
 * Load the file at the given file path.
 *
 * TODO
 * I would like to change this to a FILE pointer instead so the calling application
 * is repsonsible for finding and opening the file for more flexibility.
 */
int gb_load (const char* file) ;

/**
 * Step the emulator.
 * This equals stepping until an entire new frame is ready.
 */
void gb_step () ;

/**
 * Deinitiliaze the Game Boy emulator.
 */
void gb_quit () ;

/**
 * Get pointer to the screen buffer.
 * This can change between frames so it is recommended to make a call each time
 * right before drawing to screen.
 */
const uint8_t* gb_lcd () ;

#endif /* GB_H */
