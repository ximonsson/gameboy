#include "gameboy/mbc3.h"
#include "gameboy/cpu.h"

static uint8_t* ROM;
static uint8_t* RAM;

/* RAM enabled register. */
static uint8_t ram_enabled;
#define RAM_ENABLED ((ram_enabled & 0x0A) == 0x0A)

static int write_ram_enable_h (uint16_t address, uint8_t v)
{
	if (address > 0x1FFF) return 0;
	ram_enabled = v;
	return 1;
}

/* ROM bank number. */
static uint8_t rom_bank;

static int write_bank_h (uint16_t address, uint8_t v)
{
	if (address < 0x2000 || address >= 0x4000) return 0;

	rom_bank = v & 0x7f;
	gb_cpu_load_rom (1, ROM + (rom_bank << 14));

	return 1;
}

/* current RAM bank. */
static uint8_t ram_bank;

static int write_ram_bank_h (uint16_t address, uint8_t v)
{
	if (address < 0x4000 || address >= 0x6000) return 0;

	if (v & 0x3)
	{
		ram_bank = v & 0x3;
		gb_cpu_load_ram (RAM + (ram_bank << 12));
	}

	return 1;
}

void gb_mbc3_load (uint8_t* rom, uint8_t* ram)
{
	ROM = rom;
	RAM = ram;

	gb_cpu_register_store_handler (write_ram_enable_h);
	gb_cpu_register_store_handler (write_bank_h);
}
