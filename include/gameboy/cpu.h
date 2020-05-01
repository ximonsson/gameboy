#ifndef GB_CPU_H
#define GB_CPU_H

#include <stdint.h>

#define ROM_BANK_SIZE 0x4000

/**
 * Step the CPU one operation.
 */
int gb_cpu_step () ;

/**
 * Reset the CPU.
 */
void gb_cpu_reset () ;

/**
 * Get a pointer to a location within the RAM.
 * USE WITH CAUTION!
 */
uint8_t* gb_cpu_mem (uint16_t /* offset */) ;

/**
 * Load data into the ROM Bank.
 */
void gb_cpu_load_rom (uint8_t /* bank */, uint8_t* /* data */);

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

#endif
