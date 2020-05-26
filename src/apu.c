#include "gameboy/cpu.h"
#include <stdint.h>

/* Registers ------------------------------------------------ */

/**
 * NR10 - Channel 1 sweep (R/W)
 * Bit 6-4 - Sweep Time
 * Bit 3   - Sweep Increase/Decrease
 *           0: Addition    (frequency increases)
 *           1: Subtraction (frequency decreases)
 * Bit 2-0 - Number of sweep shift (n: 0-7)
 *
 * 000: sweep off - no freq change
 * 001: 7.8 ms  (1/128Hz)
 * 010: 15.6 ms (2/128Hz)
 * 011: 23.4 ms (3/128Hz)
 * 100: 31.3 ms (4/128Hz)
 * 101: 39.1 ms (5/128Hz)
 * 110: 46.9 ms (6/128Hz)
 * 111: 54.7 ms (7/128Hz)
 *
 * The change of frequency (NR13,NR14) at each shift is calculated by the following formula where X(0) is
 * initial freq & X(t-1) is last freq:
 *   X(t) = X(t-1) +/- X(t-1)/2^n`
 */
static uint8_t* nr10;

/**
 * Bit 7-6 - Wave Pattern Duty (Read/Write)
 * Bit 5-0 - Sound length data (Write Only) (t1: 0-63)
 *
 * Wave Duty:
 *
 * 00: 12.5% ( _-------_-------_------- )
 * 01: 25%   ( __------__------__------ )
 * 10: 50%   ( ____----____----____---- ) (normal)
 * 11: 75%   ( ______--______--______-- )
 *
 * Sound Length = (64-t1)*(1/256) seconds The Length value is used only if Bit 6 in NR14 is set.
 */
static uint8_t* nr11;

/**
 * Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
 * Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
 * Bit 2-0 - Number of envelope sweep (n: 0-7)
 *           (If zero, stop envelope operation.)
 *
 * Length of 1 step = n*(1/64) seconds
 */
static uint8_t* nr12;

/**
 * Lower 8 bits of 11 bit frequency (x). Next 3 bit are in NR14 ($FF14)
 */
static uint8_t* nr13;

/**
 * Bit 7   - Initial (1=Restart Sound)     (Write Only)`
 * Bit 6   - Counter/consecutive selection (Read/Write)`
 *          (1=Stop output when length in NR11 expires)`
 * Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)`
 *
 * Frequency = 131072/(2048-x) Hz
 */
static uint8_t* nr14;

static uint8_t* nr21;
static uint8_t* nr22;
static uint8_t* nr23;
static uint8_t* nr24;

/**
 * Bit 7 - Sound Channel 3 Off  (0=Stop, 1=Playback)  (Read/Write)
 */
static uint8_t* nr30;

/**
 *   Bit 7-0 - Sound length (t1: 0 - 255)`
 * Sound Length = (256-t1)*(1/256) seconds This value is used only if Bit 6 in NR34 is set.
 */
static uint8_t* nr31;

/**
 * Bit 6-5 - Select output level (Read/Write)`
 *
 * Possible Output levels are:
 * 0: Mute (No sound)`
 * 1: 100% Volume (Produce Wave Pattern RAM Data as it is)`
 * 2:  50% Volume (Produce Wave Pattern RAM data shifted once to the right)`
 * 3:  25% Volume (Produce Wave Pattern RAM data shifted twice to the right)`
 */
static uint8_t* nr32;

/**
 * Lower 8 bits of an 11 bit frequency (x).
 */
static uint8_t* nr33;

/**
 * Bit 7   - Initial (1=Restart Sound)     (Write Only)`
 * Bit 6   - Counter/consecutive selection (Read/Write)`
 *          (1=Stop output when length in NR31 expires)`
 * Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)`
 *
 * Frequency = 4194304/(64*(2048-x)) Hz = 65536/(2048-x) Hz
 */
static uint8_t* nr34;

/**
 * Contents - Waveform storage for arbitrary sound data`
 * This storage area holds 32 4-bit samples that are played back, upper 4 bits first.
 *
 * Wave RAM should only be accessed while CH3 is disabled (NR30 bit 7 reset), otherwise accesses will behave weirdly.
 *
 * On almost all models, the byte will be written at the offset CH3 is currently reading.
 * On GBA, the write will simply be ignored.
 */
static uint8_t* wave_pat;

/**
 * Bit 5-0 - Sound length data (t1: 0-63)
 *
 * Sound Length = (64-t1)*(1/256) seconds The Length value is used only if Bit 6 in NR44 is set.
 */
static uint8_t* nr41;

/**
 *  Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
 *  Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
 *  Bit 2-0 - Number of envelope sweep (n: 0-7)`
 *            (If zero, stop envelope operation.)

 * Length of 1 step = n*(1/64) seconds
 */
static uint8_t* nr42;

/**
 * The amplitude is randomly switched between high and low at the given frequency.
 * A higher frequency will make the noise to appear 'softer'. When Bit 3 is set, the output
 * will become more regular, and some frequencies will sound more like Tone than Noise.
 *
 * Bit 7-4 - Shift Clock Frequency (s)
 * Bit 3   - Counter Step/Width (0=15 bits, 1=7 bits)
 * Bit 2-0 - Dividing Ratio of Frequencies (r)
 *
 * Frequency = 524288 Hz / r / 2^(s+1) ;For r=0 assume r=0.5 instead
 */
static uint8_t* nr43;

/**
 * Bit 7   - Initial (1=Restart Sound)     (Write Only)
 * Bit 6   - Counter/consecutive selection (Read/Write)
 *           (1=Stop output when length in NR41 expires)
 */
static uint8_t* nr44;

/**
 * The volume bits specify the "Master Volume" for Left/Right sound output.
 * SO2 goes to the left headphone, and SO1 goes to the right.
 *
 *  Bit 7   - Output Vin to SO2 terminal (1=Enable)
 *  Bit 6-4 - SO2 output level (volume)  (0-7)
 *  Bit 3   - Output Vin to SO1 terminal (1=Enable)
 *  Bit 2-0 - SO1 output level (volume)  (0-7)
 *
 *  The Vin signal is an analog signal received from the game cartridge bus, allowing external hardware
 *  in the cartridge to supply a fifth sound channel, additionally to the Game Boy's internal four channels.
 *  No licensed games used this feature, and it was omitted from the Game Boy Advance.
 *
 *  (Despite rumors, Pocket Music does not use Vin. It blocks use on the GBA for a different reason:
 *  the developer couldn't figure out how to silence buzzing associated with the wave channel's DAC.)
 */
static uint8_t* nr50;

/**
 * Each channel can be panned hard left, center, or hard right.
 *
 *  Bit 7 - Output sound 4 to SO2 terminal
 *  Bit 6 - Output sound 3 to SO2 terminal
 *  Bit 5 - Output sound 2 to SO2 terminal
 *  Bit 4 - Output sound 1 to SO2 terminal
 *  Bit 3 - Output sound 4 to SO1 terminal
 *  Bit 2 - Output sound 3 to SO1 terminal
 *  Bit 1 - Output sound 2 to SO1 terminal
 *  Bit 0 - Output sound 1 to SO1 terminal
 */
static uint8_t* nr51;

/**
 * If your GB programs don't use sound then write 00h to this register to save 16% or more on GB power consumption.
 * Disabeling the sound controller by clearing Bit 7 destroys the contents of all sound registers.
 * Also, it is not possible to access any sound registers (execpt FF26) while the sound controller is disabled.
 *
 *  Bit 7 - All sound on/off  (0: stop all sound circuits) (Read/Write)
 *  Bit 3 - Sound 4 ON flag (Read Only)
 *  Bit 2 - Sound 3 ON flag (Read Only)
 *  Bit 1 - Sound 2 ON flag (Read Only)
 *  Bit 0 - Sound 1 ON flag (Read Only)
 *
 * Bits 0-3 of this register are read only status bits, writing to these bits does NOT enable/disable sound.
 * The flags get set when sound output is restarted by setting the Initial flag (Bit 7 in NR14-NR44),
 * the flag remains set until the sound length has expired (if enabled). A volume envelopes which has
 * decreased to zero volume will NOT cause the sound flag to go off.
 */
static uint8_t* nr52;

#define NR10 (* nr10)
#define NR11 (* nr11)
#define NR12 (* nr12)
#define NR13 (* nr13)
#define NR14 (* nr14)

#define NR21 (* nr21)
#define NR22 (* nr22)
#define NR23 (* nr23)
#define NR24 (* nr24)

#define NR30 (* nr30)
#define NR31 (* nr31)
#define NR32 (* nr32)
#define NR33 (* nr33)
#define NR34 (* nr34)

#define NR41 (* nr41)
#define NR42 (* nr42)
#define NR43 (* nr43)
#define NR44 (* nr44)

#define NR50 (* nr50)
#define NR51 (* nr51)
#define NR52 (* nr52)

/* Channels ------------------------------------------------- */


typedef
struct channel_square
{

}
channel_square;



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

static void step ()
{

}

void gb_apu_step (int cc)
{
	for (; cc > 0; cc --) step ();
}
