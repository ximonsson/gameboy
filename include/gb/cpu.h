#ifndef GB_CPU_H
#define GB_CPU_H

#include <stdint.h>

#define ROM_BANK_SIZE 0x4000
#define RAM_BANK_SIZE 0x2000
#define OAM_LOC 0xFE00

#define GB_DIV_CLOCK 16384
#define GB_DIV_CC 256 // CPU CLOCK / DIV CLOCK

/**
 * Step the CPU one operation.
 */
int gb_cpu_step () ;

/**
 * Reset the CPU.
 *
 * Pass 0 or 1 for DMG compability mode.
 */
void gb_cpu_reset (uint8_t) ;

/**
 * Get a pointer to a location within the RAM.
 * USE WITH CAUTION!
 */
uint8_t* gb_cpu_mem (uint16_t /* offset */) ;

/**
 * Load data total ROM data and give the number of banks.
 */
void gb_cpu_load_rom (int /* banks */, const uint8_t* /* data */) ;

/**
 * Switch ROM bank one with the given bank number (of total in ROM).
 */
void gb_cpu_switch_rom_bank (int /* bank */);

/**
 * Load data into RAM bank.
 */
void gb_cpu_load_ram (uint8_t* /* data */) ;

/**
 * Read from RAM handler.
 *
 * This is a function that takes the address as first parameter and the current value in memory
 * at that address.
 *
 * In case the handler affects the RAM address and normal execution should be stopped an non-zero
 * value should be returned by the handler, else zero.
 */
typedef int (*read_handler) (uint16_t, uint8_t*) ;

/**
 * Add a callback (read_handler) for when trying to read from memory.
 */
void gb_cpu_register_read_handler (read_handler /* handler */) ;

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

/**
 * Add a callback when storing to RAM.
 */
void gb_cpu_register_store_handler (store_handler /* handler */) ;

/**
 * Bit 0: V-Blank  Interrupt Enable  (INT 40h)  (1=Enable)
 * Bit 1: LCD STAT Interrupt Enable  (INT 48h)  (1=Enable)
 * Bit 2: Timer    Interrupt Enable  (INT 50h)  (1=Enable)
 * Bit 3: Serial   Interrupt Enable  (INT 58h)  (1=Enable)
 * Bit 4: Joypad   Interrupt Enable  (INT 60h)  (1=Enable)
*/
typedef
enum interrupt_flag
{
	INT_FLAG_VBLANK   = 0x01,
	INT_FLAG_LCD_STAT = 0x02,
	INT_FLAG_TIMER    = 0x04,
	INT_FLAG_SERIAL   = 0x08,
	INT_FLAG_JOYPAD   = 0x10,
}
interrupt_flag;

void gb_cpu_flag_interrupt (interrupt_flag /* flag */) ;

#endif
