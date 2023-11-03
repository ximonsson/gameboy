/**
 * Implementation of MBC5.
 */
#include "gb/mbc5.h"
#include "gb/cpu.h"
#include <string.h>

static uint8_t* ram;

/* RAM enabled register. */
static uint8_t ram_enabled;
#define RAM_ENABLED ((ram_enabled & 0x0A) == 0x0A)

static int write_ram_enable_h (uint16_t address, uint8_t v)
{
	if (address > 0x1FFF) return 0;
	ram_enabled = v;
	return 1;
}

/* ROM and RAM bank numbers. */
static uint8_t bank_rom_lo;
static uint8_t bank_rom_hi;
static uint8_t bank_ram;

static int write_bank_number_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x2000 || adr >= 0x3FFF) return 0;

	// low
	if (adr <= 0x2FFF)
		bank_rom_lo = v;
	// high
	else
		bank_rom_hi = v & 1;

	uint32_t b = ((bank_rom_hi << 8) | bank_rom_lo);
	gb_cpu_switch_rom_bank (b);

	return 1;
}

static int write_ram_bank_number_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x4000 || adr > 0x5FFF) return 0;

	bank_ram = v & 0x0F;
	return 1;
}

/* Handles reading from RAM $A000 - $BFFF. */
static int read_ram_h (uint16_t address, uint8_t* v)
{
	if (address < 0xA000 || address > 0xBFFF) return 0;

	address = address - 0xA000 + (bank_ram << 13);
	*v = ram[address];
	return 1;
}

/* Handles writing to RAM $A000 - $BFFF. */
static int write_ram_h (uint16_t address, uint8_t v)
{
	if (address < 0xA000 || address > 0xBFFF) return 0;

	address = address - 0xA000 + (bank_ram << 13);
	ram[address] = v;
	return 1;
}

void gb_mbc5_load (uint8_t* ram_)
{
	ram = ram_;

	bank_rom_hi = 0;
	bank_rom_lo = 0;
	bank_ram = 0;

	gb_cpu_register_store_handler (write_ram_enable_h);
	gb_cpu_register_store_handler (write_bank_number_h);
	gb_cpu_register_store_handler (write_ram_bank_number_h);
	gb_cpu_register_store_handler (write_ram_h);

	gb_cpu_register_read_handler (read_ram_h);
}
