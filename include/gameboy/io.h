#ifndef GB_IO_H
#define GB_IO_H

#define GB_IO_P1_LOC 0xFF00

/* reset the IO controller. */
void gb_io_reset () ;

/* joypad keys. */
typedef enum gb_io_button
{
	gb_io_b_right = 0,
	gb_io_b_left = 1,
	gb_io_b_up = 2,
	gb_io_b_down = 3,
	gb_io_b_a = 4,
	gb_io_b_b = 5,
	gb_io_b_select = 6,
	gb_io_b_start = 7,
}
gb_io_button;

/* emulate pressing the given button. */
void gb_io_press_button (gb_io_button /* button */);

/* emulate releasing the given button. */
void gb_io_release_button (gb_io_button /* button */);

#endif /* GB_IO_H */
