#include "gameboy/ppu.h"
#include "gameboy/cpu.h"
#include "gb.h"
#include <string.h>

#ifdef DEBUG
#include<stdio.h>
#endif

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
#define WIN_TILE_MAP (0x9800 | ((lcdc & 0x40) << 4))
#define WIN_DISP_ENABLED ((lcdc & 0x20) == 0x20)
#define BG_WIN_TILE (0x8800 & ~((lcdc & 0x10) << 7))
#define BG_TILE_MAP (0x9800 | ((lcdc & 0x80) << 7))
#define OBJ_SIZE (lcdc & 0x04)
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
#define status (* status_)

#define LYC_EQ_LY_INT ((status & 0x40) == 0x40)
#define MODE_2_OAM_INT ((status & 0x20) == 0x20)
#define MODE_1_VBLANK_INT ((status & 0x10) == 0x10)
#define MODE_0_HBLANK_INT ((status & 0x08) == 0x08)
#define LYC_EQ_LY ((status & 0x04) == 0x04)

#define MODE (status & 0x03)
#define MODE_HBLANK MODE == 0
#define MODE_VBLANK MODE == 1
#define MODE_SEARCH_OAM MODE == 2
#define MODE_TRANSFER_LCD MODE == 3

#define SET_MODE(x) { status &= 0xFC; status |= x; }

/* OAM data pointer */
static uint8_t* oam;

/* switchable screen buffer for rendering. */
static uint8_t screen_buffer1_[GB_LCD_WIDTH * GB_LCD_HEIGHT * 3];
static uint8_t screen_buffer2_[GB_LCD_WIDTH * GB_LCD_HEIGHT * 3];
static uint8_t* lcd;

const uint8_t* gb_ppu_lcd () { return lcd; }

uint8_t* vram;

static uint32_t dot = 0;

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

	lcd = screen_buffer1_;
	vram = gb_cpu_mem (VRAM_LOC);
	oam = gb_cpu_mem (OAM_LOC);
	dot = 0;
}

/* monochrome palett. */
const uint8_t SHADES [4][3] =
{
	{ 0xFF, 0xFF, 0xFF },
	{ 0xAA, 0xAA, 0xAA },
	{ 0x55, 0x55, 0x55 },
	{ 0x00, 0x00, 0x00 },
};


static void print_sprite (uint8_t* spr)
{

}

static void draw_obj (uint8_t x, uint8_t y)
{

}

static void draw_win (uint8_t x, uint8_t y)
{

}

static void draw_bg (uint8_t x, uint8_t y)
{
	// pointer to BG tile map
	uint8_t* tp = vram + (BG_TILE_MAP - 0x8000);

	// read BG tile map
	uint8_t bgx = x + scx; uint8_t bgy = y + scy;
	// TODO wrap around
	uint16_t t = (bgx & 0xF8) + ((bgy & 0xF8) << 5); // (x - x mod 8) + (y - y mod 8) * 32
	uint8_t b = tp[t];

	// get tile
	uint16_t tn = b;
	if (BG_WIN_TILE == 0x8800)
		tn = 0x1000 + ((int8_t) b);
	uint8_t* tile = vram + tn;

	// get color within tile
	uint8_t tx = x & 0x7, ty = y & 0x7;
	uint8_t msb = tile[ty << 1]; // y mod 8 mul 2
	uint8_t lsb = tile[(ty << 1) | 1]; // y mod 8 mul 2 + 1
	uint8_t c = ((msb >> (tx - 1)) | (lsb >> tx)) & 0x3; // color value 0-3
	const uint8_t* rgb = SHADES[(bgp >> (c << 1)) & 0x3];

	memcpy(&lcd[(y * GB_LCD_WIDTH + x) * 3], rgb, 3);
}

/* draw in the LCD at position x, y. */
static void draw (uint8_t x, uint8_t y)
{
	// BG
	draw_bg (x, y);

	// WIN
	if (WIN_DISP_ENABLED)
	{
		draw_win (x, y);
	}

	// Sprite
	if (OBJ_ENABLED)
	{
		draw_obj (x, y);
	}
}

/* step the PPU one dot. */
static void step ()
{
	// TODO: if we write to LY this will overwrite it. not good
	ly = dot / GB_SCANLINE;
	uint16_t x = dot % GB_SCANLINE;

	// LYC=LY
	if (lyc == ly)
	{
		status |= 0x04;
		if (x == 0 && LYC_EQ_LY_INT)
			gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
	}
	else status &= ~0x04;

	// VBLANK
	if (ly == 144 && x == 0) {
		SET_MODE (MODE_VBLANK);

		if (MODE_1_VBLANK_INT)
		{
			gb_cpu_flag_interrupt (INT_FLAG_VBLANK);
		}
	}
	else if (ly < 144)
	{
		if (x == 0)
		{
			SET_MODE (MODE_SEARCH_OAM);
		}
		else if (x == 80)
		{
			SET_MODE (MODE_TRANSFER_LCD);
		}
		else if (x > GB_LCD_WIDTH + 80)
		{
			SET_MODE (MODE_HBLANK);
		}
	}

	if (LCD_ENABLED && ly < GB_LCD_HEIGHT && (x - 80) < GB_LCD_WIDTH && (x - 80) >= 0)
	{
		draw (x - 80, ly);
	}

	dot ++; dot %= GB_FRAME;
}

void gb_ppu_step (int cc)
{
	for (int i = 0; i < cc; i ++) step ();
}
