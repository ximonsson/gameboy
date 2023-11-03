#ifndef GB_MBC
#define GB_MBC

#include "gb/mbc0.h"
#include "gb/mbc1.h"
#include "gb/mbc2.h"
#include "gb/mbc3.h"
#include "gb/mbc5.h"

/**
 * Loader function for an MBC controller.
 *
 * Each kind of supported MBC should implement a version of this.
 * */
typedef void (* mbc_loader) (uint8_t*) ;

#endif
