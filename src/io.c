#include "gameboy/io.h"
#include "gameboy/cpu.h"

static uint8_t* p1;
#define P1 (*p1)

#define BTN_KEYS (P1 & 0x20)
#define DIR_KEYS (P1 & 0x10)

/* State for each key set as : DOWN | UP | LEFT | RIGHT | START | SELECT | B | A */
static uint8_t key_states;

static int write_joypad_h (uint16_t address, uint8_t v)
{
	if (address != GB_IO_P1_LOC) return 0;

	P1 = (P1 & 0xCF) | (v & 0x30);

	return 1;
}

static int read_joypad_h (uint16_t address, uint8_t* v)
{
	if (address != GB_IO_P1_LOC) return 0;

	if (BTN_KEYS)
		*v = P1 | (key_states & 0xF);
	else // DIR_KEYS
		*v = P1 | (key_states >> 4);

	return 1;
}

void gb_io_press_button (gb_io_button b)
{
	key_states &= ~(1 << b);
	gb_cpu_flag_interrupt (INT_FLAG_JOYPAD);
}

void gb_io_release_button (gb_io_button b)
{
	key_states |= 1 << b;
}

void gb_io_reset ()
{
	p1 = gb_cpu_mem (GB_IO_P1_LOC);

	key_states = 0xFF;

	gb_cpu_register_store_handler (write_joypad_h);
	gb_cpu_register_read_handler (read_joypad_h);
}
