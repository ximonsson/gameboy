#include "gameboy/file.h"
#include "gameboy/cpu.h"
#include "gameboy/ppu.h"
#include "gameboy/mbc.h"
#include "gameboy/io.h"
#include "gameboy/apu.h"
#include "gb.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static uint8_t* ROM;
static uint8_t* RAM;

static int sample_rate;

void gb_init (int sample_rate_)
{
	sample_rate = sample_rate_;
}

/**
 * Map MBC identifiers to loader functions.
 */
static const void (*MBC[0x100]) (uint8_t*) =
{
	gb_mbc0_load, // "ROM ONLY",
	gb_mbc1_load, // "MBC1",
	gb_mbc1_load, // "MBC1+RAM",
	gb_mbc1_load, // "MBC1+RAM+BATTERY",
	0, // "0x04 unsupported",
	gb_mbc2_load, // "MBC2",
	gb_mbc2_load, // "MBC2+BATTERY",
	0, // "0x07 unsupported",
	gb_mbc0_load, // "ROM+RAM",
	gb_mbc0_load, // "ROM+RAM+BATTERY",
	0, // "0x0A unsupported",
	0, // "MMM01",
	0, // "MMM01+RAM",
	0, // "MMM01+RAM+BATTERY",
	0, // "0x0E unsupported",
	gb_mbc3_load, // "MBC3+TIMER+BATTERY",
	gb_mbc3_load, // "MBC3+TIMER+RAM+BATTERY",
	gb_mbc3_load, // "MBC3",
	gb_mbc3_load, // "MBC3+RAM",
	gb_mbc3_load, // "MBC3+RAM+BATTERY",
	0, // "0x14 Unsupported",
	0, // "MBC4",
	0, // "MBC4+RAM",
	0, // "MBC4+RAM+BATTERY",
	0, // "0x18 Unsupported",
	gb_mbc5_load, // "MBC5",
	gb_mbc5_load, // "MBC5+RAM",
	gb_mbc5_load, // "MBC5+RAM+BATTERY",
	0, // "MBC5+RUMBLE",
	0, // "MBC5+RUMBLE+RAM",
	0, // "MBC5+RUMBLE+RAM+BATTERY",
	0, // "0x21 Unsupported",
	0, // "MBC6",
	0, // "MBC7+SENSOR+RUMBLE+RAM+BATTERY",
	// 0x23 -> 0xFB unsupported TODO
	0, // "POCKET CAMERA",
	0, // "BANDAI TAMA5",
	0, // "HuC3",
	0, // "HuC1+RAM+BATTERY"
};

static int load_mbc (uint8_t mbc)
{
	mbc_loader ld = MBC[mbc];
	if (!ld)
	{
		fprintf (stderr, "MBC [%.2X] not supported\n", mbc);
		return 1;
	}
	ld (RAM);
	return 0;
}

int gb_load (const char* file)
{
	int ret = 0;

	// read file

	FILE* fp = fopen (file, "rb");
	gb_file_header h;

	if (!fp)
	{
		fprintf (stderr, "Failed to open file '%s'\n", file);
		return 1;
	}
	else if ((ret = gb_load_file (fp, &h, &ROM)) != 0)
		return ret;

	gb_print_header_info (h);

	// allocate RAM
	RAM = (uint8_t *) malloc (h.ram_size << 12);
	memset (RAM, 0xFF, h.ram_size << 12);

	// reset all units
	gb_cpu_reset ();
	gb_ppu_reset ();
	gb_io_reset ();
	gb_apu_reset (sample_rate);

	// load ROM
	gb_cpu_load_rom (h.rom_size, ROM);

	// load MBC
	return load_mbc (h.mbc);
}

void gb_press_button (gb_button b) { gb_io_press_button (b); }

void gb_release_button (gb_button b) { gb_io_release_button (b); }

void gb_step ()
{
	int cc;
	for (int cpucc = 0; cpucc < GB_FRAME; )
	{
		cc = gb_cpu_step ();

		gb_ppu_step (cc);
		gb_apu_step (cc);

		cpucc += cc;
	}
}

const uint8_t* gb_lcd () { return gb_ppu_lcd (); }

void gb_audio_samples (float* buf, size_t* n) { gb_apu_samples (buf, n); }

void gb_stop ()
{
	free (ROM);
	free (RAM);
}

void gb_quit () { }
