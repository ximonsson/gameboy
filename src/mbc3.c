#include "gameboy/mbc3.h"
#include "gameboy/cpu.h"
#include <stdio.h>
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

/* ROM bank number. */
static uint8_t rom_bank;

static int write_rom_bank_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x2000 || adr >= 0x4000) return 0;

	rom_bank = v & 0x7f;
	if (rom_bank == 0) rom_bank = 1;
	gb_cpu_load_rom (1, rom + (rom_bank << 14));

	return 1;
}

/* current RAM bank. */
static uint8_t ram_bank;

/* points correctly to address within current RAM bank. */
#define RAM(adr) ram[adr - 0xA000 + (ram_bank << 13)]

/**
 * RTC registers.
 *
 * 08h  RTC S   Seconds   0-59 (0-3Bh)
 * 09h  RTC M   Minutes   0-59 (0-3Bh)
 * 0Ah  RTC H   Hours     0-23 (0-17h)
 * 0Bh  RTC DL  Lower 8 bits of Day Counter (0-FFh)
 * 0Ch  RTC DH  Upper 1 bit of Day Counter, Carry Bit, Halt Flag
 *       Bit 0  Most significant bit of Day Counter (Bit 8)
 *       Bit 6  Halt (0=Active, 1=Stop Timer)
 *       Bit 7  Day Counter Carry Bit (1=Counter Overflow)
 */
static uint8_t rtc[5];
static uint8_t* rtc_;
#define RTC (* rtc_)

static uint8_t flag_read_rtc;

/* Handles writes to $4000 - $5FFF: writing RAM bank or RTC register. */
static int write_ram_bank_h (uint16_t adr, uint8_t v)
{
	if (adr < 0x4000 || adr >= 0x6000)
		return 0;

	v &= 0xF;

	if (v <= 0x3)
	{
		ram_bank = v;
		flag_read_rtc = 0;
	}
	else if (v >= 0x8 && v <= 0xC)
	{
		rtc_ = rtc + (v - 8);
		flag_read_rtc = 1;
	}

	return 1;
}

/* Handles reading from RAM $A000 - $BFFF. */
static int read_ram_h (uint16_t adr, uint8_t* v)
{
	if (adr < 0xA000 || adr > 0xBFFF)
		return 0;

	else if (!RAM_ENABLED)
	{
		*v = 0;
		return 1;
	}

	if (flag_read_rtc)
		*v = RTC;
	else
		*v = RAM (adr);

	return 1;
}

/* Handles writing to RAM $A000 - $BFFF. */
static int write_ram_h (uint16_t adr, uint8_t v)
{
	if (adr < 0xA000 || adr > 0xBFFF)
		return 0;
	else if (!RAM_ENABLED)
		return 1;

	if (flag_read_rtc)
		RTC = v;
	else
		RAM (adr) = v;

	return 1;
}

void gb_mbc3_load (uint8_t* rom_, uint8_t* ram_)
{
	rom = rom_;
	ram = ram_;

	memset (rtc, 0, 5);
	rom_bank = 1;
	ram_bank = 0;
	ram_enabled = 0;
	flag_read_rtc = 0;

	gb_cpu_register_store_handler (write_ram_enable_h);
	gb_cpu_register_store_handler (write_rom_bank_h);
	gb_cpu_register_store_handler (write_ram_bank_h);
	gb_cpu_register_store_handler (write_ram_h);

	gb_cpu_register_read_handler (read_ram_h);
}
