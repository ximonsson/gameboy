#include "gameboy/cpu.h"

/*
 * Apparently there are some games that are crazy enough to atempt this and this
 * handler needs to prevent it.
 */
static int write_rom_h (uint16_t addr, uint8_t v)
{
	if (addr < 0x8000) return 1;
	return 0;
}

/**
 *
 */
void gb_mbc0_load (uint8_t* rom, uint8_t* ram)
{
	gb_cpu_register_store_handler (write_rom_h);
}

