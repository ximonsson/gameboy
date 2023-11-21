#include "gb/cpu.h"
#include "gb/apu.h"
#include "gb.h"
#include <stdint.h>
#include <string.h>

#include <stdio.h>

/**
 * Channel envelope handling volume.
 * Keeps a pointer to the register with the information for the specific envelope.
 */
typedef
struct envelope
{
	// counter
	uint8_t cc;

	// register
	uint8_t* R;

	// enabled flag
	uint8_t enabled;

	// volume
	uint8_t vol;
}
envelope;

#define ENV_PERIOD(e) ((*e->R) & 0x07)
#define ENV_INC(e) ((*e->R) & 0x08)
#define ENV_VOL(e) ((*e->R) >> 4)

/* Step the envelope one tick. */
void envelope_step (envelope* e)
{
	// not enabled or period is zero
	if (!e->enabled || !((*e->R) & 0x07))
		return;

	if (-- e->cc == 0)
	{
		e->cc = ENV_PERIOD (e);

		// TODO
		// I am unsure if we are to set cc = 8 in case it is zero here
		// Other parts of the docs indicate that the envelope is not active
		// in case the period is zero.

		uint8_t vol = e->vol + (ENV_INC (e) ? 1 : -1);

		if (vol >= 0 && vol <= 15)
			e->vol = vol;
		else
			e->enabled = 0;
	}
}

/* Reset the envelope. */
void envelope_reset (envelope* e)
{
	e->enabled = 1;
	e->vol = ENV_VOL (e);
	e->cc = ENV_PERIOD (e);
}

/* Initialize and reset the envelope. */
void envelope_init (envelope* e, uint8_t* r)
{
	e->R = r;
	envelope_reset (e);
}

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

#define APU_OFF (~NR52 & 0x80)

/* Duty patterns. */
static uint8_t sqr_wav[4] =
{
	0x7F, // _-------
	0x3F, // __------
	0x0F, // ____----
	0x03, // ______--
};

#define FREQ(x) ((2048 - x) << 2) // (GB_CPU_CLOCK / (131072 / (2048 - x)))
#define SOUNDLEN(t1) (64 - t1) // (64 - t1) * 256 Hz
#define ENVLEN(n) n // n * 64 Hz

/* Channels ------------------------------------------------- */

/* Flag enabled channels. */
static uint8_t enabled_ch;

#define ENABLE_CH(c) (enabled_ch |= (1 << (c - 1)))
#define DISABLE_CH(c) (enabled_ch &= ~(1 << (c - 1)))
#define ENABLED(c) ((enabled_ch & (1 << (c - 1))) != 0)

/* Channel 1: Tone + Sweep */

#define CH1FREQ FREQ ((((NR14 & 0x07) << 8) | NR13))
#define CH1DUTY sqr_wav[(NR11 >> 6)]
#define CH1LEN SOUNDLEN ((NR11 & 0x3F))

/* Channel 1 timer. */
static uint16_t ch1_cc;

/* Channel 1 duty counter. */
static uint8_t ch1_duty_cc;

/* Step Channel 1 timer. */
static void step_timer_ch1 ()
{
	if (-- ch1_cc <= 0)
	{
		ch1_cc = CH1FREQ;
		ch1_duty_cc ++; ch1_duty_cc &= 0x07;
	}
}

/* Channel 1 length counter. */
static uint8_t ch1_len;

/* Step channel 1 length counter. */
static void step_len_ch1 ()
{
	if ((~NR14 & 0x40) || !ch1_len)
		return; // length disabled

	if (-- ch1_len == 0)
		DISABLE_CH (1); // Disable
}

#define CH1SWEEP ((NR10 >> 4) & 0x07)
#define CH1SWEEP_ENABLED (NR10 & 0x77)
#define CH1SWEEP_SHIFT (NR10 & 0x07)

#define SWEEP_OVERFLOW 2047

static uint16_t ch1_shadow;

static int16_t sweep ()
{
	int16_t freq = ch1_shadow >> CH1SWEEP_SHIFT;
	if (NR10 & 0x08)
		freq = - freq;
	freq = ch1_shadow + freq;

	if (freq > SWEEP_OVERFLOW) // overflow
		DISABLE_CH (1);

	return freq;
}

static uint8_t ch1_sweep_cc;

/* Step channel 1 sweep. */
static void step_sweep_ch1 ()
{
	if (-- ch1_sweep_cc > 0)
		return;

	ch1_sweep_cc = CH1SWEEP;
	if (!ch1_sweep_cc)
		ch1_sweep_cc = 8;

	if (!CH1SWEEP_ENABLED) return;

	int16_t freq = sweep ();

	if (freq <= SWEEP_OVERFLOW)
	{
		ch1_shadow = freq;
		NR13 = freq & 0xFF;
		NR14 = (NR14 & ~0x07) | ((freq >> 8) & 0x07);
	}

	sweep ();
}

static envelope ch1_env;

static uint8_t ch1sample ()
{
	if (! ENABLED (1)) return 0;
	uint8_t s = (CH1DUTY >> ch1_duty_cc) & 1;
	return s * ch1_env.vol;
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
static void step_timer_ch2 ()
{
	if (-- ch2_cc == 0)
	{
		ch2_cc = CH2FREQ;
		ch2_duty_cc ++; ch2_duty_cc &= 0x07;
	}
}

/* Channel 2 length counter. */
static uint8_t ch2_len;

/* Step channel 2's length counter. */
static void step_len_ch2 ()
{
	if ((~NR24 & 0x40) || !ch2_len)
		return; // length disabled
	else if (-- ch2_len == 0)
		DISABLE_CH (2); // Disable
}

static envelope ch2_env;

static uint8_t ch2sample ()
{
	if (! ENABLED (2)) return 0;

	uint8_t s = (CH2DUTY >> ch2_duty_cc) & 1;
	return s * ch2_env.vol;
}

#define WAVFREQ_(x) ((2048 - x) << 1) // (GB_CPU_CLOCK / (65536 / (2048 - x)))
#define WAVFREQ WAVFREQ_ ((((NR34 & 0x07) << 8) | NR33))
#define WAVLEN (256 - NR31)

static int wav_cc;
static uint8_t wav_duty;

/* Step Wave channel. */
static void step_timer_wav ()
{
	if (-- wav_cc <= 0)
	{
		wav_duty ++; wav_duty &= 0x1F;
		wav_cc = WAVFREQ;
	}
}

static uint16_t wav_len;

static void step_len_wav ()
{
	if ((~NR34 & 0x40) || !wav_len)
		return;
	if (-- wav_len == 0)
		DISABLE_CH (3);
}

static uint8_t wavsample ()
{
	// not enabled or volume is 0%
	if (!ENABLED (3) || (NR32 & 0x60) == 0 || (NR30 & 0x80) == 0)
		return 0;

	uint8_t s = wav_pat[wav_duty >> 1];

	if ((wav_duty & 1) == 0)
		s >>= 4;
	else
		s &= 0x0F;

	s >>= ((NR32 & 0x60) >> 5) - 1; // apply volume

	return s;
}

static int noi_cc;
static uint16_t lfsr;

static const uint8_t divisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

#define NOIFREQ (divisors[(NR43 & 0x07)] << (NR43 >> 4))

/* Step Noise channel. */
static void step_timer_noi ()
{
	if (-- noi_cc <= 0)
	{
		uint8_t v = (lfsr ^ (lfsr >> 1)) & 1;

		lfsr = (lfsr >> 1) | (v << 14);

		if (NR43 & 0x08)
			lfsr = (lfsr & ~0x40) | (v << 6);

		noi_cc = NOIFREQ;
	}
}

static uint16_t noi_len;

/* Step Noise channel's length counter. */
static void step_len_noi ()
{
	if ((~NR44 & 0x40) || !noi_len)
		return;
	if (-- noi_len == 0)
		DISABLE_CH (4);
}

static envelope noi_env;

/* Sample from the noise channel. */
static uint8_t noisample ()
{
	if ((! ENABLE_CH (4)) || !(NR42 & 0xF8))
		return 0;
	return (~lfsr & 1) * noi_env.vol;
}

/* Frame sequencer. */
static uint8_t fs;

/* Step Frame Sequencer. */
static void step_fs ()
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
		envelope_step (&ch1_env);
		envelope_step (&ch2_env);
		envelope_step (&noi_env);
	}
}

/* Frame sequencer timer */
static int fs_cc;

#define FSFREQ 8192 // CPU FREQ / 512 Hz

static void step_timer_fs ()
{
	if (-- fs_cc <= 0)
	{
		step_fs ();
		fs_cc = FSFREQ;
	}
}

static inline void step ()
{
	// step channel timers
	step_timer_ch1 ();
	step_timer_ch2 ();
	step_timer_wav ();
	step_timer_noi ();
	// step frame sequencer timer
	step_timer_fs ();
}

static void sample (uint8_t* l, uint8_t* r)
{
	*l = 0;
	*r = 0;

	if (APU_OFF) return;

	// add all the channels together

	const uint8_t samples[4] =
	{
		ch1sample (),
		ch2sample (),
		wavsample (),
		noisample (),
	};

	uint8_t f = NR51;

	for (int i = 0; i < 4; i ++)
	{
		if (f & 1) *r += samples[i];
		f >>= 1;
	}

	for (int i = 0; i < 4; i ++)
	{
		if (f & 1) *l += samples[i];
		f >>= 1;
	}
}

#define SAMPLE_BUFFER_SIZE 8192

static int sample_rate;
static float samples[SAMPLE_BUFFER_SIZE + 2];
static int samples_len;
static int apucc;

void gb_apu_samples (float* buf, size_t* n)
{
	*n = samples_len * sizeof (float);
	memcpy (buf, samples, *n);
	samples_len = 0;
}

void gb_apu_step (uint32_t cc)
{
	for (; cc > 0; cc --)
	{
		step ();

		if (sample_rate && ++ apucc == sample_rate)
		{
			static uint8_t l, r;
			sample (&l, &r);
			samples[samples_len ++] = l / 60.0 - 1.0;
			samples[samples_len ++] = r / 60.0 - 1.0;
			apucc = 0;
		}

		// in case we are not retrieving the samples in time.
		// reset the samples buffer. tough luck for the user.
		if (samples_len >= SAMPLE_BUFFER_SIZE) samples_len = 0;
	}
}

static int write_ch1 (uint16_t adr, uint8_t v)
{
	switch (adr - 0x0010)
	{
		case 1:
			NR11 = v;
			ch1_len = CH1LEN;
			break;

		case 2:
			NR12 = v;
			if ((NR12 & 0xF8) == 0) // DAC off
				DISABLE_CH (1);
			break;

		case 4:
			NR14 = v;
			if ((NR12 & 0xF8) && (v & 0x80))
			{
				ENABLE_CH (1);

				ch1_duty_cc = 0;
				ch1_cc = CH1FREQ;

				ch1_shadow = ((NR14 & 0x07) << 8) | NR13;
				ch1_sweep_cc = CH1SWEEP;
				if (!ch1_sweep_cc)
					ch1_sweep_cc = 8;
				if (CH1SWEEP_SHIFT) // sweep calculation for overflow
					sweep ();

				if (!ch1_len)
					ch1_len = 64;

				envelope_reset (&ch1_env);
			}
			break;
	}
	return 0;
}

static int write_ch2 (uint16_t adr, uint8_t v)
{
	switch (adr - 0x0015)
	{
		case 1:
			NR21 = v;
			ch2_len = CH2LEN;
			break;

		case 2:
			NR22 = v;
			if ((NR22 & 0xF8) == 0) // DAC off
				DISABLE_CH (2);
			break;

		case 4:
			NR24 = v;
			if ((NR22 & 0xF8) && (v & 0x80))
			{
				ENABLE_CH (2);

				ch2_cc = CH2FREQ;
				ch2_duty_cc = 0;

				if (!ch2_len)
					ch2_len = 64;

				envelope_reset (&ch2_env);
			}
			break;
	}
	return 0;
}

static int write_wav (uint16_t adr, uint8_t v)
{
	adr -= 0x1A;
	if (adr == 4)
	{
		NR34 = v;
		if ((v & 0x80) && (NR32 & 0xF8))
		{
			ENABLE_CH (3);

			wav_cc = WAVFREQ;
			wav_duty = 0;

			if (wav_len == 0)
				wav_len = 256;
		}
	}
	else if (adr == 2)
	{
		NR32 = v;
		if ((NR32 & 0xF8) == 0)
			DISABLE_CH (3);
	}
	else if (adr == 1)
	{
		NR31 = v;
		wav_len = WAVLEN;
	}
	return 0;
}

static int write_noi (uint16_t adr, uint8_t v)
{
	adr -= 0x1F;
	if (adr == 4)
	{
		NR44 = v;
		if ((v & 0x80) && (NR42 & 0xF8))
		{
			ENABLE_CH (4);

			noi_cc = NOIFREQ;
			lfsr = 0x7FFF;

			if (noi_len == 0)
				noi_len = 64;

			envelope_reset (&noi_env);
		}
	}
	else if (adr == 1)
	{
		NR41 = v;
		noi_len = 64 - (NR41 & 0x3F);
	}
	else if (adr == 2)
	{
		NR42 = v;
		if ((NR42 & 0xF8) == 0) // DAC off
			DISABLE_CH (4);
	}
	return 0;
}

static int write_apu_h (uint16_t adr, uint8_t v)
{
	// TODO
	// not sure if one needs to handle the unused registers.
	//
	// make this cleaner when you are less in a hurry
	// maybe a function array

	if (adr < 0xFF10 || adr > 0xFF26) return 0;

	adr &= 0x00FF;

	// ignore writes when powered off.
	if (APU_OFF && adr < 0x26)
		return 1;

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
		return write_wav (adr, v);
	}
	else if (adr < 0x24)
	{
		// write noise
		return write_noi (adr, v);
	}

	if (adr == 0x26) // NR52
	{
		if (~v & 0x80)
		{
			// power off the entire APU
			memset (nr10, 0, 0x15); // write 0 to $FF10 - $FF25
			enabled_ch = 0; // disable everything
		}
		else if (APU_OFF)
		{
			// power on the APU
			fs = 0xff;
			ch1_duty_cc = ch2_duty_cc = wav_duty = 0;
			envelope_reset (&ch1_env);
			envelope_reset (&ch2_env);
			envelope_reset (&noi_env);
		}
	}

	return 0;
}

static int read_apu_h (uint16_t adr, uint8_t* v)
{
	// TODO clean this later

	if (adr >= 0xFF10 && adr <= 0xFF25)
	{
		if (APU_OFF)
		{
			* v = 0;
			return 1;
		}
	}

	if (adr == 0xFF26) // NR52
	{
		*v = ((* v) & 0x80) | 0x70 | (enabled_ch & 0xF);
		return 1;
	}
	else if (adr >= 0xFF27 && adr <= 0xFF2F) // Wave pattern
	{
		* v = 0xFF;
		return 1;
	}

	switch (adr)
	{
		// channel 1
		case 0xFF10:
			*v |= 0x80;
			return 1;
		case 0xFF11:
			*v |= 0x3F;
			return 1;
		case 0xFF12:
			// nada
			return 1;
		case 0xFF13:
			*v = 0xFF;
			return 1;
		case 0xFF14:
			*v |= 0xBF;
			return 1;

		// channel 2
		case 0xFF15:
			*v = 0xFF;
			return 1;
		case 0xFF16:
			*v |= 0x3F;
			return 1;
		case 0xFF17:
			// nada
			return 1;
		case 0xFF18:
			*v = 0xFF;
			return 1;
		case 0xFF19:
			*v |= 0xBF;
			return 1;

		// wave
		case 0xFF1A:
			*v |= 0x7F;
			return 1;
		case 0xFF1B:
			*v = 0xFF;
			return 1;
		case 0xFF1C:
			*v |= 0x9F;
			return 1;
		case 0xFF1D:
			*v = 0xFF;
			return 1;
		case 0xFF1E:
			*v |= 0xBF;
			return 1;

		// noise
		case 0xFF1F:
			*v = 0xFF;
			return 1;
		case 0xFF20:
			*v = 0xFF;
			return 1;
		case 0xFF21:
			// nada
			return 1;
		case 0xFF22:
			// nada
			return 1;
		case 0xFF23:
			*v |= 0xBF;
			return 1;
	}

	return 0;
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
	gb_cpu_register_read_handler (read_apu_h);

	// TODO
	// reset all timers
	fs = 0xff;
	fs_cc = FSFREQ;
	enabled_ch = 0;

	ch1_cc = ch2_cc = wav_cc = noi_cc = 1;

	envelope_init (&ch1_env, nr12);
	envelope_init (&ch2_env, nr22);
	envelope_init (&noi_env, nr42);
}
