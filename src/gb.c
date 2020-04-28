#include "gameboy/file.h"
#include "gameboy/cpu.h"


int gb_load (const char* file)
{
	return gb_load_file (file);
}

void gb_step ()
{
	gb_cpu_step ();
}

void gb_quit ()
{

}
