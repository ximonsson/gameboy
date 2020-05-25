#include "gameboy/cpu.h"
#include <stdint.h>

static uint8_t* nr10;
static uint8_t* nr11;
static uint8_t* nr12;
static uint8_t* nr13;
static uint8_t* nr14;

static uint8_t* nr21;
static uint8_t* nr22;
static uint8_t* nr23;
static uint8_t* nr24;

static uint8_t* nr30;
static uint8_t* nr31;
static uint8_t* nr32;
static uint8_t* nr33;
static uint8_t* nr34;
static uint8_t* wave_pat;

static uint8_t* nr41;
static uint8_t* nr42;
static uint8_t* nr43;
static uint8_t* nr44;

static uint8_t* nr50;
static uint8_t* nr51;
static uint8_t* nr52;

void gb_apu_reset ()
{
	nr10 = gb_cpu_mem (0xFF10);
	nr11 = gb_cpu_mem (0xFF11);
	nr12 = gb_cpu_mem (0xFF12);
	nr13 = gb_cpu_mem (0xFF13);
	nr14 = gb_cpu_mem (0xFF14);

	nr21 = gb_cpu_mem (0xFF16);
	nr22 = gb_cpu_mem (0xFF17);
	nr23 = gb_cpu_mem (0xFF18);
	nr24 = gb_cpu_mem (0xFF19);

	nr30 = gb_cpu_mem (0xFF1A);
	nr31 = gb_cpu_mem (0xFF1B);
	nr32 = gb_cpu_mem (0xFF1C);
	nr33 = gb_cpu_mem (0xFF1D);
	nr34 = gb_cpu_mem (0xFF1E);
	wave_pat = gb_cpu_mem (0xFF30);

	nr41 = gb_cpu_mem (0xFF20);
	nr42 = gb_cpu_mem (0xFF21);
	nr43 = gb_cpu_mem (0xFF22);
	nr44 = gb_cpu_mem (0xFF23);

	nr50 = gb_cpu_mem (0xFF24);
	nr51 = gb_cpu_mem (0xFF25);
	nr52 = gb_cpu_mem (0xFF26);
}
