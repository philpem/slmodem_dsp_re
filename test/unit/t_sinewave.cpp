/*
 * t_sinewave -- SineWave<float, float> against the blob.
 *
 * BOTH SIDES ARE CALLED THROUGH BITS-TAKING ENTRY POINTS.  The constructor
 * takes four floats BY VALUE, and on i386 cdecl a `float` argument and a
 * 4-byte integer argument occupy the same stack slot -- but which instruction
 * fills that slot is the caller's choice, and GCC has been observed choosing
 * `flds`/`fstps` for one side of a differential pair and a `push` of the memory
 * word for the other (finding 247, found in t_queue).  An x87 store quietens a
 * signalling NaN, so with hostile bit patterns in the inputs -- which this test
 * has, deliberately -- that would hand the two constructors DIFFERENT NUMBERS
 * and report a difference the objects did not make.  Declaring the same entry
 * points a second time with `unsigned` parameters puts a byte-identical stack
 * image in front of both.  `generate` takes pointers, so it needs no such care.
 *
 * Everything is compared as bits: the outputs are NaN and infinity for whole
 * regions of the input space, and a NaN compares unequal to itself.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/SineWave.h"

extern "C" {
/* The declarations at their real types, so the symbols are named honestly... */
void ref_sw_ctor(void *self, float a, float f, float p, float sr)
	asm("ref__ZN8SineWaveIffEC1Effff");
void ref_sw_dtor(void *self)		asm("ref__ZN8SineWaveIffED1Ev");
void ref_sw_generate(void *self, float *out, unsigned long n)
	asm("ref__ZN8SineWaveIffE8generateEPfm");

/*
 * ...and the pair that matters, at the type the test actually calls.  `this`
 * is a plain first stack argument -- the object is cdecl, not thiscall -- so
 * these declarations and the members are the same entry points.
 */
void ref_sw_ctor_bits(void *self, unsigned a, unsigned f, unsigned p,
		      unsigned sr)	asm("ref__ZN8SineWaveIffEC1Effff");
void our_sw_ctor_bits(void *self, unsigned a, unsigned f, unsigned p,
		      unsigned sr)	asm("_ZN8SineWaveIffEC1Effff");
void our_sw_generate(void *self, float *out, unsigned long n)
	asm("_ZN8SineWaveIffE8generateEPfm");
void our_sw_dtor(void *self)		asm("_ZN8SineWaveIffED1Ev");
}

static long bits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return (long)u.u;
}

static unsigned fbits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return u.u;
}

/*
 * All sixteen object bytes.  The class is four floats and no vptr, so this is
 * the whole of it; comparing as words rather than through the members means a
 * NaN in `phase` compares equal to itself.
 */
static void cmp_obj(const void *a, const void *b, long tag)
{
	const unsigned *x = (const unsigned *)a;
	const unsigned *y = (const unsigned *)b;
	int i;

	for (i = 0; i < 4; i++)
		diff_eq_int("object word", (long)x[i], (long)y[i],
			    tag * 10 + i);
}

/*
 * One case: construct both, run the same sequence of `generate` calls on each,
 * compare the object and the whole output buffer after every one.
 */
static void run(unsigned a, unsigned f, unsigned p, unsigned sr,
		const unsigned long *lens, int nlens, long tag)
{
	static unsigned char oa[32], ob[32];
	static float outa[544], outb[544];
	int j;
	unsigned i;

	/* Poisoned, so a constructor writing past +16 is caught. */
	memset(oa, 0x5a, sizeof(oa));
	memset(ob, 0x5a, sizeof(ob));
	our_sw_ctor_bits(oa, a, f, p, sr);
	ref_sw_ctor_bits(ob, a, f, p, sr);
	for (i = 16; i < 32; i++)
		diff_eq_int("wrote past the object", (long)oa[i], (long)ob[i],
			    tag * 100 + (long)i);
	cmp_obj(oa, ob, tag);

	for (j = 0; j < nlens; j++) {
		memset(outa, 0x5a, sizeof(outa));
		memset(outb, 0x5a, sizeof(outb));
		our_sw_generate(oa, outa, lens[j]);
		ref_sw_generate(ob, outb, lens[j]);
		cmp_obj(oa, ob, tag * 100 + j);
		/*
		 * The WHOLE buffer, not the first `lens[j]`: a version writing
		 * one sample too many would pass a comparison that stopped at
		 * the requested count.
		 */
		for (i = 0; i < 544; i++)
			diff_eq_int("sample", bits(outa[i]), bits(outb[i]),
				    (tag * 100 + j) * 1000 + i);
	}

	our_sw_dtor(oa);
	ref_sw_dtor(ob);
	cmp_obj(oa, ob, tag + 900000);
}

int
main(void)
{
	static const unsigned long lens1[] = { 8 };
	static const unsigned long lens2[] = { 0, 1, 1, 2, 3, 5, 7, 0, 48, 480 };
	int rc = 0;
	unsigned i, j, k, m;
	long tag = 0;

	/*
	 * The cross product.  The amplitudes and rates include zero and a
	 * negative; the phases include the two thresholds where the wrap and
	 * the sine stop working, which are the interesting part.
	 */
	diff_begin("sinewave: amplitude x frequency x phase x rate");
	{
		static const float amps[] = {
			1.0f, 4800.0f, -1.0f, 0.0f, 1e-30f, 3.4e38f, -0.0f,
			1e20f
		};
		static const float freqs[] = {
			980.0f, 1200.0f, 0.0f, -1200.0f, 1.0f, 4000.0f,
			8000.0f, 1e6f, 1e-6f, 4800.0f, 2400.0f, 3.4e38f
		};
		static const float phases[] = {
			0.0f, -0.0f, 1.0f, -1.0f, 3.14159265f, 6.2831853f,
			100.0f, -100.0f, 1e6f,
			/*
			 * 1.349e10 is 2^31*2pi: at and above it the wrap's
			 * `fistpl` returns the integer indefinite and the
			 * "reduction" adds 1.349e10 instead.  2e19 is past
			 * 2^63, where `fsin` gives up and returns its argument.
			 * Both are silent in the object and both are here.
			 */
			1.349e10f, 1.4e10f, 1e12f, 2e19f, 1e-30f
		};
		static const float rates[] = {
			9600.0f, 8000.0f, 7200.0f, 1.0f, 0.0f, -9600.0f,
			1e-30f, 3.4e38f
		};

		for (i = 0; i < sizeof(amps) / sizeof(amps[0]); i++)
			for (j = 0; j < sizeof(freqs) / sizeof(freqs[0]); j++)
				for (k = 0; k < sizeof(phases) / sizeof(phases[0]); k++)
					for (m = 0; m < sizeof(rates) / sizeof(rates[0]); m++)
						run(fbits(amps[i]), fbits(freqs[j]),
						    fbits(phases[k]), fbits(rates[m]),
						    lens1, 1, tag++);
	}
	rc |= diff_end();

	/*
	 * Length sequences on the real call parameters, so the phase is carried
	 * across calls the way `qcLineVerification` carries it.  The zeros in
	 * the middle are not filler: a zero-length call still rewrites `phase`,
	 * and a reconstruction that returned early would diverge on the call
	 * after it rather than on the zero-length one itself.
	 */
	diff_begin("sinewave: phase carried across calls, including zero-length");
	{
		static const float amps[] = { 4800.0f, 1.0f };
		static const float freqs[] = { 980.0f, 1200.0f, 3300.0f };
		static const float phases[] = { 0.0f, 2.5f, -2.5f, 1e6f };
		static const float rates[] = { 9600.0f, 8000.0f };

		for (i = 0; i < sizeof(amps) / sizeof(amps[0]); i++)
			for (j = 0; j < sizeof(freqs) / sizeof(freqs[0]); j++)
				for (k = 0; k < sizeof(phases) / sizeof(phases[0]); k++)
					for (m = 0; m < sizeof(rates) / sizeof(rates[0]); m++)
						run(fbits(amps[i]), fbits(freqs[j]),
						    fbits(phases[k]), fbits(rates[m]),
						    lens2,
						    sizeof(lens2) / sizeof(lens2[0]),
						    tag++);
	}
	rc |= diff_end();

	/*
	 * FULLY RANDOM BIT PATTERNS in all four members, which is where the
	 * NaNs, the infinities, the denormals and the signalling NaNs come
	 * from.  A hand-picked list only reaches the cases someone thought of;
	 * this reaches the ones nobody did.  The generator is a fixed LCG, so a
	 * failure is reproducible.
	 */
	diff_begin("sinewave: random bit patterns");
	{
		static const unsigned long lens3[] = { 0, 1, 4, 17 };
		unsigned long s = 20240815ul;
		int t;

		for (t = 0; t < 1500; t++) {
			unsigned v[4];
			int q;

			for (q = 0; q < 4; q++) {
				s = s * 1103515245ul + 12345ul;
				v[q] = (unsigned)((s >> 8) ^ (s << 13));
			}
			run(v[0], v[1], v[2], v[3], lens3,
			    sizeof(lens3) / sizeof(lens3[0]), tag++);
		}
	}
	rc |= diff_end();

	return rc;
}
