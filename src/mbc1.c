/**
 * Implementation of MBC1.
 */
#include "gameboy/mbc1.h"
#include "gameboy/cpu.h"
#include <string.h>

static uint8_t* rom;
static uint8_t* ram;

/* RAM enabled register. */
static uint8_t ram_enabled;
#define RAM_ENABLED ((ram_enabled & 0x0A) == 0x0A)

static int write_ram_enable_h (uint16_t adr, uint8_t v)
{
	if (adr > 0x1FFF) return 0;
	ram_enabled = v;
	return 1;
}

/* ROM/RAM mode select. */
static uint8_t select_mode;
#define ROM_SELECT_MODE (select_mode == 0)
#define RAM_SELECT_MODE (select_mode == 1)

static uint8_t bank_lo;
static uint8_t bank_hi;
static uint8_t bank_ram;

/* points correctly to address within current RAM bank. */
#define RAM(adr) ram[adr - 0xA000 + (bank_ram << 13)]

static void reload_banks ()
{
	uint32_t b;

	if (ROM_SELECT_MODE)
	{
		b = (bank_hi << 5) | bank_lo;
		bank_ram = 0;
	}
	else // RAM_SELECT_MODE
	{
		b = bank_lo;
		bank_ram = bank_hi;
	}

	gb_cpu_load_rom (1, rom + (b << 14));
}

static int write_select_mode_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x6000 || adr >= 0x8000) return 0;
	select_mode = v & 1;
	reload_banks ();
	return 1;
}

static int write_bank_number_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x2000 || adr >= 0x6000)
		return 0; // not trying to change bank #
	else if (adr < 0x4000)
	{
		bank_lo = v & 0x1F;
		if (bank_lo == 0) bank_lo = 1; // can't choose ROM bank 00h
	}
	else bank_hi = v & 0x03;

	reload_banks ();

	return 1;
}

/* Handles reading from RAM $A000 - $BFFF. */
static int read_ram_h (uint16_t adr, uint8_t* v)
{
	if (adr < 0xA000 || adr >= 0xC000) return 0;
	*v = RAM_ENABLED ? RAM (adr) : 0;
	return 1;
}

/* Handles writing to RAM $A000 - $BFFF. */
static int write_ram_h (uint16_t adr, uint8_t v)
{
	if (adr < 0xA000 || adr > 0xBFFF) return 0;
	else if (RAM_ENABLED) RAM (adr) = v;
	return 1;
}

void gb_mbc1_load (uint8_t* rom_, uint8_t* ram_)
{
	rom = rom_;
	ram = ram_;

	bank_hi = 0;
	bank_lo = 1;
	bank_ram = 0;
	ram_enabled = 0;
	select_mode = 0;

	gb_cpu_register_store_handler (write_ram_enable_h);
	gb_cpu_register_store_handler (write_bank_number_h);
	gb_cpu_register_store_handler (write_select_mode_h);
	gb_cpu_register_store_handler (write_ram_h);

	gb_cpu_register_read_handler (read_ram_h);
}
