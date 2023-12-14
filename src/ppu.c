#include "gb/ppu.h"
#include "gb/cpu.h"
#include "gb.h"
#include <string.h>

//#ifdef DEBUG_PPU
#include <stdio.h>
//#endif

/* Dot counter within scanline. */
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

#define SCY (* scy_)
#define SCX (* scx_)
#define LY (* ly_)
#define LYC (* lyc_)
#define WY (* wy_)
#define WX (* wx_)
#define BGP (* bgp_)
#define OBP0 (* obp0_)
#define OBP1 (* obp1_)

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
#define LCDC (* lcdc_)

#define LCD_ENABLED (LCDC & 0x80)
#define WIN_TILE_MAP (0x1800 | ((LCDC & 0x40) << 4))
#define WIN_DISP_ENABLED (LCDC & 0x20)
#define BG_WIN_TILE (LCDC & 0x10)
#define BG_TILE_MAP (0x1800 | ((LCDC & 0x08) << 7))
#define OBJ_SIZE (8 + ((LCDC & 0x04) << 1))
#define OBJ_ENABLED (LCDC & 0x02)
#define BG_WIN_PRIO (LCDC & 0x01)

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
static uint8_t *status_;
#define STATUS (*status_)

#define LYC_EQ_LQ_FLAG 0x04

#define LYC_EQ_LY_INT (STATUS & 0x40)
#define MODE_2_OAM_INT (STATUS & 0x20)
#define MODE_1_VBLANK_INT (STATUS & 0x10)
#define MODE_0_HBLANK_INT (STATUS & 0x08)
#define LYC_EQ_LY (STATUS & LYC_EQ_LQ_FLAG)

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

#ifdef DEBUG_PPU
	uint8_t tmp = LCDC;
	if (!LCD_ENABLED)
	{
		LCDC = v;
		if (LCD_ENABLED)
			printf (" PPU > LCD enabled\n");
		LCDC = tmp;
	}
#endif

	LCDC = v;
	if (!LCD_ENABLED)
	{
#ifdef DEBUG_PPU
		printf (" PPU > LCD disabled\n");
#endif
		dot = LY = 0;
		SET_MODE (MODE_HBLANK);
	}
	return 1;
}

/**
 * Block any illegal writes because of PPU mode.
 */
static int write_mode_block (uint16_t addr, uint8_t v)
{
	// no access to OAM while in MODE 2
	if ((MODE == MODE_SEARCH_OAM) && ((addr >= 0xFE00) && (addr <= 0xFE9F)))
		return 1;
	// no access to PPU during MODE 3
	else if (MODE == MODE_TRANSFER_LCD)
	{
		if ((addr >= 0x8000) && (addr <= 0x9FFF))
			return 1;
		else if ((addr >= 0xFE00) && (addr <= 0xFE9F))
			return 1;
	}

	return 0;
}

/**
 * Block reads to VRAM and OAM when not accessible.
 */
static int read_mode_block (uint16_t addr, uint8_t *v)
{
	// no access to OAM while in MODE 2
	if ((MODE == MODE_SEARCH_OAM) && ((addr >= 0xFE00) && (addr <= 0xFE9F)))
	{
		*v = 0xFF;
		return 1;
	}
	// no access to PPU during MODE 3
	else if (MODE == MODE_TRANSFER_LCD)
	{
		if ((addr >= 0x8000) && (addr <= 0x9FFF))
		{
			*v = 0xFF;
			return 1;
		}
		else if ((addr >= 0xFE00) && (addr <= 0xFE9F))
		{
			*v = 0xFF;
			return 1;
		}
	}

	return 0;
}

/* OAM data pointer */
static uint8_t* oam;

#define SPRITE_BG_PRIO(sprite) (sprite[3] & 0x80)
#define SPRITE_YFLIP(sprite) (sprite[3] & 0x40)
#define SPRITE_XFLIP(sprite) (sprite[3] & 0x20)
#define SPRITE_PALETTE(sprite) ((sprite[3] & 0x10) >> 4)
#define SPRITE_VRAM(sprite) ((sprite[3] & 0x08) >> 3)
#define SPRITE_PALETTE_CGB(sprite) (sprite[3] & 0x07)

#define NPIXELS 23040  // width * height

/* switchable screen buffer for rendering. */
static uint16_t *lcd, *lcd_buf;
static uint16_t __lcd_1[NPIXELS];
static uint16_t __lcd_2[NPIXELS];


const uint16_t *gb_ppu_lcd () { return lcd; }

/* VRAM */
static uint8_t* vram;

/* VRAM banks */
static uint8_t *vram_bank0;
static uint8_t vram_bank1[0x2000];

static uint8_t *_vbk;
#define VBK (*_vbk)
#define VBK_LOC 0xFF4F

static int write_vbk_handler (uint16_t adr, uint8_t v)
{
	if (adr != VBK_LOC) return 0;

	if (v & 1) vram = vram_bank1;
	else vram = vram_bank0;

	VBK = 0xFE | (v & 1);

	return 1;
}

static uint8_t CRAM_BG[64];

static uint8_t *_bcps;
#define BCPS (*_bcps)
#define BCPS_LOC 0xFF68

//static uint8_t *_bcpd;
//#define BCPD (*_bcpd)
#define BCPD_LOC 0xFF69

static int write_bcpd_handler (uint16_t adr, uint8_t v)
{
	if (adr != BCPD_LOC) return 0;

	CRAM_BG[BCPS & 0x3F] = v;

	// auto increment
	if (BCPS & 0x80)
		BCPS = 0x80 | (((BCPS & 0x3F) + 1) & 0x3F);

	return 1;
}

static uint8_t CRAM_OBJ[64];

static uint8_t *_ocps;
#define OCPS (*_ocps)
#define OCPS_LOC 0xFF68

//static uint8_t *_ocpd;
//#define OCPD (*_ocpd)
#define OCPD_LOC 0xFF69

static int write_ocpd_handler (uint16_t adr, uint8_t v)
{
	if (adr != OCPD_LOC) return 0;

	CRAM_OBJ[OCPS & 0x3F] = v;

	// auto increment
	if (OCPS & 0x80)
		OCPS = 0x80 | (((OCPS & 0x3F) + 1) & 0x3F);

	return 1;
}

/* monochrome palett. */
static const uint16_t SHADES[4] = { 0xFFFF, 0xAD6A, 0x294A, 0x0000 };

// 2 bits / color, so shift the palette 2 × the color index right to
// put the desired color in the 2 least significant bits
#define SHADE(c, pal) SHADES[(pal >> (c << 1)) & 0x03]

/* get color @ (x, y) within 8x8 tile. */
static uint8_t color_tile (uint8_t *tile, uint8_t x, uint8_t y)
{
	uint8_t shift = 7 - x;
	y <<= 1; // y mul 2

	uint8_t lsb = (tile[y] >> shift) & 1;
	uint8_t msb = (tile[y | 1] >> shift) & 1;

	return (msb << 1) | lsb; // color value 0-3
}

/* get color within background BG tile. */
static inline uint8_t color_bg_tile (uint8_t n, uint8_t x, uint8_t y)
{
	if (!BG_WIN_TILE) // $8800 addressing mode
		return color_tile (vram + 0x1000 + ((int8_t) n << 4), x, y);
	else
		return color_tile (vram + (n << 4), x, y);
}

/**
 * Draw BG pixel @ x,y in LCD.
 */
static inline uint8_t color_bg (uint8_t x)
{
	// BG X and Y viewport, the uint8_t type makes sure to wrap around 255.
	uint8_t bgx = (x + SCX), bgy = (LY + SCY);

	// determine BG tile index in 32x32 tile map.
	// (x div 8) + (y div 8) * 32
	uint16_t _t = (bgx >> 3) + ((bgy & 0xF8) << 2);

	// pointer to BG tile map
	uint8_t t = vram[BG_TILE_MAP + _t];

	return color_bg_tile (t, bgx & 0x7, bgy & 0x7);
}

static inline uint8_t color_win (uint8_t x)
{
	uint8_t winx = x - (WX - 7), winy = LY - WY;

	uint16_t t = (winx >> 3) + ((winy & 0xF8) << 2);
	uint8_t tn = vram[WIN_TILE_MAP + t];

	return color_bg_tile (tn, winx & 0x7, winy & 0x7);
}

#define SPRITES_PER_LINE 10

/* Indices of the sprites that are visible on this line. */
static uint8_t line_sprites[SPRITES_PER_LINE + 1];

#define RESET_LINE_SPRITES memset (line_sprites, 0xFF, SPRITES_PER_LINE + 1);

static inline uint16_t __color_obj_dmg (uint8_t *sprite, uint8_t ti, uint8_t dx, uint8_t dy)
{
	uint8_t oc = color_tile (vram + (ti << 4), dx, dy);
	return SHADE (sprite[3] & 0x10 ? OBP1 : OBP0, oc);
}

static inline uint16_t __color_obj_cgb (uint8_t *sprite, uint8_t ti, uint8_t dx, uint8_t dy)
{
	uint8_t oc = color_tile
	(
		(sprite[3] & 0x08 ? vram_bank1 : vram_bank0) + (ti << 4),
		dx,
		dy
	);

	// 4 colors / palette × 2 B / colors = every 8 B
	return CRAM_OBJ[(SPRITE_PALETTE_CGB(sprite) << 3) + (oc & 0x3)];
}

static uint16_t (*__color_obj) (uint8_t *, uint8_t, uint8_t, uint8_t);

static inline void color_obj (uint8_t x, uint16_t *c, uint16_t bgc)
{
	uint8_t *sprite;
	int16_t dx;
	uint8_t dy, ti;
	uint16_t oc;

	for (uint8_t *s = line_sprites; (*s) != 0xFF; s ++)
	{
		sprite = oam + ((*s) << 2);

		// background priority
		if (SPRITE_BG_PRIO (sprite) && bgc) continue;

		dx = x - sprite[1] + 8;

		if (dx >= 0 && dx < 8)
		{
			dy = LY - sprite[0] + 16;

			// flip
			if (SPRITE_XFLIP (sprite))
				dx = 7 - dx;
			if (SPRITE_YFLIP (sprite))
				dy = OBJ_SIZE - 1 - dy;

			// tile
			ti = sprite[2]; if (OBJ_SIZE == 16) ti &= 0xFE;

			// tile color
			oc = __color_obj (sprite, ti, dx, dy);

			if (oc)
			{
				*c = oc;
				break;
			}
		}
	}
}

/* Find (the first 10) sprites that are visible on the current line. */
static inline void find_line_sprites ()
{
	uint8_t x, y, *sprite;

	RESET_LINE_SPRITES
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
		int16_t dy = LY - (y - 16);
		if (dy < OBJ_SIZE && dy >= 0)
			line_sprites[n ++] = i;
	}
}

void gb_ppu_stall (uint32_t cc)
{
	dot += cc;
	if (dot >= GB_SCANLINE) dot %= GB_FRAME;
}

#define OAM_CC 80

static inline void draw_dmg (uint16_t x)
{
	uint16_t bgc = 0, c = 0;

	// Background
	if (BG_WIN_PRIO)
	{
		// BG
		c = bgc = color_bg (x);
		// WIN
		if (WIN_DISP_ENABLED && (x >= (WX - 7)) && (LY >= WY))
			c = color_win (x);
	}
	// Sprite
	if (OBJ_ENABLED)
		color_obj (x, &c, bgc);

	lcd_buf[LY * GB_LCD_WIDTH + x] = c;
}

static inline void draw_cgb (uint16_t x)
{
	uint16_t bgc = 0, c = 0;

	// TODO
	// implement correct priorities

	// Background
	/*
	if (BG_WIN_PRIO)
	{
		// BG
		c = bgc = color_bg (x);
		// WIN
		if (WIN_DISP_ENABLED && (x >= (WX - 7)) && (LY >= WY))
			c = color_win (x);
	}
	*/
	// Sprite
	if (OBJ_ENABLED)
		color_obj (x, &c, bgc);

	lcd_buf[LY * GB_LCD_WIDTH + x] = c;
}

static void (*draw) (uint16_t);

/* step the PPU one dot. */
static inline void step ()
{
	if (!LCD_ENABLED) return;

	// start of new scanline.
	if (dot == 0)
	{
		// LYC=LY
		if (LYC == LY)
		{
			STATUS |= LYC_EQ_LQ_FLAG;
			if (LYC_EQ_LY_INT)
				gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
		}
		// OAM search
		if (LY < GB_LCD_HEIGHT)
		{
			SET_MODE (MODE_SEARCH_OAM);
			if (MODE_2_OAM_INT)
				gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
			find_line_sprites ();
		}
		// V-BLANK
		else if (LY == GB_LCD_HEIGHT)
		{
			gb_cpu_flag_interrupt (INT_FLAG_VBLANK);
			SET_MODE (MODE_VBLANK);
			if (MODE_1_VBLANK_INT)
				gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);

			// Transfer data to LCD
			if (lcd == __lcd_1)
			{
				lcd = __lcd_2; lcd_buf = __lcd_1;
			}
			else
			{
				lcd = __lcd_1; lcd_buf = __lcd_2;
			}
		}
	}
	// else - if we are on a visible line and past mode 2
	else if (LY < GB_LCD_HEIGHT && dot >= OAM_CC)
	{
		uint16_t x = dot - OAM_CC;

		if (x == 0) SET_MODE (MODE_TRANSFER_LCD);

		if (x < GB_LCD_WIDTH) draw (x);
		// H-BLANK
		else if (x == (GB_LCD_WIDTH + 12))
		{
			SET_MODE (MODE_HBLANK);
			if (MODE_0_HBLANK_INT)
				gb_cpu_flag_interrupt (INT_FLAG_LCD_STAT);
		}
	}

	// step the dot counter and line
	dot ++;
	if (dot >= GB_SCANLINE)
	{
		dot -= GB_SCANLINE;
		LY ++; if (LY == GB_SCANLINES) LY = 0;
		if (LYC_EQ_LY && LYC != LY) STATUS &= ~LYC_EQ_LQ_FLAG;
	}
}

void gb_ppu_step (uint32_t cc)
{
	for (; cc > 0; cc --) step ();
}

void gb_ppu_reset (uint8_t dmg)
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

	LCDC = 0x91; // NOTE a lot of games do not set the LCD enabled when starting....
	BGP = 0xFC;
	OBP0 = OBP1 = 0xFF;

	vram = gb_cpu_mem (VRAM_LOC);
	oam = gb_cpu_mem (OAM_LOC);

	dot = LY = 0;

	RESET_LINE_SPRITES

	gb_cpu_register_read_handler (read_mode_block);

	gb_cpu_register_store_handler (write_mode_block);
	gb_cpu_register_store_handler (write_status_h);
	gb_cpu_register_store_handler (write_lcdc_h);
	gb_cpu_register_store_handler (write_ly_h);

	if (!dmg)
	{
		_vbk = gb_cpu_mem (VBK_LOC);
		_ocps = gb_cpu_mem (OCPS_LOC);
		_bcps = gb_cpu_mem (BCPS_LOC);
		gb_cpu_register_store_handler (write_vbk_handler);
		gb_cpu_register_store_handler (write_bcpd_handler);
		gb_cpu_register_store_handler (write_ocpd_handler);
		memset (CRAM_BG, 0, 64);
		memset (CRAM_OBJ, 0, 64);

		draw = draw_cgb;
		__color_obj = __color_obj_cgb;
	}
	else
	{
		draw = draw_dmg;
		__color_obj = __color_obj_dmg;
	}

	memset (__lcd_1, 0, NPIXELS * sizeof (uint16_t));
	memset (__lcd_2, 0, NPIXELS * sizeof (uint16_t));
	lcd = __lcd_1;
	lcd_buf = __lcd_2;
}

#ifdef DEBUG_PPU

/* get color within sprite. */
static inline uint8_t color_sprite (uint8_t n, uint8_t x, uint8_t y)
{
	if (OBJ_SIZE == 16) n &= 0xFE;
	return color_tile (vram + (n << 4), x, y);
}

static inline void print_tile (uint8_t* t)
{
	uint8_t c;
	for (uint8_t y = 0; y < 8; y ++)
	{
		for (uint8_t x = 0; x < 8; x ++)
		{
			c = color_tile (t, x, y);
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

	printf
	(
		" SPRITE @ (%d - 8 [%d], %d - 16 [%d])\n",
		sprite[1],
		sprite[1] - 8,
		sprite[0],
		sprite[0] - 16
	);
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
#endif  // ifdef DEBUG_PPU
