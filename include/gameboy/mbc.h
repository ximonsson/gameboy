#ifndef GB_MBC
#define GB_MBC

#include "gameboy/mbc0.h"
#include "gameboy/mbc1.h"
#include "gameboy/mbc3.h"
#include "gameboy/mbc5.h"

/**
 * Loader function for an MBC controller.
 *
 * Each kind of supported MBC should implement a version of this.
 * */
typedef void (* mbc_loader) (uint8_t*, uint8_t*) ;

#endif
