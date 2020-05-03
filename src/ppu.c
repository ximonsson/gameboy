#include "gameboy/ppu.h"
#include "gameboy/cpu.h"

/* Registers --------------------------------------------------- */
static uint8_t* scy_;
static uint8_t* scx_;
static uint8_t* ly_;
static uint8_t* lyc_;
static uint8_t* wy_;
static uint8_t* bgp_;
static uint8_t* obp0_;
static uint8_t* obp1_;

#define scy (* scy_)
#define scx (* scx_)
#define ly (* ly_)
#define lyc (* lyc_)
#define wy (* wy_)
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

static uint8_t lcdc;

#define LCD_ENABLED ((lcdc & 0x80) == 0x80)
#define WIN_TILE_MAP (lcdc & 0x40)
#define WIN_DISP_ENABLED ((lcdc & 0x20) == 0x20)
#define BG_WIN_TILE (lcdc & 0x10)
#define BG_TILE_MAP (lcdc & 0x08)
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

#define LYC_EQ_LY_INT ((status & 0x40) == 0x60)
#define MODE_2_OAM_INT ((status & 0x20) == 0x40)
#define MODE_1_VBLANK_INT ((status & 0x10) == 0x20)
#define MODE_0_HBLANK_INT ((status & 0x08) == 0x10)
#define LYC_EQ_LY ((status & 0x04) == 0x04)

#define STATUS (status & 0x03)
#define MODE_HBLANK STATUS == 0
#define MODE_VBLANK STATUS == 1
#define MODE_SEARCH_OAM STATUS == 2
#define MODE_TRANSFER_LCD STATUS == 3

/* switchable screen buffer for rendering. */
static uint8_t screen_buffer1_[GB_FRAME];
static uint8_t screen_buffer2_[GB_FRAME];
static uint8_t* lcd;

const uint8_t* gb_ppu_lcd () { return lcd; }

uint8_t* vram;

void gb_ppu_reset ()
{
	status_ = gb_cpu_mem (STATUS_LOC);
	scy_    = gb_cpu_mem (0xFF42);
	scx_    = gb_cpu_mem (0xFF43);
	ly_     = gb_cpu_mem (0xFF44);
	lyc_    = gb_cpu_mem (0xFF45);
	wy_     = gb_cpu_mem (0xFF4A);
	bgp_    = gb_cpu_mem (0xFF47);
	obp0_   = gb_cpu_mem (0xFF48);
	obp1_   = gb_cpu_mem (0xFF49);

	lcd = screen_buffer1_;
	vram = gb_cpu_mem (VRAM_LOC);
}

static uint16_t dot = 0;

static void step ()
{
	dot ++; dot += GB_FRAME;

	scx ++;

	if (lyc == ly)
	{
		status |= 0x04;
		gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
	}

	ly %= GB_SCANLINE;

	// VBLANK
	if (ly == 144) {
		status &= 0xFC;
		status |= 0x01;
		if (MODE_1_VBLANK_INT)
			gb_cpu_flag_interrupt (INT_FLAG_VBLANK);
	}

}

void gb_ppu_step (int cc)
{
	for (int i = 0; i < cc; i ++) step ();
}
