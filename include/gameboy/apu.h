#ifndef GB_APU
#define GB_APU

/**
 * Reset the APU.
 */
void gb_apu_reset () ;

/**
 * Step the APU according to the number CPU cycles provided.
 */
void gb_apu_step (int /* cc */) ;

#endif
