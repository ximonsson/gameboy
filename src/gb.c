#include "gameboy/file.h"
#include "gameboy/cpu.h"
#include "gameboy/ppu.h"
#include "gameboy/mbc.h"
#include "gameboy/io.h"
#include <stdlib.h>

static uint8_t* ROM = 0;
static uint8_t* RAM = 0;

static const void (*MBC[0x100]) (uint8_t*, uint8_t*) =
{
	0, //  "ROM ONLY",
	gb_mbc1_load, // "MBC1",
	0, // "MBC1+RAM",
	0, // "MBC1+RAM+BATTERY",
	0, // "0x04 unsupported",
	0, // "MBC2",
	0, // "MBC2+BATTERY",
	0, // "0x07 unsupported",
	0, // "ROM+RAM",
	0, // "ROM+RAM+BATTERY",
	0, // "0x0A unsupported",
	0, // "MMM01",
	0, // "MMM01+RAM",
	0, // "MMM01+RAM+BATTERY",
	0, // "0x0E unsupported",
	0, // "MBC3+TIMER+BATTERY",
	0, // "MBC3+TIMER+RAM+BATTERY",
	0, // "MBC3",
	0, // "MBC3+RAM",
	0, // "MBC3+RAM+BATTERY",
	0, // "0x14 Unsupported",
	0, // "MBC4",
	0, // "MBC4+RAM",
	0, // "MBC4+RAM+BATTERY",
	0, // "0x18 Unsupported",
	0, // "MBC5",
	0, // "MBC5+RAM",
	0, // "MBC5+RAM+BATTERY",
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
