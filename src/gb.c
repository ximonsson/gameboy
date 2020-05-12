#include "gameboy/file.h"
#include "gameboy/cpu.h"
#include "gameboy/ppu.h"
#include "gameboy/mbc.h"
#include "gameboy/io.h"
#include <stdlib.h>

static uint8_t* ROM = 0;
static uint8_t* RAM = 0;

static int load_mbc (uint8_t mbc)
{
	return 0;
}

int gb_load (const char* file)
{
	int ret = 0;

	gb_cpu_reset ();
	gb_ppu_reset ();
	gb_io_reset ();
	// gb_apu_reset ();

	uint8_t mbc;
	if ((ret = gb_load_file (file, &mbc, &ROM, &RAM)) != 0)
		return ret;

	gb_cpu_load_rom (0, ROM);
	gb_cpu_load_rom (1, ROM + 0x4000);

	return load_mbc (mbc);
}

void gb_step ()
{
	for (int cpucc = 0; cpucc < GB_FRAME; )
	{
		int cc = gb_cpu_step ();
		gb_ppu_step (cc);
		cpucc += cc;
	}
}

const uint8_t* gb_lcd () { return gb_ppu_lcd (); }

void gb_stop ()
{
	free (ROM);
	free (RAM);
}

void gb_quit ()
{

}
