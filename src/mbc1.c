/**
 * Implementation of MBC1.
 */
#include "gameboy/mbc1.h"
#include "gameboy/cpu.h"
#include <string.h>

uint8_t* ROM;
uint8_t* RAM;
uint8_t* cpu_ram;

/* RAM enabled register. */
static uint8_t ram_enabled;
#define RAM_ENABLED ((ram_enabled & 0x0A) == 0x0A)

static int write_ram_enable_h (uint16_t address, uint8_t v)
{
	if (address > 0x1FFF) return 0;
	ram_enabled = v;
	return 1;
}

/* ROM/RAM mode select. */
static uint8_t select_mode;
#define ROM_SELECT_MODE (select_mode == 0)
#define RAM_SELECT_MODE (select_mode == 1)

static uint8_t bank_lo;
static uint8_t bank_hi;

static void reload_banks ()
{
	if (ROM_SELECT_MODE)
	{
		uint32_t b = (bank_hi << 4 | bank_lo) << 14;
		memcpy (cpu_ram + 0x4000, ROM + b, ROM_BANK_SIZE);
	}
	else
	{
		memcpy (cpu_ram + 0x4000, ROM + (bank_lo << 14), ROM_BANK_SIZE);
		memcpy (cpu_ram + 0xD000, RAM + (bank_hi << 12), RAM_BANK_SIZE);
	}
}

static int write_select_mode_h (uint16_t adr, uint8_t v)
{
	if (!(adr >= 0x6000 && adr < 0x8000)) return 0;
	select_mode = v & 1;
	reload_banks ();
	return 1;
}

static int write_bank_number_h (uint16_t adr, uint8_t v)
{
	if (adr >= 0x2000 && adr < 0x4000)
	{
		// low ROM bank
		bank_lo = (v & 0xF) | 1;
		reload_banks ();
		return 1;
	}
	else if (adr >= 0x4000 && adr < 0x6000)
	{
		// high ROM bank or RAM
		v &= 0x3;
		bank_hi = v;
		reload_banks ();
		return 1;
	}
	return 0;
}

void gb_mbc1_load (uint8_t* rom, uint8_t* ram)
{
	ROM = rom;
	RAM = ram;

	cpu_ram = gb_cpu_mem (0);

	gb_cpu_register_store_handler (write_ram_enable_h);
	gb_cpu_register_store_handler (write_bank_number_h);
	gb_cpu_register_store_handler (write_select_mode_h);
}
