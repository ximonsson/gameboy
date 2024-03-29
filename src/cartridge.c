#include "gb/cartridge.h"
#include "gb/cpu.h"
#include "gb/mbc.h"
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

/**
 * validate_checksum of the header to make sure it is valid.
 */
static int validate_checksum (const uint8_t* header)
{
	int x = 0;
	for (int i = 0x34; i < 0x4D; i ++)
		x = x - header[i] - 1;
	return header[0x4D] == (x & 0xFF);
}

/**
 * Read header from the ROM data (cartridge).
 */
static int read_header (const uint8_t *rom, gb_cartridge_header *hdr)
{
	// point to header
	const uint8_t *header = rom + GB_HEADER_LOCATION;

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

/*
 * print_nintendo_logo prints the nintendo logo to stdout from the buffer pointed at by logo.
 * this function is used to make sure the header is loaded correctly.
 */
static void print_nintendo_logo (const uint8_t* logo)
{
	int bitmap[GB_HEADER_LOGO_SIZE << 3];
	int w = 12;
	int h = 8;
	int bw = GB_HEADER_LOGO_SIZE;


	for (int i = 0; i < GB_HEADER_LOGO_SIZE; i ++)
	{
		uint8_t b = logo[i];
		int scanln = (i / 24) * 2 ;
		if (i & 1) scanln ++;
		int square = ((i & 0xFE) / 2) % w;

		for (int r = 0; r < 2; r ++)
		{
			for (int j = 0; j < 4; j ++)
			{
				int dot = square * 4 + j;
				int x = (scanln * 2 + r) * bw + dot;

				if (b & 0x80)
					bitmap[x] = '#';
				else
					bitmap[x] = ' ';

				b <<= 1;
			}
		}
	}

	printf ("\n");
	for (int i = 0; i < h; i ++)
	{
		for (int j = 0; j < bw; j ++)
			printf ("%c", bitmap[i * bw + j]);
		printf ("\n");
	}

	printf ("\n");
}

void gb_print_header_info (gb_cartridge_header h)
{
	const char* MBC[0x100] =
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
	printf ("%-10s > %s\n", "TITLE", h.title);
	printf ("%-10s > %s\n", "MBC", MBC[h.mbc]);
	printf ("%-10s > %-3d x 16KB\n", "ROM", h.rom_size);
	printf ("%-10s > %-3d x 8KB\n", "RAM", h.ram_size);
	printf ("%-10s > %s\n", "CGB", h.cgb == 0x80 ? "CGB SUPPORT" : h.cgb == 0xC0 ? "CGB _ONLY_" : "DMG");
	printf ("%-10s > %s\n", "SGB", h.sgb == 0x03 ? "YES" : "NO");
	printf ("\n");
}

int gb_load_mbc (gb_cartridge_header h, uint8_t *ram)
{
	const void (*MBC[0x100]) (uint8_t*) =
	{
		gb_mbc0_load, // "ROM ONLY",
		gb_mbc1_load, // "MBC1",
		gb_mbc1_load, // "MBC1+RAM",
		gb_mbc1_load, // "MBC1+RAM+BATTERY",
		0, // "0x04 unsupported",
		gb_mbc2_load, // "MBC2",
		gb_mbc2_load, // "MBC2+BATTERY",
		0, // "0x07 unsupported",
		gb_mbc0_load, // "ROM+RAM",
		gb_mbc0_load, // "ROM+RAM+BATTERY",
		0, // "0x0A unsupported",
		0, // "MMM01",
		0, // "MMM01+RAM",
		0, // "MMM01+RAM+BATTERY",
		0, // "0x0E unsupported",
		gb_mbc3_load, // "MBC3+TIMER+BATTERY",
		gb_mbc3_load, // "MBC3+TIMER+RAM+BATTERY",
		gb_mbc3_load, // "MBC3",
		gb_mbc3_load, // "MBC3+RAM",
		gb_mbc3_load, // "MBC3+RAM+BATTERY",
		0, // "0x14 Unsupported",
		0, // "MBC4",
		0, // "MBC4+RAM",
		0, // "MBC4+RAM+BATTERY",
		0, // "0x18 Unsupported",
		gb_mbc5_load, // "MBC5",
		gb_mbc5_load, // "MBC5+RAM",
		gb_mbc5_load, // "MBC5+RAM+BATTERY",
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

	mbc_loader ld = MBC[h.mbc];
	if (!ld)
	{
		fprintf (stderr, "MBC [%.2X] not supported\n", h.mbc);
		return 1;
	}

	ld (ram);
	return 0;
}

int gb_load_cartridge
(
	const uint8_t *data,
	gb_cartridge_header *h,
	uint8_t **ram,
	size_t *ram_size
)
{
	// read header
	if (read_header (data, h) != 0) return 1;

	*ram_size = h->ram_size * RAM_BANK_SIZE;

	// if no previous RAM we allocate new
	// otherwise we expect that the RAM is all good and continue
	if (!*ram)
	{
		*ram = malloc (*ram_size);
		// NOTE
		// i have no documentation that this is important but some games require it
		// i can not remember where i saw this first.
		memset (*ram, 0xFF, *ram_size);
	}

	// load MBC
	//return gb_load_mbc (*h, *ram);
	return 0;
}
