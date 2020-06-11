#ifndef GB_FILE
#define GB_FILE

#include <stdint.h>
#include <stdio.h>

#define GB_HEADER_LOGO_SIZE 48
#define GB_HEADER_LOCATION 0x0100
#define GB_HEADER_SIZE 0x4F
#define GB_HEADER_TITLE_SIZE 16

/**
 * gb_file_header contains information from the ROM file header.
 */
typedef
struct file_header
{
	uint8_t rom_size; // number of 8KB banks
	uint8_t ram_size; // number of 4KB banks
	uint8_t mbc;      // MBC ID number
	uint8_t cgb;      // CGB support flag
	uint8_t sgb;      // SGB support flag
	uint8_t title[GB_HEADER_TITLE_SIZE]; // ROM title
	uint8_t logo[GB_HEADER_LOGO_SIZE];   // Nintendo logo
}
gb_file_header;

/**
 * gb_load_file loads the game file with the file pointer.
 * A non-zero return value is returned in case of error.
 */
int gb_load_file (FILE* /* fp */, gb_file_header* /* header */, const uint8_t** /* ROM */) ;

/**
 * gb_print_header_info prints information about the cartridge retrieved from the header.
 */
void gb_print_header_info (gb_file_header /* hdr */);

#endif /* _GB_FILE_ */
