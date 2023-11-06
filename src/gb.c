#include "gb/cartridge.h"
#include "gb/cpu.h"
#include "gb/ppu.h"
#include "gb/mbc.h"
#include "gb/io.h"
#include "gb/apu.h"
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

int gb_load (const char *file, uint8_t *ram)
{
	int ret = 0;

	// read file
	// TODO
	// i think this should be done before calling this function.
	// this function should take the read data from a particular source.

	FILE* fp = fopen (file, "rb");
	gb_file_header h;

	if (!fp)
	{
		fprintf (stderr, "Failed to open file '%s'\n", file);
		ret = 1;
		goto end;
	}
	else if ((ret = gb_load_cartridge (fp, &h, &ROM)) != 0)
		goto end;

	gb_print_header_info (h);

	// allocate RAM
	if (ram)
	{
		printf ("battery backed RAM supplied\n");
		RAM = ram;
	}
	else
	{
		RAM = (uint8_t *) malloc (h.ram_size * RAM_BANK_SIZE);
		memset (RAM, 0xFF, h.ram_size * RAM_BANK_SIZE);
	}
	// reset all units
	gb_cpu_reset ();
	gb_ppu_reset ();
	gb_io_reset ();
	gb_apu_reset (sample_rate);

	if ((ret = gb_load_mbc (h, RAM)) != 0)
		goto end;


	// load ROM
	// TODO
	// i think this should be part of the reset instead.
	gb_cpu_load_rom (h.rom_size, ROM);

end:
	return ret;
}

void gb_press_button (gb_button b) { gb_io_press_button (b); }

void gb_release_button (gb_button b) { gb_io_release_button (b); }

const uint8_t* gb_lcd () { return gb_ppu_lcd (); }

void gb_audio_samples (float* buf, size_t* n) { gb_apu_samples (buf, n); }

uint32_t gb_step (uint32_t ccs)
{
	static uint32_t cpucc = 0;
	uint32_t prev_cpucc = cpucc, cc = 0;

	while (cpucc < ccs)
	{
		// step all units
		cc = gb_cpu_step ();
		gb_ppu_step (cc);
		gb_apu_step (cc);

		cpucc += cc;
	}

	uint32_t ret = cpucc - prev_cpucc;  // number of cycles that ran
	cpucc -= ccs;

	return ret;
}

void gb_stop ()
{
	free (ROM);
	free (RAM);
}

void gb_quit () { }
