#include "gameboy/file.h"
#include "gameboy/cpu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Nintendo logo data.
 * When reading the header the gameboy makes sure that the logo @ $0104-$0133 matches the below
 * bitmap. Else there is an error.
 */
static const uint8_t LOGO[GB_HEADER_LOGO_SIZE] =
{
	0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
	0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
	0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

/*
 * print_nintendo_logo prints the nintendo logo to stdout from the buffer pointed at by logo.
 * this function is used to make sure the header is loaded correctly.
 */
static void print_nintendo_logo (const uint8_t* logo)
{
	uint8_t bitmap[GB_HEADER_LOGO_SIZE << 3];
	int w = 12;
	int h = 8;
	int bw = GB_HEADER_LOGO_SIZE;

	for (int i = 0; i < GB_HEADER_LOGO_SIZE; i ++)
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

/**
 * validate_checksum of the header to make sure it is valid.
 */
static int validate_checksum (uint8_t* header)
{
	int x = 0;
	for (int i = 0x34; i < 0x4D; i ++)
		x = x - header[i] - 1;
	return header[0x4D] == (x & 0xFF);
}

static inline int load_header (uint8_t* rom, gb_file_header* hdr)
{
	// point to header
	uint8_t* header = rom + GB_HEADER_LOCATION;

	// copy logo from cartridge
	// and compare to correct logo, making sure they are correct
	memcpy (hdr->logo, header + 4, GB_HEADER_LOGO_SIZE);
	if (memcmp (hdr->logo, LOGO, GB_HEADER_LOGO_SIZE) != 0)
	{
		fprintf (stderr, "Logos do not match!\n");
		return 1;
	}

	// validate checksum
	if (validate_checksum (header) != 1)
	{
		fprintf (stderr, "Checksum does not match!\n");
		return 1;
	}

	// copy title
	memcpy (hdr->title, header + 0x34, GB_HEADER_TITLE_SIZE);
	hdr->title[GB_HEADER_TITLE_SIZE - 1] = 0; // make sure it is null-terminated

	// support flags
	hdr->cgb = header[0x43];
	hdr->sgb = header[0x46];
	hdr->mbc = header[0x47];

	// ROM size in 8KB banks
	// TODO
	// there are some special cases that are not covered here - one day will probably have to deal with them.
	hdr->rom_size = 1 << (header[0x48] + 1);

	// RAM size in 4KB banks
	switch (header[0x49])
	{
		case 0: hdr->ram_size = 0; break;
		case 1: hdr->ram_size = 1; break;
		case 2: hdr->ram_size = 1; break;
		case 3: hdr->ram_size = 4; break;
		case 4: hdr->ram_size = 16; break;
		case 5: hdr->ram_size = 8; break;
	}

	return 0;
}

int gb_load_file (FILE* fp, gb_file_header* hdr, uint8_t** rom)
{
	int ret = 0;

	// load ROM data

	fseek (fp, 0, SEEK_END);
	size_t rom_size = ftell (fp);
	*rom = (uint8_t *) malloc (rom_size);

	fseek (fp, 0, SEEK_SET);
	ret = fread (*rom, 1, rom_size, fp);
	if (ret != rom_size)
	{
		fprintf (stderr, "Did not manage to read the ROM data! (read only %d B out of %lu) \n", ret, rom_size);
		ret = 1;
		goto end;
	}
	else ret = 0;

	// load header
	ret = load_header (*rom, hdr);
	if (ret != 0)
		goto end;

end:
	return ret;
}

void gb_print_header_info (gb_file_header h)
{
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

	print_nintendo_logo (h.logo);
	printf ("%-15s > %s\n", "TITLE", h.title);
	printf ("%-15s > %s\n", "MBC", MBC[h.mbc]);
	printf ("%-15s > %-3d x 8KB\n", "ROM", h.rom_size);
	printf ("%-15s > %-3d x 4KB\n", "RAM", h.ram_size);
	printf ("%-15s > %s\n", "CGB", h.cgb == 0x80 ? "CGB SUPPORT" : h.cgb == 0xC0 ? "CGB _ONLY_" : "DMG");
	printf ("%-15s > %s\n", "SGB", h.sgb == 0x03 ? "YES" : "NO");
}
