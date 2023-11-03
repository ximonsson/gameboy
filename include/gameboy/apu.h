#ifndef GB_APU
#define GB_APU

#include <stdlib.h>

/**
 * Reset the APU.
 */
void gb_apu_reset () ;

/**
 * Step the APU according to the number CPU cycles provided.
 */
void gb_apu_step (uint32_t /* cc */) ;

/**
 * Copy sampled samples to the buffer and set the size parameter to the length of the buffer.
 */
void gb_apu_samples (float* /* buffer */, size_t* /* samples */) ;

#endif
