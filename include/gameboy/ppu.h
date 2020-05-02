#ifndef GB_PPU
#define GB_PPU

#include <stdint.h>

/**
 * Screen size definitions.
 */
#define GB_SCANLINE 456
#define GB_SCANLINES 154
#define GB_FRAME 70224

/**
 * Location of VRAM in memory map.
 */
#define VRAM_LOC 0x8000

/**
 * Step the PPU for a certain number of cycles.
 */
void gb_ppu_step (int /* cycles */) ;

/**
 * Reset the PPU.
 * Should be done on startup _after_ the CPU.
 */
void gb_ppu_reset () ;

/**
 * Return a pointer to the current buffer being drawn to.
 */
const uint8_t* gb_ppu_lcd () ;

#endif
