#include "gb.h"
#include <stdio.h>

int main (int argc, char** argv)
{
	printf ("Starting Game Boy emulator test\n\n");

	int ret = gb_load (argv[1]);
	if (ret != 0)
	{
		fprintf (stderr, "Aborting!\n");
		return ret;
	}

	printf ("\ntesting a couple of CPU instructions\n");
	// step a frame and see what happens
	gb_step ();

	printf ("ciao!\n");

	return 0;
}
