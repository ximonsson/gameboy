#include <stdio.h>
#include <stdint.h>

int gb_load_file (const char* file)
{
	FILE* fp = fopen (file, "rb");
	int ret = 0;
	if (!fp)
	{
		fprintf (stderr, "Failed to open file '%s'\n", file);
		return 1;
	}

	uint8_t buf[8 << 10]; // 8KiB
	int n = fread (buf, 1, 0x147, fp);
	if (n < 0x147)
	{
		fprintf (stderr, "Did not get entire file\n");
		ret = 1;
		goto end;
	}

	uint8_t mapper = buf[0x147];
	printf ("mapper = %.2X\n", mapper);

end:
	fclose (fp);
	return ret;
}

int main (int argc, char** argv)
{
	//gb_load_file ("/home/ximon/mnt/drive/games/roms/gb/megaman-dr.willys-revenge.gb");
	gb_load_file ("/home/ximon/mnt/drive/games/roms/gb/pokemon_yellow.gb");
	return 0;
}
