#include "gameboy/ppu.h"
#include "gameboy/cpu.h"
#include "gb.h"
#include <string.h>

#ifdef DEBUG_PPU
#include <stdio.h>
#endif

/* Dot counter within frame. */
static uint32_t dot;

/* Registers --------------------------------------------------- */
static uint8_t* scy_;
static uint8_t* scx_;
static uint8_t* ly_;
static uint8_t* lyc_;
static uint8_t* wy_;
static uint8_t* wx_;
static uint8_t* bgp_;
static uint8_t* obp0_;
static uint8_t* obp1_;

#define scy (* scy_)
#define scx (* scx_)
#define ly (* ly_)
#define lyc (* lyc_)
#define wy (* wy_)
#define wx (* wx_)
#define bgp (* bgp_)
#define obp0 (* obp0_)
#define obp1 (* obp1_)

#define LY_LOC 0xFF44

/* LY register is read only. */
static int write_ly_h (uint16_t adr, uint8_t v)
{
	return adr == LY_LOC;
}

/**
 * LCD Control register
 *
 * Bit 7 - LCD Display Enable             (0=Off, 1=On)
 * Bit 6 - Window Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
 * Bit 5 - Window Display Enable          (0=Off, 1=On)
 * Bit 4 - BG & Window Tile Data Select   (0=8800-97FF, 1=8000-8FFF)
 * Bit 3 - BG Tile Map Display Select     (0=9800-9BFF, 1=9C00-9FFF)
 * Bit 2 - OBJ (Sprite) Size              (0=8x8, 1=8x16)
 * Bit 1 - OBJ (Sprite) Display Enable    (0=Off, 1=On)
 * Bit 0 - BG/Window Display/Priority     (0=Off, 1=On)
 */

#define LCDC_LOC 0xFF40
static uint8_t* lcdc_;
#define lcdc (* lcdc_)

#define LCD_ENABLED ((lcdc & 0x80) == 0x80)
#define WIN_TILE_MAP (0x1800 | ((lcdc & 0x40) << 4))
#define WIN_DISP_ENABLED ((lcdc & 0x20) == 0x20)
#define BG_WIN_TILE (0x0800 & ~((lcdc & 0x10) << 7))
#define BG_TILE_MAP (0x1800 | ((lcdc & 0x08) << 7))
#define OBJ_SIZE (8 + ((lcdc & 0x04) << 1))
#define OBJ_ENABLED (lcdc & 0x02)
#define BG_WIN_PRIO (lcdc & 0x01)

/**
 * LCD Status register
 *
 * Bit 6 - LYC=LY Coincidence Interrupt (1=Enable) (Read/Write)
 * Bit 5 - Mode 2 OAM Interrupt         (1=Enable) (Read/Write)
 * Bit 4 - Mode 1 V-Blank Interrupt     (1=Enable) (Read/Write)
 * Bit 3 - Mode 0 H-Blank Interrupt     (1=Enable) (Read/Write)
 * Bit 2 - Coincidence Flag  (0:LYC<>LY, 1:LYC=LY) (Read Only)
 * Bit 1-0 - Mode Flag       (Mode 0-3, see below) (Read Only)
 *           0: During H-Blank
 *           1: During V-Blank
 *           2: During Searching OAM
 *           3: During Transferring Data to LCD Driver
 */

#define STATUS_LOC 0xFF41
static uint8_t* status_;
#define STATUS (* status_)

#define LYC_EQ_LQ_FLAG 0x04

#define LYC_EQ_LY_INT ((STATUS & 0x40) == 0x40)
#define MODE_2_OAM_INT ((STATUS & 0x20) == 0x20)
#define MODE_1_VBLANK_INT ((STATUS & 0x10) == 0x10)
#define MODE_0_HBLANK_INT ((STATUS & 0x08) == 0x08)
#define LYC_EQ_LY ((STATUS & LYC_EQ_LQ_FLAG) == LYC_EQ_LQ_FLAG)

#define MODE (STATUS & 0x03)
#define MODE_HBLANK 0
#define MODE_VBLANK 1
#define MODE_SEARCH_OAM 2
#define MODE_TRANSFER_LCD 3

#define SET_MODE(x) { STATUS = (STATUS & 0xFC) | x; }

static int write_status_h (uint16_t addr, uint8_t v)
{
	if (addr != STATUS_LOC) return 0;

	// do not overwrite mode bits and LY=LYC
	STATUS = (v & 0xF8) | (STATUS & 0x07);

	return 1;
}

static int write_lcdc_h (uint16_t adr, uint8_t v)
{
	if (adr != LCDC_LOC) return 0;

	lcdc = v;
	if (!LCD_ENABLED)
	{
#ifdef DEBUG_PPU
		printf (" >>> LCD disabled\n");
#endif
		dot = ly = 0;
		SET_MODE (MODE_VBLANK);
	}
	return 1;
}

/* OAM data pointer */
static uint8_t* oam;

#define SPRITE_BG_PRIO(sprite) ((sprite[3] & 0x80) == 0x80)
#define SPRITE_YFLIP(sprite) ((sprite[3] & 0x40) == 0x40)
#define SPRITE_XFLIP(sprite) ((sprite[3] & 0x20) == 0x20)
#define SPRITE_PALETTE(sprite) ((sprite[3] & 0x10) >> 4)
#define SPRITE_VRAM(sprite) ((sprite[3] & 0x08) >> 3)
#define SPRITE_PALETTE_CGB(sprite) (sprite[3] & 0x07)

#define NPIXELS 69120 // width x height x rgb

/* switchable screen buffer for rendering. */
static uint8_t lcd_buffer[NPIXELS];
static uint8_t lcd[NPIXELS];

const uint8_t* gb_ppu_lcd () { return lcd; }

/* VRAM */
static uint8_t* vram;

/* monochrome palett. */
static const uint8_t SHADES [4][3] =
{
	{ 0xFF, 0xFF, 0xFF },
	{ 0xAA, 0xAA, 0xAA },
	{ 0x55, 0x55, 0x55 },
	{ 0x00, 0x00, 0x00 },
};

/* get color within tile @ (x, y). */
static uint8_t color (uint8_t* tile, uint8_t x, uint8_t y)
{
	uint8_t shift = 7 - x;
	y <<= 1; // y mul 2

	uint8_t lsb = (tile[y] >> shift) & 1;
	uint8_t msb = (tile[y | 1] >> shift) & 1;

	uint8_t c = (msb << 1) | lsb; // color value 0-3
	return c;
}

/* get color within sprite. */
static uint8_t color_sprite (uint8_t n, uint8_t x, uint8_t y)
{
	if (OBJ_SIZE == 16) n &= 0xFE;
	return color (vram + (n << 4), x, y);
}

/* get color within background. */
static uint8_t color_bg (uint8_t n, uint8_t x, uint8_t y)
{
	static int16_t offset;
	if (BG_WIN_TILE == 0x0800) // $8800 addressing mode
		offset = 0x1000 + ((int8_t) n << 4);
	else offset = n << 4;

	return color (vram + offset, x, y);
}

#define LCD_COLOR(x, y, c, pal) { \
	const uint8_t* rgb = SHADES[(pal >> (c << 1)) & 0x3];\
	memcpy (&lcd_buffer[(y * GB_LCD_WIDTH + x) * 3], rgb, 3);\
}

static uint8_t draw_bg (uint8_t x, uint8_t y)
{
	// read BG tile map
	uint8_t bgx = (x + scx), bgy = (y + scy);

	uint16_t t = (bgx >> 3) + ((bgy & ~0x7) << 2); // (x div 8) + (y div 8) * 32

	// pointer to BG tile map
	uint8_t tn = *(vram + BG_TILE_MAP + t);

	uint8_t c = color_bg (tn, bgx & 0x7, bgy & 0x7);
	LCD_COLOR (x, y, c, bgp);

	return c;
}

static void draw_win (uint8_t x, uint8_t y)
{
	uint8_t winx = x - (wx - 7), winy = y - wy;

	uint16_t t = (winx >> 3) + ((winy & ~0x7) << 2);
	uint8_t tn = *(vram + WIN_TILE_MAP + t);

	uint8_t c = color_bg (tn, winx & 0x7, winy & 0x7);
	LCD_COLOR (x, y, c, bgp);
}

#define SPRITES_PER_LINE 10

/* Indices of the sprites that are visible on this line. */
static uint8_t line_sprites[SPRITES_PER_LINE];

#define RESET_LINE_SPRITES memset (line_sprites, 0xFF, SPRITES_PER_LINE)

static void draw_obj (uint8_t x, uint8_t y, uint8_t bgc)
{
	static uint8_t* sprite;
	static int16_t dx;
	static uint8_t dy, c;

	for (int i = 0; i < SPRITES_PER_LINE && line_sprites[i] != 0xFF; i ++)
	{
		sprite = oam + (line_sprites[i] << 2);

		if (SPRITE_BG_PRIO (sprite) && bgc) continue;

		dx = x - sprite[1] + 8;

		if (dx >= 0 && dx < 8)
		{
			dy = ly - sprite[0] + 16;

			if (SPRITE_XFLIP (sprite))
				dx = 7 - dx;

			if (SPRITE_YFLIP (sprite))
				dy = OBJ_SIZE - 1 - dy;

			c = color_sprite (sprite[2], dx, dy);
			if (c)
			{
				uint8_t pal = *(obp0_ + SPRITE_PALETTE (sprite));
				LCD_COLOR (x, y, c, pal);
				break;
			}
		}
	}
}

/* draw in the LCD at position x, y. */
static void draw (uint8_t x, uint8_t y)
{
	uint8_t bgc = 0;
	// Background
	if (BG_WIN_PRIO)
	{
		// BG
		bgc = draw_bg (x, y);
		// WIN
		if (WIN_DISP_ENABLED && x >= (wx - 7) && y >= wy)
			draw_win (x, y);
	}
	// Sprite
	if (OBJ_ENABLED)
		draw_obj (x, y, bgc);
}

/* Find (the first 10) sprites that are visible on the current line. */
static void inline find_line_sprites ()
{
	static uint8_t* sprite;
	static uint8_t x, y;

	RESET_LINE_SPRITES;
	for (uint8_t i = 0, n = 0; i < 40 && n < SPRITES_PER_LINE; i ++)
	{
		// every 4 B is a sprite
		sprite = oam + (i << 2);

		y = sprite[0];
		x = sprite[1];

		// hidden sprite (outside of screen)?
		if (y == 0 || y >= (GB_LCD_HEIGHT + 16) || x == 0 || x >= (GB_LCD_WIDTH + 8))
			continue;

		// visible sprite on the current line
		int16_t dy = ly - (y - 16);
		if (dy < OBJ_SIZE && dy >= 0)
			line_sprites[n ++] = i;
	}
}

#define OAM_CC 80

/* step the PPU one dot. */
static void step ()
{
	if (!LCD_ENABLED) return;

	// which dot on the current line
	int16_t x = dot % GB_SCANLINE;

	// LYC=LY
	if (lyc == ly && x == 0)
	{
		STATUS |= LYC_EQ_LQ_FLAG;
		if (LYC_EQ_LY_INT)
			gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
	}
	else if (LYC_EQ_LY && lyc != ly)
		STATUS &= ~LYC_EQ_LQ_FLAG;

	// V-BLANK
	if (ly == GB_LCD_HEIGHT && x == 0)
	{
		gb_cpu_flag_interrupt (INT_FLAG_VBLANK);
		SET_MODE (MODE_VBLANK);
		if (MODE_1_VBLANK_INT)
			gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
		// Transfer data to LCD
		memcpy (lcd, lcd_buffer, NPIXELS);
	}
	// Visible line
	else if (ly < GB_LCD_HEIGHT)
	{
		x -= OAM_CC;

		// OAM search
		if (x == -OAM_CC)
		{
			// Load sprites on this line.
			SET_MODE (MODE_SEARCH_OAM);
			if (MODE_2_OAM_INT)
				gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
			find_line_sprites ();
		}
		// H-BLANK
		else if (x == GB_LCD_WIDTH)
		{
			SET_MODE (MODE_HBLANK);
			if (MODE_0_HBLANK_INT)
				gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
		}
		// Draw
		else if (x >= 0 && x < GB_LCD_WIDTH)
		{
			if (x == 0)
				SET_MODE (MODE_TRANSFER_LCD);
			draw (x, ly);
		}
	}

	// step the dot counter and line
	dot ++;
	dot %= GB_FRAME;
	ly = dot / GB_SCANLINE;
}

void gb_ppu_reset ()
{
	lcdc_   = gb_cpu_mem (LCDC_LOC);
	status_ = gb_cpu_mem (STATUS_LOC);
	scy_    = gb_cpu_mem (0xFF42);
	scx_    = gb_cpu_mem (0xFF43);
	ly_     = gb_cpu_mem (0xFF44);
	lyc_    = gb_cpu_mem (0xFF45);
	wy_     = gb_cpu_mem (0xFF4A);
	wx_     = gb_cpu_mem (0xFF4B);
	bgp_    = gb_cpu_mem (0xFF47);
	obp0_   = gb_cpu_mem (0xFF48);
	obp1_   = gb_cpu_mem (0xFF49);

	lcdc = 0x91; // NOTE a lot of games do not set the LCD enabled when starting....
	bgp = 0xFC;
	obp0 = obp1 = 0xFF;

	vram = gb_cpu_mem (VRAM_LOC);
	oam = gb_cpu_mem (OAM_LOC);

	dot = ly = 0;

	RESET_LINE_SPRITES;

	gb_cpu_register_store_handler (write_status_h);
	gb_cpu_register_store_handler (write_lcdc_h);
	gb_cpu_register_store_handler (write_ly_h);

	memset (lcd_buffer, 0, NPIXELS);
	memset (lcd, 0, NPIXELS);
}

void gb_ppu_step (int cc)
{
	for (; cc > 0; cc --) step ();
}

#ifdef DEBUG_PPU
static inline void print_tile (uint8_t* t)
{
	uint8_t c;
	for (uint8_t y = 0; y < 8; y ++)
	{
		for (uint8_t x = 0; x < 8; x ++)
		{
			c = color (t, x, y);
			switch (c)
			{
				case 0: printf (" "); break;
				case 1: printf ("░"); break;
				case 2: printf ("▒"); break;
				case 3: printf ("█"); break;
				default: printf ("%d", c); break;
			}
		}
		printf ("\n");
	}
	printf ("\n");
}

static inline void print_sprite (uint8_t s)
{
	uint8_t* sprite = oam + (s << 2);

	printf (" SPRITE @ (%d - 8 [%d], %d - 16 [%d])\n", sprite[1], sprite[1] - 8, sprite[0], sprite[0] - 16);
	printf (" > Tile N: %d\n", sprite[2]);
	printf (" > BG prio: %d\n", SPRITE_BG_PRIO (sprite));
	printf (" > Y-flip: %d\n", SPRITE_YFLIP (sprite));
	printf (" > X-flip: %d\n", SPRITE_XFLIP (sprite));
	printf (" > Palette: %d\n", SPRITE_PALETTE (sprite));
	printf (" > VRAM: %d\n", SPRITE_VRAM (sprite));
	printf (" > Palette (CGB): %d\n", SPRITE_PALETTE_CGB (sprite));
	printf ("\n");

	uint8_t* tile = vram + (sprite[2] << 4);
	print_tile (tile);
}

static inline void print_oam ()
{
	for (int i = 0; i < 40; i ++)
		print_sprite (i);
}

static inline void print_bg (uint8_t bg)
{

}
#endif
