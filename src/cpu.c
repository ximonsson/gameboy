#include <stdint.h>

/* CPU Registers */

uint16_t reg_af;
uint16_t reg_bc;
uint16_t reg_de;
uint16_t reg_hl;
uint16_t sp;
uint16_t pc;

uint8_t* reg_a = ((uint8_t *) &reg_af) + 1;
uint8_t* reg_f = ((uint8_t *) &reg_af);
uint8_t* reg_b = ((uint8_t *) &reg_bc) + 1;
uint8_t* reg_c = ((uint8_t *) &reg_bc);
uint8_t* reg_h = ((uint8_t *) &reg_hl) + 1;
uint8_t* reg_l = ((uint8_t *) &reg_hl);

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

/* RAM memory. */
static uint8_t ram[0xFFFF + 1]; // the +1 is because of the IE register.

/* Interrupt Enable (IE) register. Is located at RAM memory $FFFF. */
static uint8_t* reg_ie = ram + 0xFFFF;
#define IE (* reg_ie)

/* Interrupt Flag (IF) register. Is located at RAM memory $FF0F. */
static uint8_t* reg_if = ram + 0xFF0F;
#define IF (* reg_if)

/* memory */

/**
 * Read from RAM handler.
 *
 * This is a function that takes the address as first parameter and the current value in memory
 * at that address.
 *
 * In case the handler affects the RAM address and normal execution should be stopped an non-zero
 * value should be returned by the handler, else zero.
 */
typedef int (*read_handler) (uint16_t, uint8_t*);

/* Currently registered read handlers. */
static read_handler read_handlers[8] = { 0 };
static int n_read_handlers = 0;

/**
 * Add a callback (read_handler) for when trying to read from memory.
 */
void gb_cpu_register_read_handler (read_handler h)
{
	read_handlers[n_read_handlers] = h;
	n_read_handlers ++;
}

static uint8_t mem_read (uint16_t address)
{
	uint8_t v = ram[address];
	int stop = 0;
	for (read_handler* h = read_handlers; h != 0 && !stop; h ++)
	{
		stop = (*h)(address, &v);
	}
	return v;
}

#define RAM(a) mem_read (a)

/**
 * Store to RAM handler.
 *
 * This is a function that takes the address as first parameter and the value that is to be
 * stored as second.
 *
 * In case the handler affects the RAM address and normal execution should be stopped an non-zero
 * value should be returned by the handler, else zero.
 */
typedef int (*store_handler) (uint16_t, uint8_t);

/* Current store handlers. */
static store_handler store_handlers[8] = { 0 };
static int n_store_handlers = 0;

/**
 * Add a callback when storing to RAM.
 */
void gb_cpu_register_store_handler (store_handler h)
{
	store_handlers[n_store_handlers] = h;
	n_store_handlers ++;
}

/**
 * Store to memory.
 * Makes sure the callbacks are run for specific memory addresses.
 */
static void mem_store (uint16_t address, uint8_t v)
{
	int stop = 0;
	for (store_handler* h = store_handlers; h != 0 && !stop; h ++)
	{
		stop = (*h)(address, v);
	}
	if (!stop) // if we didn't break the loop we can store to RAM @ address.
		ram[address] = v;
}

#define STORE(a, v) mem_store (a, v)

/**
 * stack_push pushes the value v to the stack.
 */
void stack_push (uint16_t v)
{
	ram[sp--] = v >> 8;
	ram[sp--] = v;
}

#define PUSH(v) stack_push (v)

/**
 * stack_pop pops the stack and returns the value;
 */
uint16_t stack_pop ()
{
	uint16_t lo = ram[++sp];
	uint16_t hi = ram[++sp];
	return (hi << 8) | lo;
}

#define POP() stack_pop ()

/* CPU Instructions */

// TODO these LD instructions will not work for storing to RAM as they will not call the handlers
// A solution might be to keep it like this and solve it in a wrapper function instead.

#define ld(n, nn) *n = nn

void ldd (uint8_t* n, uint8_t nn)
{
	ld (n, nn);
	HL --;
}

void ldi (uint8_t* n, uint8_t nn)
{
	ld (n, nn);
	HL ++;
}

/*
void ld8 (uint8_t* n, uint8_t nn)
{
	*n = nn;
}

void ld16 (uint16_t* n, uint16_t nn)
{
	*n = nn;
}
*/

void push (uint16_t v)
{
	PUSH (v);
}

void pop (uint16_t *v)
{
	*v = POP ();
}

void add (int8_t n)
{
	uint8_t a = A;
	A += n;
	F = 0; // reset flags
	if (A == 0)
		F |= F_Z;
	if ((a & 0x80) != 0 && (A & 0x80) == 0) // carry
		F |= F_C;
	if ((a & 0x08) != 0 && (A & 0x08) == 0) // half carry
		F |= F_H;
}

void addhl (uint16_t n)
{
	uint16_t nn_ = HL;
	HL += n;
	F &= ~F_N;
	if ((nn_ & 0x0800) != 0 && (HL & 0x0800) == 0) // half carry
		F |= F_C;
	if ((nn_ & 0x8000) != 0 && (HL & 0x8000) == 0) // carry
		F |= F_C;
}

void addsp ()
{
	uint16_t nn_ = sp;
	int8_t n = RAM (pc ++);
	sp += n;

	F &= ~(F_N | F_Z);
	if ((nn_ & 0x0800) != 0 && (sp & 0x0800) == 0) // half carry
		F |= F_C;
	if ((nn_ & 0x8000) != 0 && (sp & 0x8000) == 0) // carry
		F |= F_C;
}

void adc (uint8_t n)
{
	uint8_t a = A;
	A += n + ((F & F_C) >> 4);
	F = 0; // reset flags
	if (A == 0)
		F |= F_Z;
	if ((a & 0x80) != 0 && ((A) & 0x80) == 0) // carry
		F |= F_C;
	if ((a & 0x08) != 0 && ((A) & 0x08) == 0) // half carry
		F |= F_H;
}

void sub (uint8_t n)
{
	uint8_t a = A;
	A = a - n;

	F = F_N;
	if (A == 0)
		F |= F_Z;
	// TODO other flags
}

void sbc (uint8_t n)
{
	uint8_t a = A;
	A = a - (n + ((F & F_C) >> 4));

	F = F_N;
	if (A == 0)
		F |= F_Z;
	// TODO other flags
}

void and (uint8_t n)
{
	A &= n;
	F = F_H;
	if (A == 0)
		F |= F_Z;
}

void or (uint8_t n)
{
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
	uint8_t v = A - n;
	F = F_N;
	if (v == 0)
		F |= F_Z;
	if (A < n)
		F |= F_C;
	// TODO other flags
}

void inc (uint8_t *n)
{
	uint8_t tmp = *n;
	(*n)++;
	F &= ~F_N;
	if (*n == 0)
		F |= F_Z;
	if ((tmp & 0x10) == 0 && (*n & 0x10) == 0x10)
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
	uint8_t tmp = *n;
	(*n)--;
	F &= ~F_N;
	if (*n == 0)
		F |= F_Z;
	if (!((tmp & 0x08) == 0 && (*n & 0x08) == 0x08))
		F |= F_H;
}

void swap (uint16_t* n)
{
	uint16_t tmp = (*n) & 0xff; // lower nibble
	*n >>= 8;
	*n |= (tmp << 8);
	F = 0;
	if (*n == 0)
		F |= F_Z;
}

void daa ()
{
	// TODO
}

void cpl ()
{
	A = ~A;
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
	// nada?
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

void rlc (uint8_t *n)
{
	uint8_t tmp = ((*n) & 0x80) >> 7;
	(*n) <<= 1;
	(*n) |= tmp;
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= tmp << 4; // C = old bit 7
	if ((*n) == 0)
		F &= ~F_Z;
}
#define rlca rlc(reg_a)

void rl (uint8_t *n)
{
	uint8_t tmp = ((*n) & 0x80) >> 7;
	(*n) <<= 1;
	(*n) |= (F >> 4) & 1; // bit 0 = C
	// reset flags
	F &= ~(F_N | F_H | F_C);
	F |= tmp << 4; // C = old bit 7
	if ((*n) == 0)
		F &= ~F_Z;
}
#define rla rl(reg_a)

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
#define rrca rrc(reg_a)

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
#define rra rr(reg_a)

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

void bit (uint8_t b, uint8_t* r)
{
	F &= ~F_N;
	F |= F_H;
	if (((1 << b) & *r) == 0)
		F |= F_Z;
}

void set(uint8_t b, uint8_t* r)
{
	(*r) |= 1 << b;
}

void res(uint8_t b, uint8_t* r)
{
	(*r) &= ~(1 << b);
}

//void jp (uint16_t nn)
//
//pc = nn;
//

#define jp(nn) pc = nn

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

//void jr (uint8_t n)
//{
	//pc += n;
//}

#define jr(n) pc += n

void jrcc (enum jump_cc cc, uint8_t n)
{
	switch (cc)
	{
		case JP_CC_NZ: if ((F & F_Z) == 0) jr (n); break;
		case JP_CC_Z:  if ((F & F_Z) != 0) jr (n); break;
		case JP_CC_NC: if ((F & F_C) == 0) jr (n); break;
		case JP_CC_C:  if ((F & F_C) != 0) jr (n); break;
	}
}

//void call (uint16_t nn)
//{
//PUSH (pc);
//pc = nn;
//}

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

void ret ()
{
	jp (POP ());
}

void reti ()
{
	jp (POP ());
	ime = 1;
}

/**
 * instruction defines a CPU instruction
 * Points to a function to be excecuted.
 */
typedef void(*instruction)() ;

/**
 * operation defines a specific CPU instruction to be excecuted
 * with a fix addressing mode.
 * It is identified by it's opcode.
 */
typedef
struct operation
{
	// name is the debugging name of the operation.
	const char* name;

	// the instruction to be executed
	instruction instruction;

	// number of bytes the operation consumes
	uint8_t bytes;

	// number of cycles the operation needs.
	// in some cases there might be more in the case of e.g. branching.
	uint8_t cycles;
}
operation;

/**
 * operations maps opcodes to operations.
 */
const operation operations[256] =
{

};

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
void interrupt ()
{
	// all interrupts disabled
	if (!ime) return;

	// loop over interrupt flags in priority order.
	// call any interrupts that have been flagged and enabled.
	for (uint8_t b = 0; b < 5; b ++)
	{
		if (IF & (1 << b) && IE & (1 << b))
		{
			ime = 0;
			PUSH (pc);
			pc = 0x40 + 0x8 * b;
		}
	}
}

/**
 * Reset the CPU.
 */
void gb_cpu_reset ()
{
	pc = 0x100;
	sp = 0xFFFE;
}

/**
 * Step the CPU, executing one operation.
 *
 * Returns the number of CPU cycles it took to perform the operation.
 */
int gb_cpu_step ()
{
	// first check any interrupts
	interrupt ();

	// load an opcode and perform the operation associated,
	// step the PC and clock the number of cycles
	uint8_t opcode = RAM (pc);
	operation op = operations[opcode];
	op.instruction ();
	pc += op.bytes;
	return op.cycles;
}
