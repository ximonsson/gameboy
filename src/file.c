#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
	uint8_t bitmap[8 * LOGO_SIZE];
	int w = 12;
	int h = 8;
	int bw = LOGO_SIZE * 8 / h;

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
					bitmap[x] = '0';
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

	printf ("\n\n");
}

static int read_header (FILE* fp)
{
	fseek (fp, HEADER_LOCATION, SEEK_SET);
	uint8_t header[HEADER_SIZE];
	fread (header, 1, HEADER_SIZE, fp);

	// print the logo to stdout to make sure all is g00d
	if (memcmp (header + 4, LOGO, LOGO_SIZE) != 0)
	{
		fprintf (stderr, "Logos do not match!\n");
		return 1;
	}
	print_nintendo_logo (&header[4]);

	// print title of the cartridge
	char title[16];
	memcpy (title, header + 0x34, 16);
	printf ("%s\n", title);

	// CGB support
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
	switch (header[0x46])
	{
	case 0x03:
		printf ("SGB Support\n");
		break;
	default:
		printf ("Normal Gameboy or CGB only game\n");
		break;
	}
}

int gb_load_file (const char* file)
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
	if (read_header (fp) != 0)
		ret = 1; // faulty header

	// load PRG and CHR
	fclose (fp);
	return ret;
}
