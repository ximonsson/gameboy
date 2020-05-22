#include "gameboy/file.h"
#include "gameboy/cpu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define HEADER_LOCATION 0x0100
#define HEADER_SIZE 0x4F

/*
 * Nintendo logo data.
 * When reading the header the gameboy makes sure that the logo @ $0104-$0133 matches the below
 * bitmap. Else there is an error.
 */
#define LOGO_SIZE 48
static const uint8_t LOGO[LOGO_SIZE] =
{
	0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
	0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

/*
 * print_nintendo_logo prints the nintendo logo to stdout from the buffer pointed at by logo.
 * this function is used to make sure the header is loaded correctly.
 */
static void print_nintendo_logo (uint8_t* logo)
{
	uint8_t bitmap[LOGO_SIZE << 3];
	int w = 12;
	int h = 8;
	int bw = LOGO_SIZE;

	for (int i = 0; i < LOGO_SIZE; i ++)
	{
		uint8_t b = logo[i];
		int scanln = (i / 24) * 2 ;
		if (i & 1)
			scanln ++;
		int square = ((i & 0xFE) / 2) % w;

		for (int r = 0; r < 2; r ++)
		{
			for (int j = 0; j < 4; j ++)
			{
				int dot = square * 4 + j;
				int x = (scanln * 2 + r) * bw + dot;

				if (b & 0x80)
					bitmap[x] = '@';
				else
					bitmap[x] = ' ';

				b <<= 1;
			}
		}
	}

	for (int i = 0; i < h; i ++)
	{
		for (int j = 0; j < bw; j ++)
			printf ("%c", bitmap[i * bw + j]);
		printf ("\n");
	}

	printf ("\n");
}

static int validate_checksum (uint8_t* header)
{
	int x = 0;
	for (int i = 0x34; i < 0x4D; i ++)
		x = x - header[i] - 1;
	return header[0x4D] == (x & 0xFF);
}

static const char* MBC[0x100] =
{
	"ROM ONLY",
	"MBC1",
	"MBC1+RAM",
	"MBC1+RAM+BATTERY",
	"0x04 unsupported",
	"MBC2",
	"MBC2+BATTERY",
	"0x07 unsupported",
	"ROM+RAM",
	"ROM+RAM+BATTERY",
	"0x0A unsupported",
	"MMM01",
	"MMM01+RAM",
	"MMM01+RAM+BATTERY",
	"0x0E unsupported",
	"MBC3+TIMER+BATTERY",
	"MBC3+TIMER+RAM+BATTERY",
	"MBC3",
	"MBC3+RAM",
	"MBC3+RAM+BATTERY",
	"0x14 Unsupported",
	"MBC4",
	"MBC4+RAM",
	"MBC4+RAM+BATTERY",
	"0x18 Unsupported",
	"MBC5",
	"MBC5+RAM",
	"MBC5+RAM+BATTERY",
	"MBC5+RUMBLE",
	"MBC5+RUMBLE+RAM",
	"MBC5+RUMBLE+RAM+BATTERY",
	"0x21 Unsupported",
	"MBC6",
	"MBC7+SENSOR+RUMBLE+RAM+BATTERY",
	// 0x23 -> 0xFB unsupported TODO
	"POCKET CAMERA",
	"BANDAI TAMA5",
	"HuC3",
	"HuC1+RAM+BATTERY"
};

static int read_header (FILE* fp, uint8_t* mbc, size_t* rom, size_t* ram)
{
	uint8_t header[HEADER_SIZE];
	fseek (fp, HEADER_LOCATION, SEEK_SET);
	fread (header, 1, HEADER_SIZE, fp);

	// print the logo to stdout to make sure all is g00d
	print_nintendo_logo (&header[4]);

	// compare logo bytes to make sure they are correct
	if (memcmp (header + 4, LOGO, LOGO_SIZE) != 0)
	{
		fprintf (stderr, "Logos do not match!\n");
		return 1;
	}

	if (validate_checksum (header) != 1)
	{
		fprintf (stderr, "checksum does not match!\n");
		return 1;
	}

	// print title of the cartridge
	char title[16];
	memcpy (title, header + 0x34, 16); title[15] = 0;
	printf ("TITLE                > %s\n", title);

	// CGB support
	printf ("SUPPORT              > ");
	switch (header[0x43])
	{
	case 0x80:
		printf ("CGB Support\n");
		break;
	case 0xC0:
		printf ("CGB _ONLY_ cartdridge\n");
		break;
	default:
		printf ("Ordinary GB cartdridge\n");
		break;
	}

	// SGB support
	printf ("SGB SUPPORT          > ");
	switch (header[0x46])
	{
	case 0x03:
		printf ("SGB Support\n");
		break;
	default:
		printf ("Normal Gameboy or CGB only game\n");
		break;
	}

	// Cartridge type
	* mbc = header[0x47];
	printf ("CARTRIDGE TYPE (MBC) > [0x%.2X] %s\n", *mbc, MBC[*mbc]);

	// ROM size
	* rom = 32 << (10 + header[0x48]);
	printf ("ROM SIZE             > [%u] %lu KB\n",  header[0x48], (* rom) >> 10);

	// RAM size
	* ram = header[0x49];
	printf ("RAM SIZE             > [%lu]", * ram);
	switch (*ram)
	{
		case 0: * ram = 8 << 10; break;
		case 1: * ram = 2 << 10; break;
		case 2: * ram = 8 << 10; break;
		case 3: * ram = 32 << 10; break;
		case 4: * ram = 128 << 10; break;
		case 5: * ram = 64 << 10; break;
	}
	printf (" %ld kB\n", (*ram) >> 10);

	return 0;
}

int gb_load_file (const char* file, uint8_t* mbc, uint8_t** rom, uint8_t** ram)
{
	// open file for reading
	FILE* fp = fopen (file, "rb");
	int ret = 0;
	if (!fp)
	{
		fprintf (stderr, "Failed to open file '%s'\n", file);
		return 1;
	}

	// read the header
	size_t rom_size, ram_size;
	if (read_header (fp, mbc, &rom_size, &ram_size) != 0)
		ret = 1; // faulty header

	fseek (fp, 0, SEEK_SET);

	// load ROM data
	*rom = (uint8_t *) malloc (rom_size);
	ret = fread (*rom, 1, rom_size, fp);
	if (ret != rom_size)
	{
		fprintf (stderr, "Did not manage to read the ROM data! (read only %d B out of %lu) \n", ret, rom_size);
		return 1;
	}
	else ret = 0;

	// allocate RAM
	*ram = (uint8_t *) calloc (ram_size, sizeof (uint8_t));

	fclose (fp);
	return ret;
}
