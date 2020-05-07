#include "gameboy/cpu.h"
#include <string.h>
#include <assert.h>

#ifdef DEBUG
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

/* define flags */
enum flags
{
	F_Z = 0x80,
	F_N = 0x40,
	F_H = 0x20,
	F_C = 0x10,
};

/* Interrupt Master Enable flag (IME). */
static int ime = 0;

/* Memory ------------------------------------------------------------------------------------------------ */

/* RAM memory. */
static uint8_t ram[1 << 16];

/* Interrupt Enable (IE) register. Is located at RAM memory $FFFF. */
static uint8_t* reg_ie = ram + 0xFFFF;
#define IE (* reg_ie)

/* Interrupt Flag (IF) register. Is located at RAM memory $FF0F. */
static uint8_t* reg_if = ram + 0xFF0F;
#define IF (* reg_if)

uint8_t* gb_cpu_mem (uint16_t p) { return ram + p; }

void gb_cpu_load_rom (uint8_t b, uint8_t* data) { memcpy (ram + ROM_BANK_SIZE * b, data, ROM_BANK_SIZE); }

/* Transfer memory to OAM location. */
static void oam_dma_transfer (uint8_t v)
{
#ifdef DEBUG
	printf ("OAM transfer\n");
#endif
	uint16_t src = ((v & 0x1F) << 8) | 0x9F;
	memcpy (ram + OAM_LOC, ram + src, 0x9F);
}

#define MAX_HANDLERS 16

/* Currently registered read handlers. */
static read_handler read_handlers[MAX_HANDLERS];
static int n_read_handlers = 0;

void gb_cpu_register_read_handler (read_handler h)
{
	read_handlers[n_read_handlers] = h;
	n_read_handlers ++;
	read_handlers[n_read_handlers] = 0;
}

static uint8_t mem_read (uint16_t address)
{
	uint8_t v = ram[address];
	int stop = 0;
	for (read_handler* h = read_handlers; (*h) != 0 && !stop; h ++)
	{
		stop = (*h)(address, &v);
	}
	return v;
}

#define RAM(a) mem_read (a)

/* Current store handlers. */
static store_handler store_handlers[MAX_HANDLERS];
static int n_store_handlers = 0;

void gb_cpu_register_store_handler (store_handler h)
{
	store_handlers[n_store_handlers] = h;
	n_store_handlers ++;
	store_handlers[n_store_handlers] = 0;
}

/**
 * Store to memory.
 * Makes sure the callbacks are run for specific memory addresses.
 */
static void mem_store (uint16_t address, uint8_t v)
{
	int stop = 0;
	for (store_handler* h = store_handlers; (*h) != 0 && !stop; h ++)
	{
		stop = (*h)(address, v);
	}
	if (!stop) // if we didn't break the loop we can store to RAM @ address.
		ram[address] = v;
}

#define STORE(a, v) mem_store (a, v)

/* Define some memory handlers here. */

/* check writes to initiate OAM DMA transfer. */
static int oam_dma_transf_handler (uint16_t address, uint8_t v)
{
	if (address != 0xFF46) return 0;
	oam_dma_transfer (v);
	return 1;
}

/**
 * stack_push pushes the value v to the stack.
 */
void stack_push (uint16_t v)
{
#ifdef DEBUG
	printf ("    PUSH %.4X @ $%.4X\n", v, sp);
#endif
	STORE (sp--, v >> 8); // msb
	STORE (sp--, v);      // lsb
}

#define PUSH(v) stack_push (v)

/**
 * stack_pop pops the stack and returns the value;
 */
uint16_t stack_pop ()
{
	uint16_t lo = RAM (++sp);
	uint16_t hi = RAM (++sp);
#ifdef DEBUG
	printf ("    POP %.4X\n", (hi << 8) | lo);
#endif
	return (hi << 8) | lo;
}

#define POP() stack_pop ()

/* CPU Instructions */

// NOTE : LD instructions are not implemented here; they are all implemented in the autogeneretade code instead.

void ldhl (int8_t n)
{
	HL = SP + n;
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
#ifdef DEBUG
	printf ("    x%.2X + x%.2X\n", A, n);
#endif
	uint16_t a = A + n;

	F = 0; // reset flags

	if (a > 0xFF)
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
	F &= ~(F_N | F_H | F_C);
	if (hl > 0xFFFF)
		F |= F_C;
	if (((HL ^ n ^ hl) & 0x1000) == 0x1000) // half carry
		F |= F_H;
	HL = hl;
}

void addsp ()
{
	int8_t n = RAM (pc ++);
	uint32_t sp_ = sp + n;
	F = 0;
	if (sp_ > 0xFFFF)
		F |= F_C;
	if (((sp ^ n ^ sp_) & 0x1000) == 0x1000) // half carry
		F |= F_H;
	sp = sp_;
}

void adc (uint8_t n)
{
#ifdef DEBUG
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
	uint8_t a = A;
	A -= n;

	F = F_N;
	if (A == 0)
		F |= F_Z;
	if (a < n)
		F |= F_C;
	if ((a & 0x0F) < (n & 0x0F))
		F |= F_H;
}

void sbc (uint8_t n)
{
#ifdef DEBUG
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
#ifdef DEBUG
	printf ("    x%.2X & x%.2X\n", A, n);
#endif
	A &= n;
	F = F_H;
	if (A == 0)
		F |= F_Z;
}

void or (uint8_t n)
{
#ifdef DEBUG
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
#ifdef DEBUG
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
	// https://github.com/taisel/GameBoy-Online/blob/master/js/GameBoyCore.js#L588
	// as one can see there is a lot of magic and i am afraid of trying to figure
	// it out myself after 7 weeks of debugging and reading docs.
	//
	// I hate this solution though; I have never seen so many if-statements

	F &= ~(F_Z | F_H);

	if ((F & F_N) == 0)
	{
		if ((F & F_C) || A > 0x99)
		{
			A += 0x60;
			F |= F_C;
		}
		if ((F & F_H) || (A & 0x0F) > 0x9)
		{
			A += 0x06;
			F &= ~F_H;
		}
	}
	else if ((F & (F_C | F_H)) == (F_C | F_H))
	{
		A += 0x9A;
		F &= ~F_H;
	}
	else if ((F & F_C) == F_C)
	{
		A += 0xA0;
	}
	else if ((F & F_H) == F_H)
	{
		A += 0xFA;
		F &= ~F_H;
	}

	if (A == 0) F |= F_Z;
}

void cpl ()
{
	//A = ~A;
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
	//pc ++;
}

void halt ()
{
	// power down cpu until an interrupt occurs.
	// nada?
	//pc ++;
}

void stop ()
{
	// halt cpu & display until button pressed
	// nada ?
	//pc ++;
}

void di ()
{
	ime = 0;
}

void ei ()
{
	ime = 1;
}

void rlc (uint8_t *n)
{
	uint8_t tmp = ((*n) & 0x80) >> 7;
	(*n) <<= 1;
	(*n) |= tmp;

	// reset flags
	F = 0;
	F |= tmp << 4; // C = old bit 7
}

#define rlca() rlc (&A)

void rl (uint8_t *n)
{
	uint8_t tmp = ((*n) & 0x80) >> 7;
	(*n) <<= 1;
	(*n) |= (F >> 4) & 1; // bit 0 = C

	// reset flags
	F = 0;
	F |= tmp << 4; // C = old bit 7
	if ((*n) == 0)
		F &= ~F_Z;
}

#define rla() rl (&A)

void rrc (uint8_t *n)
{
	uint8_t tmp = (*n) & 1;
	(*n) >>= 1;
	(*n) |= tmp << 7; // bit 0 = old bit 7
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= tmp << 4; // C = old bit 0
	if ((*n) == 0)
		F &= ~F_Z;
}
#define rrca() rrc(&A)

void rr (uint8_t *n)
{
	uint8_t tmp = (*n) & 1;
	(*n) >>= 1;
	(*n) |= (F << 3) & 0x80; // bit 0 = carry
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= tmp << 4; // C = old bit 0
	if ((*n) == 0)
		F &= ~F_Z;
}
#define rra() rr(&A)

void sla (uint8_t *n)
{
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= ((*n) & 0x80) >> 3; // C = old bit 7

	(*n) <<= 1;
	if ((*n) == 0)
		F &= ~F_Z;
}

void sra (uint8_t* n)
{
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= ((*n) & 1) << 4; // C = old bit 0

	uint8_t msb = (*n) & 0x80;
	(*n) >>= 1;
	(*n) |= msb; // MSB does not change

	if ((*n) == 0)
		F &= ~F_Z;
}

void srl (uint8_t* n)
{
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= ((*n) & 1) << 4; // C = old bit 0

	(*n) >>= 1;
	if ((*n) == 0)
		F &= ~F_Z;
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

#ifdef DEBUG
void jp (uint16_t nn)
{
	printf ("    JUMP @ %.4X\n", nn);
	pc = nn;
}
#else
#define jp(nn) pc = nn
#endif

enum jump_cc
{
	JP_CC_NZ,
	JP_CC_Z,
	JP_CC_NC,
	JP_CC_C,
};

void jpcc (enum jump_cc cc, uint16_t nn)
{
	switch (cc)
	{
		case JP_CC_NZ: if ((F & F_Z) == 0) jp (nn); break;
		case JP_CC_Z:  if ((F & F_Z) != 0) jp (nn); break;
		case JP_CC_NC: if ((F & F_C) == 0) jp (nn); break;
		case JP_CC_C:  if ((F & F_C) != 0) jp (nn); break;
	}
}

#ifdef DEBUG
void jr (int8_t n)
{
	printf ("    JUMP @ PC +/- %d (=> %.4X)\n", n, pc + n);
	pc += n;
}
#else
#define jr(n) pc += n
#endif

void jrcc (enum jump_cc cc, int8_t n)
{
	switch (cc)
	{
		case JP_CC_NZ: if ((F & F_Z) == 0) jr (n); break;
		case JP_CC_Z:  if ((F & F_Z) != 0) jr (n); break;
		case JP_CC_NC: if ((F & F_C) == 0) jr (n); break;
		case JP_CC_C:  if ((F & F_C) != 0) jr (n); break;
	}
}

#define call(nn) { PUSH (pc); jp (nn); }

void callcc (enum jump_cc cc, uint16_t nn)
{
	switch (cc)
	{
		case JP_CC_NZ: if ((F & F_Z) == 0) call (nn); break;
		case JP_CC_Z:  if ((F & F_Z) != 0) call (nn); break;
		case JP_CC_NC: if ((F & F_C) == 0) call (nn); break;
		case JP_CC_C:  if ((F & F_C) != 0) call (nn); break;
	}
}

void rst (uint8_t n)
{
	call (n);
}

#define ret() jp (POP ())

void retcc (enum jump_cc cc)
{
	switch (cc)
	{
		case JP_CC_NZ: if ((F & F_Z) == 0) ret (); break;
		case JP_CC_Z:  if ((F & F_Z) != 0) ret (); break;
		case JP_CC_NC: if ((F & F_C) == 0) ret (); break;
		case JP_CC_C:  if ((F & F_C) != 0) ret (); break;
	}
}

void reti ()
{
	jp (POP ());
	ime = 1;
}

/* Include generated file with operations. */
#include "gameboy/operations.h"

void gb_cpu_flag_interrupt (interrupt_flag f)
{
#ifdef DEBUG
	printf ("Interrupt x%.4X requested\n", f);
#endif
	IF |= f;
}

/**
 * Interrupt.
 *
 * From docs:
 *   1. When an interrupt is generated, the IF flag will be set.
 *   2. If the IME flag is set & the corresponding IE flag is set, the following 3 steps are performed.
 *   3. Reset the IME flag and prevent all interrupts.
 *   4. The PC (program counter) is pushed onto the stack.
 *   5. Jump to the starting address of the interrupt.
 */
int interrupt ()
{
	// all interrupts disabled
	if (!ime) return 1;

	// loop over interrupt flags in priority order.
	// call any interrupts that have been flagged and enabled.
	for (uint8_t b = 0; b < 5; b ++)
	{
		if (IF & (1 << b) && IE & (1 << b))
		{
			ime = 0;
			IF &= ~(1 << b);
			PUSH (pc);
			pc = 0x40 + 0x8 * b;
#ifdef DEBUG
			printf (" [interrupt] > calling handler @ $%.4X\n", pc);
#endif
			return 0;
		}
	}

	// no interrupt requested
	return 1;
}

/**
 * Reset the CPU.
 */
void gb_cpu_reset ()
{
	pc = 0x0100;
	sp = 0xFFFE;

	AF = 0;
	BC = 0;
	DE = 0;
	HL = 0;
	//AF = 0x01B0;
	//BC = 0x0013;
	//DE = 0x00D8;
	//HL = 0x014D;

	ime = 0;

	// reset memory read/write handlers and add the default ones.

	n_store_handlers = 0;
	gb_cpu_register_store_handler (oam_dma_transf_handler);
	read_handlers[n_read_handlers] = 0;

	n_read_handlers = 0;
	read_handlers[n_read_handlers] = 0;

	memset (ram, 0, 1 << 16);
}

/**
 * Step the CPU, executing one operation.
 *
 * Returns the number of CPU cycles it took to perform the operation.
 */
int gb_cpu_step ()
{
	int cc = 0;
	// first check any interrupts
	if (interrupt () == 0) cc += 5;

	// load an opcode and perform the operation associated,
	// step the PC and clock the number of cycles
#ifdef DEBUG
	printf ("$%.4X: ", pc);
#endif
	//assert (pc < 0x8000 || pc >= 0xFF80);
	uint8_t opcode = RAM (pc ++);
	const operation* op = &operations[opcode];
#ifdef DEBUG
	if (opcode == 0xCB) op = &operations_cb[RAM (pc ++)]; // hijack in debug mode so we can print the operation
	printf ("%-20s AF = x%.4X BC = x%.4X DE = x%.4X SP = x%.4X HL = x%.4X IF = x%.2X IE = 0x%.2X IME = %d\n",
			op->name, AF, BC, DE, SP, HL, IF, IE, ime);
#endif
	op->instruction ();
	return cc + op->cc;
}
