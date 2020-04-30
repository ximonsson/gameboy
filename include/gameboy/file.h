#ifndef GB_FILE
#define GB_FILE

#include <stdint.h>

/**
 * gb_load_file loads the game file with the given path.
 * A non-zero return value is returned in case of error.
 */
int gb_load_file (const char* /* path */, uint8_t * /* MBC */, uint8_t ** /* ROM */, uint8_t ** /* RAM */) ;

#endif /* _GB_FILE_ */
