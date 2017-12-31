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
#define A *reg_a
#define F *reg_f

#define BC reg_bc
#define B *reg_b
#define C *reg_c

#define DE reg_de
#define D *reg_d
#define E *reg_e

#define HL reg_hl
#define H *reg_h
#define L *reg_l

/* define flags */
enum flags
{
	F_Z = 0x80,
	F_N = 0x40,
	F_H = 0x20,
	F_C = 0x10,
};

static uint8_t ram[0xFFFF];

typedef int (*read_handler) (uint16_t, uint8_t*);
static read_handler* read_handlers = 0;
static int n_read_handlers = 0;

void gb_cpu_add_read_handler (read_handler h)
{
	read_handlers[n_read_handlers] = h;
	n_read_handlers ++;
}

static uint8_t read_ram (uint16_t address)
{
	uint8_t v = ram[address];
	int stop = 0;
	for (read_handler* h = read_handlers; h != 0 && !stop; h ++)
	{
		stop = (*h)(address, &v);
	}
	return v;
}

#define RAM(a) read_ram(a)

typedef int (*store_handler) (uint16_t, uint8_t);
static store_handler* store_handlers = 0;
static int n_store_handlers = 0;

void gb_cpu_add_store_handler (store_handler h)
{
	store_handlers[n_store_handlers] = h;
	n_store_handlers ++;
}

static void store_ram (uint16_t address, uint8_t v)
{
	int stop = 0;
	for (store_handler* h = store_handlers; h != 0 && !stop; h ++)
	{
		stop = (*h)(address, v);
	}
	if (!stop) // if we didn't break the loop we can store to RAM @ address.
		ram[address] = v;
}

#define STORE(a, v) store_ram(a, v)

/**
 * stack_push pushes the value v to the stack.
 */
void stack_push (uint16_t v)
{
	ram[sp] = v >> 8;
	sp --;
	ram[sp] = v;
	sp --;
}

#define PUSH(v) stack_push(v)

/**
 * stack_pop pops the stack and returns the value;
 */
uint16_t stack_pop ()
{
	sp ++;
	uint16_t lo = ram[sp];
	sp ++;
	uint16_t hi = ram[sp];
	return (hi << 8) | lo;
}

#define POP() stack_pop()


/* CPU Flags (register F) */
enum cpu_flags
{
	zf = 0x80, // zero
	n  = 0x40, // negative
	h  = 0x20, // half carry
	cy = 0x10  // carry
	// bits 0-3 are unused
};

/* CPU Instructions */

/**
 * instruction defines a CPU instruction
 * Points to a function to be excecuted.
 */
typedef void(*instruction)() ;

void ld8 (uint8_t* d, uint8_t v)
{
	*d = v;
}

void ld16 (uint8_t* d, uint16_t v)
{
	uint8_t _v = v;
	ld8 (d, _v);
	_v = v >> 8;
	ld8 (d + 1, _v);
}

void push (uint16_t v)
{
	stack_push (v);
}

void pop (uint16_t *v)
{
	stack_pop (v);
}

void add (uint8_t n)
{
	uint8_t a = A;
	A = a + n;
	F = 0; // reset flags
	if (A == 0)
		F |= F_Z;
	if (a & 0x80 != 0 && (A) & 0x80 == 0) // carry
		F |= F_C;
	if (a & 0x08 != 0 && (A) & 0x08 == 0) // half carry
		F |= F_H;
}

void adc (uint8_t n)
{
	uint8_t a = A;
	A = a + n + ((F & F_C) >> 4);
	F = 0; // reset flags
	if (A == 0)
		F |= F_Z;
	if (a & 0x80 != 0 && (A) & 0x80 == 0) // carry
		F |= F_C;
	if (a & 0x08 != 0 && (A) & 0x08 == 0) // half carry
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
	A = (A) & n;
	F = F_H;
	if (A == 0)
		F |= F_Z;
}

/**
 * operation defines a specific CPU instruction to be excecuted
 * with a fix addressing mode.
 * It is identified by it's opcode.
 */
typedef struct operation
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
const operation operations[16][16] =
{

};

void gb_cpu_reset ()
{
	pc = 0x100;
}

int gb_cpu_step ()
{
	uint8_t opcode = RAM (pc);
	operation op = operations[opcode & 0xF][opcode >> 4];
	op.instruction();
	pc += op.bytes;
	return op.cycles;
}
