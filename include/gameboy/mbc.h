#ifndef GB_MBC
#define GB_MBC

#include "gameboy/mbc1.h"

/**
 * Loader function for an MBC controller.
 *
 * Each kind of supported MBC should implement a version of this.
 * */
typedef void (* mbc_loader) (uint8_t*, uint8_t*) ;

/**
 * Dummy loader for the MBC0 - does nothing
 */
void gb_mbc0_load (uint8_t* rom, uint8_t* ram) { /* nada */ }

#endif
