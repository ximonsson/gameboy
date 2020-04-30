#ifndef GB_CPU_H
#define GB_CPU_H

#include <stdint.h>

int gb_cpu_step () ;

void gb_cpu_reset () ;

/**
 * Get a pointer to a location within the RAM.
 * USE WITH CAUTION!
 */
uint8_t* gb_cpu_mem (uint16_t /* offset */) ;

#endif
