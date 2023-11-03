#include "gb/cpu.h"
#include <string.h>
#include <assert.h>

#ifdef DEBUG_CPU
#include <stdio.h>
#endif

/* CPU Registers */

static uint16_t reg_af;
static uint16_t reg_bc;
static uint16_t reg_de;
static uint16_t reg_hl;
static uint16_t sp;
static uint16_t pc;

static uint8_t* reg_a = ((uint8_t *) &reg_af) + 1;
static uint8_t* reg_f = ((uint8_t *) &reg_af);
static uint8_t* reg_b = ((uint8_t *) &reg_bc) + 1;
static uint8_t* reg_c = ((uint8_t *) &reg_bc);
static uint8_t* reg_d = ((uint8_t *) &reg_de) + 1;
static uint8_t* reg_e = ((uint8_t *) &reg_de);
static uint8_t* reg_h = ((uint8_t *) &reg_hl) + 1;
static uint8_t* reg_l = ((uint8_t *) &reg_hl);

#define AF reg_af
#define A (* reg_a)
#define F (* reg_f)

#define BC reg_bc
#define B (* reg_b)
#define C (* reg_c)

#define DE reg_de
#define D (* reg_d)
#define E (* reg_e)

#define HL reg_hl
#define H (* reg_h)
#define L (* reg_l)

#define SP sp
#define PC pc

/* define flags */
enum flags
{
	F_Z = 0x80,
	F_N = 0x40,
	F_H = 0x20,
	F_C = 0x10,
};

/* Memory ------------------------------------------------------------------------------------------------ */

/* RAM memory. */
static uint8_t ram[1 << 16];

uint8_t* gb_cpu_mem (uint16_t p) { return ram + p; }

/**
 * n_rom_banks keeps track of the total number of banks in the cartridge.
 * This is needed because some games seem to write a higher value than allowed.
 */
static int n_rom_banks;
static const uint8_t* ROM;

void gb_cpu_load_rom (int banks, const uint8_t* data)
{
	n_rom_banks = banks;
	ROM = data;
	memcpy (ram, ROM, ROM_BANK_SIZE << 1);
}

void gb_cpu_switch_rom_bank (int b)
{
	memcpy (ram + ROM_BANK_SIZE, ROM + (b % n_rom_banks) * ROM_BANK_SIZE, ROM_BANK_SIZE);
}

void gb_cpu_load_ram (uint8_t* data) { memcpy (ram + 0xA000, data, RAM_BANK_SIZE); }

#define MAX_HANDLERS 32

/* Currently registered read handlers. */
static read_handler read_handlers[MAX_HANDLERS];
static int n_read_handlers = 0;

void gb_cpu_register_read_handler (read_handler h)
{
	read_handlers[n_read_handlers ++] = h;
	read_handlers[n_read_handlers] = 0;
}

static uint8_t mem_read (uint16_t adr)
{
	uint8_t v = ram[adr];
	int stop = 0;
	for (read_handler* h = read_handlers; (*h) != 0 && !stop; h ++)
		stop = (*h)(adr, &v);
	return v;
}

#define RAM(a) mem_read (a)

/* Current store handlers. */
static store_handler store_handlers[MAX_HANDLERS];
static int n_store_handlers = 0;

void gb_cpu_register_store_handler (store_handler h)
{
	store_handlers[n_store_handlers ++] = h;
	store_handlers[n_store_handlers] = 0;
}

/**
 * Store to memory.
 * Makes sure the callbacks are run for specific memory addresses.
 */
static void mem_store (uint16_t adr, uint8_t v)
{
	int stop = 0;
	for (store_handler* h = store_handlers; (*h) != 0 && !stop; h ++)
		stop = (*h)(adr, v);
	if (!stop) // if we didn't break the loop we can store to RAM @ address.
		ram[adr] = v;
}

#define STORE(a, v) mem_store (a, v)

/* Define some memory handlers here. */

/* Transfer memory to OAM location. */
static void oam_dma_transfer (uint8_t v)
{
	uint16_t src = v << 8, dst = OAM_LOC;
	for (int i = 0; i < 0xA0; i ++, dst ++, src ++)
		STORE(dst, RAM(src));

#ifdef DEBUG_CPU
	printf ("\t\t>>> OAM transfer [$%.2X => $%.4X]\n", v, src);
#endif
}

/* check writes to initiate OAM DMA transfer. */
static int oam_dma_transf_handler (uint16_t address, uint8_t v)
{
	if (address == 0xFF46)
		oam_dma_transfer (v);
	return 0;
}

/**
 * stack_push pushes the value v to the stack.
 */
void stack_push (uint16_t v)
{
#ifdef DEBUG_CPU
	printf ("\tPUSH %.4X @ $%.4X\n", v, SP);
#endif
	STORE (--SP, v >> 8); // msb
	STORE (--SP, v);      // lsb
}

#define PUSH(v) stack_push (v)

/**
 * stack_pop pops the stack and returns the value;
 */
uint16_t stack_pop ()
{
	uint16_t lo = RAM (SP++);
	uint16_t hi = RAM (SP++);
#ifdef DEBUG_CPU
	printf ("\tPOP %.4X\n", (hi << 8) | lo);
#endif
	return (hi << 8) | lo;
}

#define POP() stack_pop ()

static int write_unused_ram_h (uint16_t adr, uint8_t v)
{
	return adr >= 0xFEA0 && adr <= 0xFEFF;
}

static int read_unused_ram_h (uint16_t adr, uint8_t* v)
{
	if (adr >= 0xFEA0 && adr <= 0xFEFF)
	{
		*v = 0xFF;
		return 1;
	}
	return 0;
}

static int write_echo_ram_h (uint16_t adr, uint8_t v)
{
	if (adr >= 0xE000 && adr <= 0xFDFF)
	{
		ram[adr - 0x2000] = v;
		return 1;
	}
	return 0;
}

static int read_echo_ram_h (uint16_t adr, uint8_t* v)
{
	if (adr >= 0xE000 && adr <= 0xFDFF)
	{
		*v = ram[adr - 0x2000];
		return 1;
	}
	return 0;
}

/* Special Registers ---------------------------------------------------------------- */

/* HALT flag. */
static uint8_t f_halt;

/* Interrupt Master Enable flag (IME). */
static uint8_t ime;

/* Interrupt Enable (IE) register. Is located at RAM memory $FFFF. */
static uint8_t* reg_ie = ram + 0xFFFF;
#define IE (* reg_ie)

/* Interrupt Flag (IF) register. Is located at RAM memory $FF0F. */
static uint8_t* reg_if = ram + 0xFF0F;
#define IF (* reg_if)

/* Divider register */
#define DIV_LOC 0xFF04
static uint8_t* reg_div = ram + DIV_LOC;
#define DIV (* reg_div)

/* writing to the DIV register resets it. */
static int write_div_h (uint16_t addr, uint8_t n)
{
	if (addr == DIV_LOC)
	{
		DIV = 0;
		return 1;
	}
	return 0;
}

// keep track of DIV cycles.
static int divcc;

static void inc_div (int cc)
{
	divcc += cc;
	if (divcc >= GB_DIV_CC)
	{
		DIV += divcc / GB_DIV_CC;
		divcc %= GB_DIV_CC;
	}
}

/* Time counter register. */
static uint8_t* reg_tima = ram + 0xFF05;
#define TIMA (* reg_tima)

/* Timer Modulo register. */
static uint8_t* reg_tma = ram + 0xFF06;
#define TMA (* reg_tma)

/* Timer Control register. */
static uint8_t* reg_tac = ram + 0xFF07;
#define TAC (* reg_tac)

#define TIMER_ENABLED (TAC & 0x04)

// Keep track of TIMA cycles.
static int timacc;

static void inc_tima (int cc)
{
	if (!TIMER_ENABLED) return;

	timacc += cc;
	static const uint16_t timer_cc[4] = { 1024, 16, 64, 256 };
	for (int c = timer_cc[TAC & 0x03]; timacc >= c; timacc -= c)
	{
		if (++TIMA == 0) // overflow
		{
			gb_cpu_flag_interrupt (INT_FLAG_TIMER);
			TIMA = TMA;
		}
	}
}

/* CPU Instructions ----------------------------------------------------------------- */

// NOTE : LD instructions are not implemented here; they are all implemented in the
// autogeneretade code instead.

void ldhl (uint8_t n)
{
	uint16_t hl = SP + (int8_t) n;

	F = 0;

	if ((n ^ SP ^ hl) & 0x10)
		F |= F_H;

	if (n + (SP & 0xFF) > 0xFF)
		F |= F_C;

	HL = hl;
}

void push (uint16_t v)
{
	PUSH (v);
}

void pop (uint16_t *v)
{
	*v = POP ();
}

void add (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    x%.2X + x%.2X\n", A, n);
#endif
	uint16_t a = A + n;

	F = 0; // reset flags

	if (a & 0xFF00)
		F |= F_C;
	if (((A ^ n ^ a) & 0x10) == 0x10) // half carry
		F |= F_H;

	A = a;
	if (A == 0)
		F |= F_Z;
}

void addhl (uint16_t n)
{
	uint32_t hl = HL + n;
	F &= F_Z;
	if (hl > 0xFFFF)
		F |= F_C;
	if (((HL ^ n ^ hl) & 0x1000) == 0x1000) // half carry
		F |= F_H;
	HL = hl;
}

void addsp (uint8_t n)
{
	F = 0; // reset flags

	uint16_t sp_ = SP + (int8_t) n;

	if ((n ^ SP ^ sp_) & 0x10)
		F |= F_H;

	if (n + (SP & 0xFF) > 0xFF)
		F |= F_C;

	SP = sp_;
}

void adc (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    $%.2X + $%.2X + %d\n", A, n, ((F & F_C) >> 4));
#endif
	uint16_t a = A + n + ((F & F_C) >> 4);

	F = 0; // reset flags

	if (a > 0xFF)
		F |= F_C;
	if (((A ^ n ^ a) & 0x10) == 0x10) // half carry
		F |= F_H;

	A = a;
	if (A == 0)
		F |= F_Z;
}

void sub (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    $%.2X - $%.2X\n", A, n);
#endif
	uint16_t a = A - n;

	F = F_N;
	if (a & 0xFF00)
		F |= F_C;
	if (((A ^ n ^ a) & 0x10) == 0x10) // half carry
		F |= F_H;

	A = a;
	if (A == 0)
		F |= F_Z;
}

void sbc (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    $%.2X - $%.2X - %d\n", A, n, ((F & F_C) >> 4));
#endif
	uint16_t a = A - n - ((F & F_C) >> 4);

	F = F_N;
	if (a & 0xFF00)
		F |= F_C;
	if (((A ^ n ^ a) & 0x10) == 0x10) // half carry
		F |= F_H;

	A = a;
	if (A == 0)
		F |= F_Z;
}

void and (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    x%.2X & x%.2X\n", A, n);
#endif
	A &= n;
	F = F_H;
	if (A == 0)
		F |= F_Z;
}

void or (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    x%.2X | x%.2X\n", A, n);
#endif
	A |= n;
	F = 0;
	if (A == 0)
		F |= F_Z;
}

void xor (uint8_t n)
{
	A ^= n;
	F = 0;
	if (A == 0)
		F |= F_Z;
}

void cp (uint8_t n)
{
#ifdef DEBUG_CPU
	printf ("    x%.2X == x%.2X\n", A, n);
#endif
	F = F_N;
	if (A == n)
		F |= F_Z;
	if (A < n)
		F |= F_C;
	if ((A & 0x0F) < (n & 0x0F))
		F |= F_H;
}

void inc (uint8_t *n)
{
	uint8_t tmp = *n;
	(*n) ++;
	F &= ~(F_N | F_Z | F_H);
	if (*n == 0)
		F |= F_Z;
	if (((tmp ^ (*n) ^ 1) & 0x10) == 0x10) // half carry
		F |= F_H;
}

void inc16 (uint16_t* nn)
{
	(* nn) ++;
}

void dec16 (uint16_t* nn)
{
	(* nn) --;
}

void dec (uint8_t *n)
{
	(* n) --;
	F |= F_N;
	F &= ~(F_Z | F_H);
	if ((* n) == 0)
		F |= F_Z;
	if (((*n) & 0x0F) == 0x0F)
		F |= F_H;
}

void swap (uint8_t* n)
{
	uint16_t tmp = ((*n) & 0xF) << 4; // lower nibble
	*n = tmp | ((*n) >> 4);
	F = 0;
	if (*n == 0)
		F |= F_Z;
}

void daa ()
{
	// this implementation is "inspired" by
	// https://github.com/deltabeard/Peanut-GB/blob/master/peanut_gb.h#L1786

	uint16_t a = A;

	if (F & F_N)
	{
		if (F & F_H)
			a = (a - 0x06) & 0xFF;

		if (F & F_C)
			a -= 0x60;
	}
	else
	{
		if ((F & F_H) || (a & 0x0F) > 0x9)
			a += 0x06;

		if ((F & F_C) || a > 0x9F)
			a += 0x60;
	}

	F &= ~(F_Z | F_H);

	if (a & 0x100) F |= F_C;
	A = a;
	if (A == 0) F |= F_Z;
}

void cpl ()
{
	A ^= 0xFF;
	F |= (F_N | F_H);
}

void ccf ()
{
	uint8_t tmp = F & F_C;
	F &= ~(F_C | F_N | F_H); // reset N H C flags
	F |= (~tmp & F_C);
}

void scf ()
{
	F &= ~(F_N | F_H); // reset N H flags
	F |= F_C;
}

void nop ()
{
	// nada
}

void halt ()
{
	// power down cpu until an interrupt occurs.
	f_halt = 1;
}

void stop ()
{
	// halt cpu & display until button pressed
	// nada ?
}

void di ()
{
	ime = 0;
}

void ei ()
{
	ime = 1;
}

void rl (uint8_t *n)
{
	uint8_t tmp = ((*n) & 0x80) >> 7;
	(*n) <<= 1;
	(*n) |= (F >> 4) & 1; // bit 0 = C

	// reset flags
	F = tmp << 4; // C = old bit 7
	if ((*n) == 0)
		F |= F_Z;
}

#define rla() { rl (&A); F &= ~F_Z; }

void rlc (uint8_t *n)
{
	uint8_t tmp = ((*n) & 0x80) >> 7;
	(*n) <<= 1;
	(*n) |= tmp;

	// reset flags
	F = tmp << 4; // C = old bit 7
	if ((*n) == 0)
		F |= F_Z;
}

#define rlca() { rlc (&A); F &= ~F_Z; }

void rrc (uint8_t *n)
{
	uint8_t tmp = (*n) & 1;
	(*n) >>= 1;
	(*n) |= tmp << 7; // bit 0 = old bit 7
	// reset flags
	F = tmp << 4; // C = old bit 0
	if ((*n) == 0)
		F |= F_Z;
}

#define rrca() { rrc (&A); F &= ~F_Z; }

void rr (uint8_t *n)
{
	uint8_t tmp = (*n) & 1;
	(*n) >>= 1;
	(*n) |= (F << 3) & 0x80; // bit 0 = carry
	// reset flags
	F = tmp << 4; // C = old bit 0
	if ((*n) == 0)
		F |= F_Z;
}

#define rra() { rr (&A); F &= ~F_Z; }

void sla (uint8_t *n)
{
	// reset flags
	F = ((*n) & 0x80) >> 3; // C = old bit 7
	(*n) <<= 1;
	if ((*n) == 0)
		F |= F_Z;
}

void sra (uint8_t* n)
{
	// reset flags
	F = ((*n) & 1) << 4; // C = old bit 0

	uint8_t msb = (*n) & 0x80;
	(*n) >>= 1;
	(*n) |= msb; // MSB does not change

	if ((*n) == 0)
		F |= F_Z;
}

void srl (uint8_t* n)
{
	// reset flags
	F = ((*n) & 1) << 4; // C = old bit 0
	(*n) >>= 1;
	if ((*n) == 0)
		F |= F_Z;
}

void bit (uint8_t r, uint8_t b)
{
	F &= ~(F_N | F_Z);
	F |= F_H;
	if (((1 << b) & r) == 0)
		F |= F_Z;
}

void set (uint8_t* r, uint8_t b)
{
	(*r) |= 1 << b;
}

void res (uint8_t* r, uint8_t b)
{
	(*r) &= ~(1 << b);
}

#ifdef DEBUG_CPU
void jp (uint16_t nn)
{
	printf (">>> JUMP @ $%.4X\n", nn);
	PC = nn;
}
#else
#define jp(nn) PC = nn
#endif

enum jump_cc
{
	JP_CC_NZ,
	JP_CC_Z,
	JP_CC_NC,
	JP_CC_C,
};

/* Keep a flag with extra cycles when conditionals are met.
 * TODO not sure what I think of this solution.
 */
static int cond_cc;

/* Macro for conditional jumps/calls. */
#define CONDITIONAL(inst, cond, c) {\
	switch (cond)\
	{\
		case JP_CC_NZ: if ((F & F_Z) == 0) { cond_cc = c; inst; } break;\
		case JP_CC_Z:  if ((F & F_Z) != 0) { cond_cc = c; inst; } break;\
		case JP_CC_NC: if ((F & F_C) == 0) { cond_cc = c; inst; } break;\
		case JP_CC_C:  if ((F & F_C) != 0) { cond_cc = c; inst; } break;\
	}\
}

void jpcc (enum jump_cc cond, uint16_t nn)
{
	CONDITIONAL (jp (nn), cond, 4);
}

#ifdef DEBUG_CPU
void jr (int8_t n)
{
	printf ("    JUMP @ PC +/- %d (=> $%.4X)\n", n, PC + n);
	PC += n;
}
#else
#define jr(n) PC += (int8_t) n
#endif

void jrcc (enum jump_cc cond, int8_t n)
{
	CONDITIONAL (jr (n), cond, 4);
}

#define call(nn) { PUSH (PC); jp (nn); }

void callcc (enum jump_cc cond, uint16_t nn)
{
	CONDITIONAL (call (nn), cond, 12);
}

void rst (uint8_t n)
{
	call (n);
}

#define ret() jp (POP ())

void retcc (enum jump_cc cond)
{
	CONDITIONAL (ret (), cond, 12);
}

void reti ()
{
	jp (POP ());
	ime = 1;
}

/* Include generated file with operations. */
#include "gb/operations.h"

void gb_cpu_flag_interrupt (interrupt_flag f)
{
#ifdef DEBUG_CPU
	printf ("%s $%.2X\n", ">>> IRQ", f);
#endif
	IF |= f;
}

/**
 * Interrupt.
 * This assumes that one has already checked that an interrupt has been requested and IME is enabled.
 *
 * From docs:
 *   1. When an interrupt is generated, the IF flag will be set.
 *   2. If the IME flag is set & the corresponding IE flag is set, the following 3 steps are performed.
 *   3. Reset the IME flag and prevent all interrupts.
 *   4. The PC (program counter) is pushed onto the stack.
 *   5. Jump to the starting address of the interrupt.
 */
void interrupt ()
{
	uint8_t f = 1;
	uint8_t b = 0;

	// loop over interrupt flags in priority order.
	// call any interrupts that have been flagged and enabled.
	for (; b < 5 && !(f & IF & IE); b ++)
		f <<= 1;

	ime = 0;
	IF &= ~f;
	PUSH (PC);
	PC = 0x40 + 0x8 * b;

#ifdef DEBUG_CPU
	printf (" <INT>; calling handler @ $%.4X\n", PC);
#endif
}

/**
 * Reset the CPU.
 */
void gb_cpu_reset ()
{
	PC = 0x100;
	SP = 0xFFFE;

	AF = 0x01B0;
	BC = 0x0013;
	DE = 0x00D8;
	HL = 0x014D;

	ime = 1;
	f_halt = 0;
	cond_cc = 0;

	// reset memory read/write handlers and add the default ones.

	n_store_handlers = 0;
	gb_cpu_register_store_handler (oam_dma_transf_handler);
	gb_cpu_register_store_handler (write_div_h);
	gb_cpu_register_store_handler (write_unused_ram_h);
	gb_cpu_register_store_handler (write_echo_ram_h);
	store_handlers[n_store_handlers] = 0;

	n_read_handlers = 0;
	gb_cpu_register_read_handler (read_unused_ram_h);
	gb_cpu_register_read_handler (read_echo_ram_h);
	read_handlers[n_read_handlers] = 0;

	memset (ram, 0, 1 << 16);

	// reset timers
	divcc = timacc = 0;
}

/* macro to check if an interrupt is requested and enabled. */
#define IRQ (IE & IF)

/**
 * Step the CPU, executing one operation.
 *
 * Returns the number of CPU cycles it took to perform the operation.
 */
int gb_cpu_step ()
{
	int cc = 0;

	if (f_halt)
	{
		// if the CPU is halted and an interrupt is requested we unset the halt flag
		// else if just increment four cycles the timers and return.

		if (IRQ)
			f_halt = 0;
		else
		{
			cc = 4;
			goto inc;
		}
	}

	// check any interrupts
	if (ime && IRQ)
	{
		interrupt ();
		cc = 5;
	}

	// load an opcode and perform the operation associated,
	// step the PC and clock the number of cycles

#ifdef DEBUG_CPU
	printf ("$%.4X: ", PC);
#endif

	uint8_t opcode = RAM (PC ++);
	const operation* op = &operations[opcode];

#ifdef DEBUG_CPU
	if (opcode == 0xCB) op = &operations_cb[RAM (PC ++)]; // hijack in debug mode so we can print the operation
	printf
	(
		"%-20s AF = x%.4X BC = x%.4X DE = x%.4X HL = x%.4X SP = x%.4X IF = x%.2X IE = 0x%.2X IME = %d\n",
		op->name, AF, BC, DE, HL, SP, IF, IE, ime
	);
#endif

	cc += op->instruction () + cond_cc;
	cond_cc = 0;

inc:
	// increment timers
	inc_div (cc);
	inc_tima (cc);

	return cc;
}
