#include "gameboy/cpu.h"
#include "gameboy/apu.h"
#include <stdint.h>
#include <string.h>

/* Registers ------------------------------------------------ */

/**
 * FF10 - NR10 - Channel 1 sweep (R/W)
 *
 *   Bit 6-4 - Sweep Time
 *   Bit 3   - Sweep Increase/Decrease
 *             0: Addition    (frequency increases)
 *             1: Subtraction (frequency decreases)
 *   Bit 2-0 - Number of sweep shift (n: 0-7)
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
 * FF11 - NR11 - Channel 1 Sound length/Wave pattern duty (R/W)
 *
 *   Bit 7-6 - Wave Pattern Duty (Read/Write)
 *   Bit 5-0 - Sound length data (Write Only) (t1: 0-63)
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
 * FF12 - NR12 - Channel 1 Volume Envelope (R/W)
 *
 *   Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
 *   Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
 *   Bit 2-0 - Number of envelope sweep (n: 0-7)
 *             (If zero, stop envelope operation.)
 *
 * Length of 1 step = n*(1/64) seconds
 */
static uint8_t* nr12;

/**
 * FF13 - NR13 - Channel 1 Frequency lo (Write Only)
 *
 * Lower 8 bits of 11 bit frequency (x). Next 3 bit are in NR14 ($FF14)
 */
static uint8_t* nr13;

/**
 * FF14 - NR14 - Channel 1 Frequency hi (R/W)
 *
 *   Bit 7   - Initial (1=Restart Sound)     (Write Only)`
 *   Bit 6   - Counter/consecutive selection (Read/Write)`
 *            (1=Stop output when length in NR11 expires)`
 *   Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)`
 *
 * Frequency = 131072/(2048-x) Hz
 */
static uint8_t* nr14;

/* FF16 - NR21 - Channel 2 Sound Length/Wave Pattern Duty (R/W) */
static uint8_t* nr21;
/* FF17 - NR22 - Channel 2 Volume Envelope (R/W) */
static uint8_t* nr22;
/* FF18 - NR23 - Channel 2 Frequency lo data (W) */
static uint8_t* nr23;
/* FF19 - NR24 - Channel 2 Frequency hi data (R/W) */
static uint8_t* nr24;

/**
 * FF1A - NR30 - Channel 3 Sound on/off (R/W)
 *
 *   Bit 7 - Sound Channel 3 Off  (0=Stop, 1=Playback)  (Read/Write)
 */
static uint8_t* nr30;

/**
 * FF1B - NR31 - Channel 3 Sound Length
 *
 *   Bit 7-0 - Sound length (t1: 0 - 255)`
 *
 * Sound Length = (256-t1)*(1/256) seconds This value is used only if Bit 6 in NR34 is set.
 */
static uint8_t* nr31;

/**
 * FF1C - NR32 - Channel 3 Select output level (R/W)
 *
 *   Bit 6-5 - Select output level (Read/Write)`
 *
 * Possible Output levels are:
 * 0: Mute (No sound)`
 * 1: 100% Volume (Produce Wave Pattern RAM Data as it is)`
 * 2:  50% Volume (Produce Wave Pattern RAM data shifted once to the right)`
 * 3:  25% Volume (Produce Wave Pattern RAM data shifted twice to the right)`
 */
static uint8_t* nr32;

/**
 * FF1D - NR33 - Channel 3 Frequency's lower data (W)
 *
 * Lower 8 bits of an 11 bit frequency (x).
 */
static uint8_t* nr33;

/**
 * FF1E - NR34 - Channel 3 Frequency's higher data (R/W)
 *
 *   Bit 7   - Initial (1=Restart Sound)     (Write Only)`
 *   Bit 6   - Counter/consecutive selection (Read/Write)`
 *            (1=Stop output when length in NR31 expires)`
 *   Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)`
 *
 * Frequency = 4194304/(64*(2048-x)) Hz = 65536/(2048-x) Hz
 */
static uint8_t* nr34;

/**
 * FF30-FF3F - Wave Pattern RAM
 *
 * Contents - Waveform storage for arbitrary sound data`
 * This storage area holds 32 4-bit samples that are played back, upper 4 bits first.
 *
 * Wave RAM should only be accessed while CH3 is disabled (NR30 bit 7 reset), otherwise accesses will behave weirdly.
 *
 * On almost all models, the byte will be written at the offset CH3 is currently reading.
 * On GBA, the write will simply be ignored.
 */
static uint8_t* wav_pat;

/**
 * FF20 - NR41 - Channel 4 Sound Length (R/W)
 *
 *   Bit 5-0 - Sound length data (t1: 0-63)
 *
 * Sound Length = (64-t1)*(1/256) seconds The Length value is used only if Bit 6 in NR44 is set.
 */
static uint8_t* nr41;

/**
 * FF21 - NR42 - Channel 4 Volume Envelope (R/W)
 *
 *   Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
 *   Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
 *   Bit 2-0 - Number of envelope sweep (n: 0-7)`
 *             (If zero, stop envelope operation.)

 * Length of 1 step = n*(1/64) seconds
 */
static uint8_t* nr42;

/**
 * FF22 - NR43 - Channel 4 Polynomial Counter (R/W)
 *
 * The amplitude is randomly switched between high and low at the given frequency.
 * A higher frequency will make the noise to appear 'softer'. When Bit 3 is set, the output
 * will become more regular, and some frequencies will sound more like Tone than Noise.
 *
 *   Bit 7-4 - Shift Clock Frequency (s)
 *   Bit 3   - Counter Step/Width (0=15 bits, 1=7 bits)
 *   Bit 2-0 - Dividing Ratio of Frequencies (r)
 *
 * Frequency = 524288 Hz / r / 2^(s+1) ;For r=0 assume r=0.5 instead
 */
static uint8_t* nr43;

/**
 * FF23 - NR44 - Channel 4 Counter/consecutive; Inital (R/W)
 *
 *   Bit 7   - Initial (1=Restart Sound)     (Write Only)
 *   Bit 6   - Counter/consecutive selection (Read/Write)
 *             (1=Stop output when length in NR41 expires)
 */
static uint8_t* nr44;

/**
 * FF24 - NR50 - Channel control / ON-OFF / Volume (R/W)
 *
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
 * FF25 - NR51 - Selection of Sound output terminal (R/W)
 *
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
 * FF26 - NR52 - Sound on/off
 *
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

/* Duty patterns. */
static uint8_t sqr_wav[4] =
{
	0x7F, // _-------
	0x3F, // __------
	0x0F, // ____----
	0x03, // ______--
};

#define FREQ(x) ((2048 - x) << 5) // (GB_CPU_CLOCK / (131072 / (2048 - x)))
#define SOUNDLEN(t1) (64 - t1) // (64 - t1) * 256 Hz
#define ENVLEN(n) n // n * 64 Hz

/* Channels ------------------------------------------------- */

/* Flag enabled channels. */
static uint8_t enabled_ch;

#define ENABLE_CH(c) (enabled_ch |= (1 << c))
#define DISABLE_CH(c) (enabled_ch &= ~(1 << c))
#define ENABLED(c) ((enabled_ch & (1 << c)) != 0)

/* Channel 1: Tone + Sweep */

#define CH1FREQ FREQ ((((NR14 & 0x07) << 8) | NR13))
#define CH1DUTY sqr_wav[(NR11 >> 6)]
#define CH1LEN SOUNDLEN ((NR11 & 0x3F))

/* Channel 1 timer. */
static int ch1_cc;

/* Channel 1 duty counter. */
static uint8_t ch1_duty_cc;

/* Step Channel 1 timer. */
static inline void step_timer_ch1 ()
{
	if (-- ch1_cc == 0)
	{
		ch1_cc = CH1FREQ;
		ch1_duty_cc ++; ch1_duty_cc &= 0x07;
	}
}

/* Channel 1 length counter. */
static int ch1_len;

/* Step channel 1 length counter. */
static inline void step_len_ch1 ()
{
	if ((NR14 & 0x40) == 0)
		return; // length disabled
	else if (-- ch1_len == 0)
		DISABLE_CH (1); // Disable
}

/* Step channel 1 sweep. */
static inline void step_sweep_ch1 () { }

/* Channel 1 envelop counter. */
static uint8_t ch1_envcc;

/* Channel 1 volume. */
static uint8_t ch1_vol;

/* Step channel 2's envelop. */
static inline void step_env_ch1 ()
{
	if (-- ch1_envcc == 0)
	{
		// reset counter
		ch1_envcc = NR12 & 0x07;
		if (!ch1_envcc) ch1_envcc = 8; // TODO w00t?

		// change volume
		if (ch1_vol > 0 && ch1_vol < 15)
			ch1_vol += NR12 & 0x08 ? 1 : -1;
	}
}

static inline float ch1sample ()
{
	float s = !ENABLED (1) ? 0.0 : (CH1DUTY >> (ch1_duty_cc >> 3)) & 1;
	return s * ch1_vol / 100;
}

/* Channel 2: Tone */

#define CH2DUTY sqr_wav[(NR21 >> 6)]
#define CH2LEN SOUNDLEN ((NR21 & 0x3F))
#define CH2FREQ FREQ ((((NR24 & 0x07) << 8) | NR23))

/* Channel 2 timer. */
static int ch2_cc;

/* Channel 2 duty counter. */
static uint8_t ch2_duty_cc;

/* Step Channel 2. */
static inline void step_timer_ch2 ()
{
	if (-- ch2_cc == 0)
	{
		ch2_cc = CH2FREQ;
		ch2_duty_cc ++; ch2_duty_cc &= 0x07;
	}
}

/* Channel 2 length counter. */
static int ch2_len;

/* Step channel 2's length counter. */
static inline void step_len_ch2 ()
{
	if ((NR24 & 0x40) == 0)
		return; // length disabled
	else if (-- ch2_len == 0)
		DISABLE_CH (2); // Disable
}

/* Channel 2 envelop counter. */
static uint8_t ch2_envcc;

/* Channel 2 volume. */
static uint8_t ch2_vol;

/* Step channel 2's envelop. */
static inline void step_env_ch2 ()
{
	if (-- ch2_envcc == 0)
	{
		// reset counter
		ch2_envcc = NR22 & 0x07;
		if (!ch2_envcc) ch2_envcc = 8; // TODO w00t?

		// change volume
		if (ch2_vol > 0 && ch2_vol < 15)
			ch2_vol += NR22 & 0x08 ? 1 : -1;
	}
}

static inline float ch2sample ()
{
	float s = !ENABLED (2) ? 0.0 : (CH2DUTY >> (ch2_duty_cc >> 3)) & 1;
	return s * ch2_vol / 100;
}

/* Step Wave channel. */
static inline void step_timer_wav () {}
static inline void step_len_wav () {}

/* Step Noise channel. */
static inline void step_timer_noi () { }
static inline void step_len_noi () { }
static inline void step_env_noi () { }

/* Frame sequencer. */
static uint8_t fs;

/* Step Frame Sequencer. */
static inline void step_fs ()
{
	fs ++; fs &= 0x07;

	if ((fs & 1) == 0) // even step - clock length counters
	{
		step_len_ch1 ();
		step_len_ch2 ();
		step_len_wav ();
		step_len_noi ();

		if (fs & 0x02) // fs == 2 or 6
			step_sweep_ch1 ();
	}
	else if (fs == 7)
	{
		step_env_ch1 ();
		step_env_ch2 ();
		step_env_noi ();
	}
}

/* Frame sequencer timer */
static int fs_cc;

#define FSFREQ 8192 // CPU FREQ / 512 Hz

static inline void step_timer_fs ()
{
	if (-- fs_cc == 0)
	{
		step_fs ();
		fs_cc = FSFREQ;
	}
}

static inline void step ()
{
	step_timer_ch1 ();
	step_timer_ch2 ();
	step_timer_wav ();
	step_timer_noi ();

	step_timer_fs ();
}

static inline float sample ()
{
	// TODO
	// right now we are just getting a value from channel 2 to see if it works
	return ch1sample ();
}

static int sample_rate;
static float samples[2048];
static int samples_len;
static int apucc;

void gb_apu_samples (float* buf, size_t* n)
{
	*n = samples_len * sizeof (float);
	memcpy (buf, samples, *n);
	samples_len = 0;
}

void gb_apu_step (int cc)
{
	for (; cc > 0; cc --)
	{
		step ();
		if (sample_rate && ++ apucc == sample_rate)
		{
			samples[samples_len ++] = sample ();
			apucc = 0;
		}
	}
}

#include <stdio.h>

static int write_ch2 (uint16_t adr, uint8_t v)
{
	//printf ("$%.4X - 0x0015 => %d\n", adr, adr - 0x0015);
	switch (adr - 0x0015)
	{
		case 4:
			if (v & 0x80)
			{
				ENABLE_CH (2);
				if (ch2_len == 0) ch2_len = 64;
				ch2_cc = CH2FREQ;
				ch2_envcc = NR22 & 0x07;
				ch2_vol = NR22 >> 4;
			}
			return 1;
	}
	return 0;
}

static int write_ch1 (uint16_t adr, uint8_t v)
{
	//printf ("$%.4X - 0x0015 => %d\n", adr, adr - 0x0015);
	switch (adr - 0x0010)
	{
		case 4:
			if (v & 0x80)
			{
				ENABLE_CH (1);
				if (ch1_len == 0) ch1_len = 64;
				ch1_cc = CH1FREQ;
				ch1_envcc = NR12 & 0x07;
				ch1_vol = NR12 >> 4;
			}
			return 1;
	}
	return 0;
}

static int write_apu_h (uint16_t adr, uint8_t v)
{
	// TODO
	// not sure if one needs to handle the unused the registers.

	if (adr < 0xFF10 || adr > 0xFF26) return 0;

	// TODO
	// make this cleaner when you are less in a hurry
	// maybe a function array

	adr &= 0x00FF;

	if (adr < 0x15)
	{
		// write channel 1
		return write_ch1 (adr, v);
	}
	else if (adr < 0x1A)
	{
		// write channel 2
		return write_ch2 (adr, v);
	}
	else if (adr < 0x20)
	{
		// write wave
		return 0;
	}
	else if (adr < 0x27)
	{
		// write noise
		return 0;
	}

	return 1;
}

static int read_apu_h (uint16_t adr, uint8_t* v)
{
	if (adr < 0xFF10 || adr > 0xFF26) return 0;

	// TODO
	// make this cleaner when you less in a hurry
	// maybe a function array

	adr &= 0x00FF;

	if (adr < 0x15)
	{
		// write channel 1
		return 0;
	}
	else if (adr < 0x1A)
	{
		// write channel 2
		return 0;
	}
	else if (adr < 0x20)
	{
		// write wave
	}
	else if (adr < 0x27)
	{
		// write noise
	}

	return 1;
}

void gb_apu_reset (int sample_rate_)
{
	sample_rate = GB_CPU_CLOCK / sample_rate_; // TODO try to be more precise
	samples_len = 0;

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
	wav_pat = gb_cpu_mem (0xFF30);

	nr41 = gb_cpu_mem (0xFF20);
	nr42 = gb_cpu_mem (0xFF21);
	nr43 = gb_cpu_mem (0xFF22);
	nr44 = gb_cpu_mem (0xFF23);

	nr50 = gb_cpu_mem (0xFF24);
	nr51 = gb_cpu_mem (0xFF25);
	nr52 = gb_cpu_mem (0xFF26);

	gb_cpu_register_store_handler (write_apu_h);

	// TODO
	// reset all timers
	fs = 0;
}
