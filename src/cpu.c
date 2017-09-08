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

typedef int addressing;

enum addressing_modes {
	addressing_a,
	addressing_f,
	addressing_af,
	addressing_b,
	addressing_c,
	addressing_bc,
	addressing_d,
	addressing_e,
	addressing_de,
	addressing_h,
	addressing_l,
	addressing_hl
};

/* CPU Instructions */

/**
 * instruction defines a CPU instruction
 * Points to a function to be excecuted.
 */
typedef struct instruction
{
	// name for debugging purposes
	const char* name;

	// function to be excecute on the CPU
	// the function returns any extra amount of cycles the operation
	// took to execute.
	int (*fn) ();
}
instruction;

int ld ()
{
	return 0;
}

int push ()
{
	return 0;
}

int pop ()
{

}

/**
 * operation defines a specific CPU instruction to be excecuted
 * with a fix addressing mode.
 * It is identified by it's opcode.
 */
typedef struct operation
{
	// the instruction to be executed
	instruction instruction;

	// number of bytes the operation consumes
	uint8_t bytes;

	// number of cycles the operation needs.
	// in some cases this might be more in case of e.g. branching.
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

}

int gb_cpu_step ()
{
	pc ++;
	uint8_t opcode = RAM(pc);
	int cc = operations[opcode & 0xF][opcode >> 4].instruction.fn();
	return cc;
}
