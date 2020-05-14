#include "gameboy/io.h"
#include "gameboy/cpu.h"

static uint8_t* p1;
#define P1 (*p1)

static int write_joypad_h (uint16_t address, uint8_t v)
{
	if (address != GB_IO_P1_LOC) return 0;

	P1 = (P1 & 0xCF) | (v & 0x30);

	return 1;
}

static int read_joypad_h (uint16_t address, uint8_t* v)
{
	if (address != GB_IO_P1_LOC) return 0;

	// TODO add correct state
	*v &= 0x30;
	*v |= 0xCF;

	return 1;
}

void gb_io_reset ()
{
	p1 = gb_cpu_mem (GB_IO_P1_LOC);

	gb_cpu_register_store_handler (write_joypad_h);
	gb_cpu_register_read_handler (read_joypad_h);
}
