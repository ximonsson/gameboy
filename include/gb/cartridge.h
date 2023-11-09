#ifndef GB_FILE
#define GB_FILE

#include <stdint.h>
#include <stdio.h>

#define GB_HEADER_LOGO_SIZE 48
#define GB_HEADER_LOCATION 0x0100
#define GB_HEADER_SIZE 0x4F
#define GB_HEADER_TITLE_SIZE 16

/**
 * gb_cartridge_header contains information from the ROM file header.
 */
typedef
struct cartridge_header
{
	uint8_t rom_size; // number of 8KB banks
	uint8_t ram_size; // number of 4KB banks
	uint8_t mbc;      // MBC ID number
	uint8_t cgb;      // CGB support flag
	uint8_t sgb;      // SGB support flag
	uint8_t title[GB_HEADER_TITLE_SIZE]; // ROM title
	uint8_t logo[GB_HEADER_LOGO_SIZE];   // Nintendo logo
}
gb_cartridge_header;

/**
 * gb_load_cartridge loads the game data in the first argument. It will populate
 * the header supplied with data from the cartridge.
 *
 * The last argument poinst to RAM data. In case the `*RAM` is non-NULL then it is
 * assumed that it is pointing to previously stored RAM data, emulating battery. In
 * case of NULL the function will allocate enough memory depending on the MBC. It
 * is up to the caller to later free this data.
 *
 * A non-zero return value is returned in case of error.
 */
int gb_load_cartridge
(
	uint8_t * /* data */,
	gb_cartridge_header * /* header */,
	uint8_t ** /* RAM */,
	size_t * /* RAM size */
) ;

/**
 * gb_print_header_info prints information about the cartridge retrieved from the header.
 */
void gb_print_header_info (gb_cartridge_header /* hdr */) ;

/**
 * Load the memory back controller that the header defines.
 *
 * `RAM` points to the allocated RAM memory that the MBC can write to/switch banks/etc.
 */
int gb_load_mbc (gb_cartridge_header /* h */, uint8_t * /* RAM */) ;

#endif /* _GB_FILE_ */
