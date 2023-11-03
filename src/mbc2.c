/**
 * Implementation of MBC2.
 */
#include "gb/mbc2.h"
#include "gb/cpu.h"

static uint8_t* ram;

#define RAM(a) ram[a - 0xA000]

/* RAM enabled register. */
static uint8_t ram_enabled;
#define RAM_ENABLED ((ram_enabled & 0x0A) == 0x0A)

static int write_ram_enable_h (uint16_t adr, uint8_t v)
{
	if (adr > 0x1FFF || (adr & 0x0100)) // LSB of upper byte of the address must be cleared
		return 0;
	ram_enabled = v;
	return 1;
}

static int write_ram_h (uint16_t adr, uint8_t v)
{
	if (adr < 0xA000 || adr > 0xA1FF)
		return 0;

	if (RAM_ENABLED)
		RAM(adr) = v & 0x0F;

	return 1;
}

static int read_ram_h (uint16_t adr, uint8_t* v)
{
	if (adr < 0xA000 || adr > 0xA1FF)
		return 0;

	if (!RAM_ENABLED)
		*v = 0;
	else
		*v = RAM (adr);

	return 1;
}

static int write_bank_number_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x2000 || adr > 0x3FFF)
		return 0;
	else if (!(adr & 0x0100)) // LSB of upper byte must be set
		return 0;

	uint8_t b = v & 0x0F;
	gb_cpu_switch_rom_bank (b);
	return 1;
}

void gb_mbc2_load (uint8_t* ram_)
{
	ram = ram_;

	ram_enabled = 0;

	gb_cpu_register_store_handler (write_ram_enable_h);
	gb_cpu_register_store_handler (write_bank_number_h);
	gb_cpu_register_store_handler (write_ram_h);

	gb_cpu_register_read_handler (read_ram_h);
}
