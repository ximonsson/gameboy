#include "gameboy/file.h"
#include "gameboy/cpu.h"


static uint8_t* ROM;
static uint8_t* RAM;


static int load_mbc (uint8_t mbc)
{
	return 0;
}

int gb_load (const char* file)
{
	int ret = 0;

	gb_cpu_reset ();
	// gb_ppu_reset ();
	// gb_apu_reset ();

	uint8_t mbc;
	if ((ret = gb_load_file (file, &mbc, &ROM, &RAM)) != 0)
		return ret;

	return load_mbc (mbc);
}

void gb_step ()
{
	gb_cpu_step ();
}

void gb_frame ()
{

}

void gb_quit ()
{

}
