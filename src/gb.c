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

static int sample_rate;

static int n_step_cbs;
static void (*step_cbs[5]) (uint32_t);

void gb_add_step_callback (void (*cb) (uint32_t))
{
	step_cbs [n_step_cbs ++] = cb;
}

void gb_init (int sample_rate_)
{
	sample_rate = sample_rate_;
}

int gb_load (const uint8_t *ROM, uint8_t **RAM, size_t *ram_size)
{
	n_step_cbs = 0;

	gb_cartridge_header h;
	if (gb_load_cartridge (ROM, &h, RAM, ram_size) != 0) return 1;

	gb_print_header_info (h);
	uint8_t cgb = h.cgb == 0x80 || h.cgb == 0xC0;

	if (cgb)
	{
		printf ("GB > Running emulator in CGB mode.\n");
	}

	// reset all units
	gb_cpu_reset (!cgb);
	gb_ppu_reset (!cgb);
	gb_io_reset ();
	gb_apu_reset (sample_rate);

	gb_load_mbc (h, *RAM);

	// load ROM
	// TODO
	// i think this should be part of the reset instead.
	gb_cpu_load_rom (h.rom_size, ROM);

	return 0;
}

void gb_press_button (gb_button b) { gb_io_press_button (b); }

void gb_release_button (gb_button b) { gb_io_release_button (b); }

const uint16_t *gb_lcd () { return gb_ppu_lcd (); }

void gb_audio_samples (float *buf, size_t *n) { gb_apu_samples (buf, n); }

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

		for (int i = 0; i < n_step_cbs; i ++)
			step_cbs[i] (cc);

		cpucc += cc;
	}

	uint32_t ret = cpucc - prev_cpucc;  // number of cycles that ran
	cpucc -= ccs;

	return ret;
}

void gb_quit () { }  // nada
