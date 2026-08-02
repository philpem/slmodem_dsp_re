/*
 * t_v34ec.c -- differential test of the V.34 echo canceller, Hilbert
 * transformer and timing high-pass.
 *
 * The echo canceller keeps almost all of its state outside its own object:
 * five pointers into arrays the caller owns.  So comparing the 0x20-byte
 * struct proves very little, and every check here compares the ARRAYS too --
 * delay line, both halves of the coefficients, and the tap history.  The one
 * field that moves inside the struct is `cursor`, and it is compared as an
 * OFFSET from each side's own base, because the two sides legitimately hold
 * different addresses.
 *
 * The coefficient pair is the part worth driving hard.  `coeff` and
 * `coeff_frac` are the two halves of one 32-bit number, and an adaptation
 * that got the signedness of the low half wrong would still look right for
 * small errors and diverge only once a tap crossed zero.  The runs below use
 * signed errors of both signs and enough iterations for taps to cross.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34filt.h"

extern void ref_V34EchoCleanUp(void *e);
extern void ref_V34EchoUpdateDelayLine(void *e, short sample);
extern int ref_V34EchoFilter(void *e, short lag);
extern void ref_V34EchoAdapt(void *e, short err);
extern int ref_V34EchoEstimateDelayLineEnergy(void *e);
extern void ref_V34EchoReportCoeff(void *e);
extern void ref_V34InitHilbertFilter(short *state);
extern void ref_V34HilbertFilter(short *state, short sample, int *re, int *im);
extern int ref_V34TimingHPFilter(void *t, short sample);
extern void ref_V34EchoPreFilterCopy(void *dst, const short *coeff);
extern void ref_V34PremptxCopy(void *m, const short *coeff);
extern const short ref_V34hilbertrealcoef[V34_HILBERT_TAPS];
extern const short ref_V34hilbertimagcoef[V34_HILBERT_TAPS];
extern const short ref_V34TimingHPFilterCoeff[V34_TIMING_HP_TAPS];
extern const short ref_V34TimingPrefilterCoeff[40];
extern const short ref_negHalfBaud_Acoef_Imag[3], ref_negHalfBaud_Acoef_Real[3];
extern const short ref_negHalfBaud_Bcoef_Imag[3], ref_negHalfBaud_Bcoef_Real[3];
extern const short ref_posHalfBaud_Acoef_Imag[3], ref_posHalfBaud_Acoef_Real[3];
extern const short ref_posHalfBaud_Bcoef_Imag[3], ref_posHalfBaud_Bcoef_Real[3];
extern const short ref_V34TimingIIR_Acoef[5], ref_V34TimingIIR_Bcoef[5];
extern void ref_V34TimingFiltersInit(void *t);
extern int ref_V34TimingPrefilter(void *t);
extern void ref_V34SetupModulator(void *m, short baud, short carrier,
				  short phase, int a4, int reset);
extern int ref_V34ModulatorProcess(void *m, int symbol, short *out);
extern int ref_V34TimingFilter(void *t, int sample);
extern int ref_V34Filter2(short s, short *st, const short *c, unsigned taps);
extern void ref_V34EchoPreFilter(short *buf, short n, void *p);

#define DLEN	64
#define TAPS	32

/* Coverage. */
static int saw_wrap;		/* the delay line cursor wrapped          */
static int saw_tap_sign;	/* a coefficient crossed zero             */
static int saw_carry;		/* a fractional part carried into `coeff` */

/* One side's storage: the object and every array it points at. */
struct side {
	struct v34_echo e;
	short dline[DLEN];
	short coeff[TAPS];
	short frac[TAPS];
	short hist[TAPS];
};

static struct side a, b;

static void
setup(unsigned taps, unsigned dlen)
{
	memset(&a, HARNESS_MALLOC_FILL, sizeof(a));
	memset(&b, HARNESS_MALLOC_FILL, sizeof(b));

	a.e.dline = a.dline; a.e.cursor = a.dline;
	a.e.coeff = a.coeff; a.e.coeff_frac = a.frac; a.e.hist = a.hist;
	a.e.unused_14 = NULL; a.e.dlen = dlen; a.e.taps = taps;

	b.e.dline = b.dline; b.e.cursor = b.dline;
	b.e.coeff = b.coeff; b.e.coeff_frac = b.frac; b.e.hist = b.hist;
	b.e.unused_14 = NULL; b.e.dlen = dlen; b.e.taps = taps;
}

/*
 * The object, the cursor as an offset, and all four arrays.
 *
 * The struct is compared field by field rather than byte by byte because four
 * of its eight words are pointers the two sides deliberately differ on; the
 * cursor is turned into an index so it can be compared at all.
 */
static void
compare(const char *what, int tag)
{
	int i;

	diff_eq_int("cursor offset", a.e.cursor - a.dline,
		    b.e.cursor - b.dline, tag);
	diff_eq_int("dlen", (long)a.e.dlen, (long)b.e.dlen, tag);
	diff_eq_int("taps", (long)a.e.taps, (long)b.e.taps, tag);

	for (i = 0; i < DLEN; i++)
		diff_eq_int("delay line", a.dline[i], b.dline[i], i);
	for (i = 0; i < TAPS; i++) {
		diff_eq_int("coeff", a.coeff[i], b.coeff[i], i);
		diff_eq_int("coeff_frac", a.frac[i], b.frac[i], i);
		diff_eq_int("hist", a.hist[i], b.hist[i], i);
	}
	(void)what;
}

int
main(void)
{
	int rc = 0;
	int i, k;
	short state_a[V34_HILBERT_TAPS], state_b[V34_HILBERT_TAPS];
	struct v34_timing t_a, t_b;

	diff_begin("v34 filters: the coefficient tables");
	for (i = 0; i < V34_HILBERT_TAPS; i++) {
		diff_eq_int("V34hilbertrealcoef[%ld]", V34hilbertrealcoef[i],
			    ref_V34hilbertrealcoef[i], i);
		diff_eq_int("V34hilbertimagcoef[%ld]", V34hilbertimagcoef[i],
			    ref_V34hilbertimagcoef[i], i);
	}
	for (i = 0; i < V34_TIMING_HP_TAPS; i++)
		diff_eq_int("V34TimingHPFilterCoeff[%ld]",
			    V34TimingHPFilterCoeff[i],
			    ref_V34TimingHPFilterCoeff[i], i);
	for (i = 0; i < 40; i++)
		diff_eq_int("V34TimingPrefilterCoeff[%ld]",
			    V34TimingPrefilterCoeff[i],
			    ref_V34TimingPrefilterCoeff[i], i);
	for (i = 0; i < 3; i++) {
		diff_eq_int("negHalfBaud_Acoef_Imag[%ld]",
			    negHalfBaud_Acoef_Imag[i],
			    ref_negHalfBaud_Acoef_Imag[i], i);
		diff_eq_int("negHalfBaud_Acoef_Real[%ld]",
			    negHalfBaud_Acoef_Real[i],
			    ref_negHalfBaud_Acoef_Real[i], i);
		diff_eq_int("negHalfBaud_Bcoef_Imag[%ld]",
			    negHalfBaud_Bcoef_Imag[i],
			    ref_negHalfBaud_Bcoef_Imag[i], i);
		diff_eq_int("negHalfBaud_Bcoef_Real[%ld]",
			    negHalfBaud_Bcoef_Real[i],
			    ref_negHalfBaud_Bcoef_Real[i], i);
		diff_eq_int("posHalfBaud_Acoef_Imag[%ld]",
			    posHalfBaud_Acoef_Imag[i],
			    ref_posHalfBaud_Acoef_Imag[i], i);
		diff_eq_int("posHalfBaud_Acoef_Real[%ld]",
			    posHalfBaud_Acoef_Real[i],
			    ref_posHalfBaud_Acoef_Real[i], i);
		diff_eq_int("posHalfBaud_Bcoef_Imag[%ld]",
			    posHalfBaud_Bcoef_Imag[i],
			    ref_posHalfBaud_Bcoef_Imag[i], i);
		diff_eq_int("posHalfBaud_Bcoef_Real[%ld]",
			    posHalfBaud_Bcoef_Real[i],
			    ref_posHalfBaud_Bcoef_Real[i], i);
	}
	for (i = 0; i < 5; i++) {
		diff_eq_int("V34TimingIIR_Acoef[%ld]", V34TimingIIR_Acoef[i],
			    ref_V34TimingIIR_Acoef[i], i);
		diff_eq_int("V34TimingIIR_Bcoef[%ld]", V34TimingIIR_Bcoef[i],
			    ref_V34TimingIIR_Bcoef[i], i);
	}

	/*
	 * Structure, not bytes.  The two half-baud filters must be mirror
	 * images -- identical but for the sign of the imaginary denominator
	 * half -- and their numerators must be real.  A regenerated table
	 * that broke either property would still match the reference above
	 * only if it were wrong on both sides, which it cannot be; this says
	 * the property out loud so a future derivation has it to fail
	 * against.  See docs/coefficients.md.
	 */
	for (i = 0; i < 3; i++) {
		diff_eq_int("pos and neg share a real denominator at %ld",
			    posHalfBaud_Acoef_Real[i],
			    negHalfBaud_Acoef_Real[i], i);
		diff_eq_int("and mirror the imaginary one at %ld",
			    posHalfBaud_Acoef_Imag[i],
			    -negHalfBaud_Acoef_Imag[i], i);
		diff_eq_int("the numerators are identical at %ld",
			    posHalfBaud_Bcoef_Real[i],
			    negHalfBaud_Bcoef_Real[i], i);
		diff_eq_int("and purely real at %ld",
			    posHalfBaud_Bcoef_Imag[i], 0, i);
	}
	/* B is 1768 * (1 - z^-2)^2, so it is symmetric with a zero middle. */
	diff_eq_int("IIR numerator is symmetric", V34TimingIIR_Bcoef[0],
		    V34TimingIIR_Bcoef[4], 0);
	diff_eq_int("IIR numerator middle is -2x the ends",
		    V34TimingIIR_Bcoef[2], -2 * V34TimingIIR_Bcoef[0], 0);
	rc |= diff_end();

	diff_begin("v34 timing: FiltersInit");
	{
		struct v34_timing ta, tb;

		memset(&ta, HARNESS_MALLOC_FILL, sizeof(ta));
		memset(&tb, HARNESS_MALLOC_FILL, sizeof(tb));
		V34TimingFiltersInit(&ta);
		ref_V34TimingFiltersInit(&tb);
		/*
		 * Byte for byte, pointers included: both sides install the
		 * SAME two addresses, because the coefficient tables are
		 * separate objects but the harness links only one of each
		 * into the comparison... which is not true, so the two
		 * pointer fields are skipped and checked by value instead.
		 */
		for (i = 0; i < (int)__builtin_offsetof(struct v34_timing,
						       prefilter_coeff); i++)
			diff_eq_int("FiltersInit state",
				    ((unsigned char *)&ta)[i],
				    ((unsigned char *)&tb)[i], i);
		diff_eq_int("prefilter pointer installed",
			    ta.prefilter_coeff == V34TimingPrefilterCoeff, 1, 0);
		diff_eq_int("hp pointer installed",
			    ta.hp_coeff == V34TimingHPFilterCoeff, 1, 0);
		diff_eq_int("and the reference installed its own",
			    tb.prefilter_coeff == ref_V34TimingPrefilterCoeff,
			    1, 0);
		diff_eq_int("likewise its hp",
			    tb.hp_coeff == ref_V34TimingHPFilterCoeff, 1, 0);

		/*
		 * D29 stated as its own assertion.  FiltersInit zeroes eighty
		 * SHORTS past `iir`, which is the whole high-pass history and
		 * only half the prefilter state -- so the upper twenty
		 * entries still hold the fill.  Both sides agreeing would
		 * pass the byte comparison above whichever way it went, so
		 * this has to be said out loud.
		 */
		for (i = 0; i < V34_TIMING_PRE_TAPS; i++) {
			int want = (i < 20) ? 0 : (int)0xa5a5a5a5;

			diff_eq_int("prefilter state after init",
				    ta.pre_state[i], want, i);
		}
	}
	rc |= diff_end();

	diff_begin("v34 timing prefilter");
	{
		struct v34_timing ta, tb;

		memset(&ta, HARNESS_MALLOC_FILL, sizeof(ta));
		memset(&tb, HARNESS_MALLOC_FILL, sizeof(tb));
		V34TimingFiltersInit(&ta);
		ref_V34TimingFiltersInit(&tb);
		/*
		 * Zero the whole state first, so the run below starts from a
		 * defined point rather than from D29's half-initialised one --
		 * that is asserted above and does not need re-proving here.
		 */
		memset(ta.pre_state, 0, sizeof(ta.pre_state));
		memset(tb.pre_state, 0, sizeof(tb.pre_state));

		for (i = 0; i < 500; i++) {
			int ra, rb;

			ta.in0 = tb.in0 = (int)(((unsigned)(i * 277 - 6000)
						 << 16)
						| (unsigned short)(i * 613
								   - 9000));
			ta.in1 = tb.in1 = (int)(((unsigned)(4000 - i * 131)
						 << 16)
						| (unsigned short)(i * 907
								   - 15000));
			ra = V34TimingPrefilter(&ta);
			rb = ref_V34TimingPrefilter(&tb);
			diff_eq_int("prefilter out", ra, rb, i);
			for (k = 0; k < V34_TIMING_PRE_TAPS; k++)
				diff_eq_int("prefilter state", ta.pre_state[k],
					    tb.pre_state[k], k);
		}
	}
	rc |= diff_end();

	diff_begin("v34 timing filter");
	{
		struct v34_timing ta, tb;

		memset(&ta, 0, sizeof(ta)); memset(&tb, 0, sizeof(tb));
		V34TimingFiltersInit(&ta); ref_V34TimingFiltersInit(&tb);
		memset(ta.pre_state, 0, sizeof(ta.pre_state));
		memset(tb.pre_state, 0, sizeof(tb.pre_state));
		for (i = 0; i < 400; i++) {
			int smp = (int)(((unsigned)(i * 311 - 5000) << 16)
					| (unsigned short)(i * 701 - 11000));

			diff_eq_int("timing out", V34TimingFilter(&ta, smp),
				    ref_V34TimingFilter(&tb, smp), i);
			/*
			 * Everything except the two coefficient pointers,
			 * which the two sides legitimately hold different
			 * addresses in -- each side installed its own table.
			 * Skipping them is the same reason compare_obj skips
			 * `tone` in t_v23tx.
			 */
			for (k = 0; k < (int)sizeof(ta); k++) {
				if (k >= (int)__builtin_offsetof(
						struct v34_timing,
						prefilter_coeff)
				    && k < (int)__builtin_offsetof(
						struct v34_timing, in0))
					continue;
				diff_eq_int("timing state at %ld",
					    ((unsigned char *)&ta)[k],
					    ((unsigned char *)&tb)[k], k);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 setup modulator");
	{
		static struct v34_modulator ma, mb;
		static short sha[4096], shb[4096];
		static const short bauds[] = { 600, 2400, 2800, 3000, 3200,
					       3429, 4800, 1234 };
		static const short carr[] = { 1200, 1600, 1680, 1800, 1829,
					      1867, 1920, 1959, 2000, 2400,
					      999 };
		unsigned bi, ci;
		int ph, rst;

		for (bi = 0; bi < sizeof(bauds) / sizeof(bauds[0]); bi++)
		for (ci = 0; ci < sizeof(carr) / sizeof(carr[0]); ci++)
		for (ph = 0; ph <= 3; ph++)
		for (rst = 0; rst <= 1; rst++) {
			memset(&ma, HARNESS_MALLOC_FILL, sizeof(ma));
			memset(&mb, HARNESS_MALLOC_FILL, sizeof(mb));
			memset(sha, 0x5a, sizeof(sha));
			memset(shb, 0x5a, sizeof(shb));
			ma.shaped = sha; mb.shaped = shb;
			/* f0c/f18 feed the modulo on the non-reset path. */
			ma.row = mb.row = 12345;
			ma.phase = mb.phase = 6789;

			V34SetupModulator(&ma, bauds[bi], carr[ci],
					  (short)ph, 0, rst);
			ref_V34SetupModulator(&mb, bauds[bi], carr[ci],
					      (short)ph, 0, rst);

			/* Scalars. */
			diff_eq_int("taps", ma.taps, mb.taps, bauds[bi]);
			diff_eq_int("f04", ma.f04, mb.f04, bauds[bi]);
			diff_eq_int("rows", ma.rows, mb.rows, bauds[bi]);
			diff_eq_int("row", ma.row, mb.row, bauds[bi]);
			diff_eq_int("sine_len", ma.sine_len, mb.sine_len,
				    carr[ci]);
			diff_eq_int("phase", ma.phase, mb.phase, carr[ci]);
			diff_eq_int("wpos", ma.wpos, mb.wpos, rst);
			diff_eq_int("wstep", ma.wstep, mb.wstep, rst);
			/*
			 * Pointers cannot be compared across the two sides,
			 * so each is checked by the CONTENT it selects --
			 * which is what actually matters and is stronger than
			 * an address comparison would be.
			 */
			for (k = 0; k < ma.sine_len * 2; k++)
				diff_eq_int("sine table", ma.sine[k],
					    mb.sine[k], k);
			for (k = 0; k < 16; k++)
				diff_eq_int("preemp", ma.preemp[k],
					    mb.preemp[k], k);
			/*
			 * 4800 baud installs no pre-emphasis table at all --
			 * it takes txAllPass and jumps straight to the
			 * engine -- so the field keeps whatever it held and
			 * must not be dereferenced.  Both sides leave it, so
			 * a comparison would be of two fill patterns anyway.
			 */
			if (bauds[bi] != 4800)
				for (k = 0; k < 42; k++)
					diff_eq_int("ec_prem b=%ld",
						    ma.ec_prem[k],
						    mb.ec_prem[k],
						    bauds[bi] * 10000L
						    + carr[ci]);
			/* And the loaded polyphase bank, byte for byte. */
			for (k = 0; k < ma.rows * V34_MOD_ROW; k++)
				diff_eq_int("shaped", sha[k], shb[k], k);
		}
	}
	rc |= diff_end();

	diff_begin("v34 modulator process");
	{
		static struct v34_modulator ma, mb;
		static short sha[4096], shb[4096];
		static short oa[256], ob[256];
		static const short bauds[] = { 600, 2400, 2800, 3000, 3200,
					       3429 };
		static const short carr[] = { 1200, 1680, 1829, 2400 };
		unsigned bi, ci;

		for (bi = 0; bi < sizeof(bauds) / sizeof(bauds[0]); bi++)
		for (ci = 0; ci < sizeof(carr) / sizeof(carr[0]); ci++) {
			memset(&ma, 0, sizeof(ma)); memset(&mb, 0, sizeof(mb));
			memset(sha, 0, sizeof(sha)); memset(shb, 0, sizeof(shb));
			ma.shaped = sha; mb.shaped = shb;
			V34SetupModulator(&ma, bauds[bi], carr[ci], 0, 0, 1);
			ref_V34SetupModulator(&mb, bauds[bi], carr[ci], 0, 0, 1);

			for (i = 0; i < 120; i++) {
				int sym = (int)(((unsigned)(i * 811 - 9000)
						 << 16)
						| (unsigned short)(i * 337
								   - 5000));
				int na, nb;

				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));
				na = V34ModulatorProcess(&ma, sym, oa);
				nb = ref_V34ModulatorProcess(&mb, sym, ob);
				diff_eq_int("nout", na, nb,
					    bauds[bi] * 10000L + i);
				for (k = 0; k < 256; k++)
					diff_eq_int("mod out", oa[k], ob[k],
						    bauds[bi] * 10000L + k);
				/*
				 * The whole modulator object, so the work
				 * buffer's shift is compared as well as the
				 * outputs -- including the stale seed D32
				 * puts at index 0.  Pointers skipped: each
				 * side holds its own tables.
				 */
				for (k = 0; k < (int)sizeof(ma); k++) {
					if (k >= (int)__builtin_offsetof(
						struct v34_modulator, sine)
					    && k < (int)__builtin_offsetof(
						struct v34_modulator, sine) + 4)
						continue;
					if (k >= (int)__builtin_offsetof(
						struct v34_modulator, shaped)
					    && k < (int)__builtin_offsetof(
						struct v34_modulator, preemp)
						   + 4)
						continue;
					diff_eq_int("mod state at %ld",
						    ((unsigned char *)&ma)[k],
						    ((unsigned char *)&mb)[k],
						    k);
				}
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 Filter2, the generic FIR nothing calls");
	{
		static short st_a[64], st_b[64];
		static short co[64];
		unsigned taps;

		for (i = 0; i < 64; i++)
			co[i] = (short)(i * 401 - 12000);
		for (taps = 0; taps <= 64; taps += 16) {
			memset(st_a, HARNESS_MALLOC_FILL, sizeof(st_a));
			memset(st_b, HARNESS_MALLOC_FILL, sizeof(st_b));
			for (i = 0; i < 300; i++) {
				short x = (short)((i * 5443) % 60001 - 30000);

				diff_eq_int("Filter2",
					    V34Filter2(x, st_a, co, taps),
					    ref_V34Filter2(x, st_b, co, taps),
					    (long)taps * 1000 + i);
				for (k = 0; k < 64; k++)
					diff_eq_int("Filter2 state", st_a[k],
						    st_b[k], k);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 echo pre-filter");
	{
		struct v34_echo_prefilter pa, pb;
		static short co[V34_ECHO_PREFILTER_TAPS];
		static short buf_a[64], buf_b[64];
		int sh;

		for (i = 0; i < V34_ECHO_PREFILTER_TAPS; i++)
			co[i] = (short)(i * 211 - 3000);

		/* Every shift the field can plausibly hold, plus a byte-sized one. */
		for (sh = 0; sh <= 16; sh += 4) {
			memset(&pa, HARNESS_MALLOC_FILL, sizeof(pa));
			memset(&pb, HARNESS_MALLOC_FILL, sizeof(pb));
			memset(pa.state, 0, sizeof(pa.state));
			memset(pb.state, 0, sizeof(pb.state));
			pa.coeff = co; pb.coeff = co;
			pa.shift = sh; pb.shift = sh;

			for (i = 0; i < 60; i++) {
				int n;

				for (n = 0; n < 64; n++)
					buf_a[n] = buf_b[n] =
						(short)((i * 64 + n) * 313
							- 20000);
				V34EchoPreFilter(buf_a, 64, &pa);
				ref_V34EchoPreFilter(buf_b, 64, &pb);
				for (n = 0; n < 64; n++)
					diff_eq_int("prefiltered", buf_a[n],
						    buf_b[n], n);
				for (n = 0; n < V34_ECHO_PREFILTER_TAPS; n++)
					diff_eq_int("prefilter state",
						    pa.state[n], pb.state[n],
						    n);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 echo: CleanUp leaves the delay line alone");
	setup(TAPS, DLEN);
	/* Put something recognisable in the line so "not cleared" is visible. */
	for (i = 0; i < DLEN; i++)
		a.dline[i] = b.dline[i] = (short)(1000 + i);
	V34EchoCleanUp(&a.e);
	ref_V34EchoCleanUp(&b.e);
	compare("after CleanUp", 0);
	/*
	 * Stated as its own assertion, not left implicit in the comparison
	 * above: both sides agreeing that the line was cleared would pass the
	 * comparison just as happily.  D26 says it is NOT cleared.
	 */
	diff_eq_int("the delay line survived CleanUp", a.dline[7], 1007, 0);
	diff_eq_int("but the coefficients did not", a.coeff[7], 0, 0);
	diff_eq_int("nor the fractional halves", a.frac[7], 0, 0);
	diff_eq_int("nor the tap history", a.hist[7], 0, 0);
	rc |= diff_end();

	diff_begin("v34 echo: the delay line wraps");
	setup(TAPS, DLEN);
	V34EchoCleanUp(&a.e);
	ref_V34EchoCleanUp(&b.e);
	for (i = 0; i < DLEN * 3 + 5; i++) {
		int before = (int)(a.e.cursor - a.dline);

		V34EchoUpdateDelayLine(&a.e, (short)(i * 137 - 4000));
		ref_V34EchoUpdateDelayLine(&b.e, (short)(i * 137 - 4000));
		compare("after a push", i);
		if ((int)(a.e.cursor - a.dline) < before)
			saw_wrap++;
	}
	rc |= diff_end();

	diff_begin("v34 echo: filter and adapt");
	setup(TAPS, DLEN);
	V34EchoCleanUp(&a.e);
	ref_V34EchoCleanUp(&b.e);

	/*
	 * A closed loop: push a sample, filter it at a lag, adapt on the
	 * result.  Running it as a loop rather than as isolated calls is what
	 * lets the coefficients actually converge, and convergence is what
	 * drives taps across zero and carries out of the fractional half.
	 */
	for (i = 0; i < 2000; i++) {
		short sample = (short)((i * 2311) % 20001 - 10000);
		short lag = (short)(i % 7);
		int fa, fb;
		short err;

		V34EchoUpdateDelayLine(&a.e, sample);
		ref_V34EchoUpdateDelayLine(&b.e, sample);

		fa = V34EchoFilter(&a.e, lag);
		fb = ref_V34EchoFilter(&b.e, lag);
		diff_eq_int("filter output", fa, fb, i);

		err = (short)((i & 1) ? (i % 300) - 150 : 150 - (i % 300));
		{
			short was[TAPS];

			memcpy(was, a.coeff, sizeof(was));
			V34EchoAdapt(&a.e, err);
			ref_V34EchoAdapt(&b.e, err);
			/*
			 * Any tap, not just tap 0: whether a particular tap
			 * changes sign depends entirely on the input, and
			 * watching one of thirty-two is how this counter read
			 * zero on its first run while the adaptation was
			 * working perfectly well.
			 */
			for (k = 0; k < TAPS; k++) {
				if (a.coeff[k] != was[k])
					saw_carry++;
				if ((a.coeff[k] < 0) != (was[k] < 0))
					saw_tap_sign++;
			}
		}
		compare("after filter and adapt", i);

		diff_eq_int("delay line energy",
			    V34EchoEstimateDelayLineEnergy(&a.e),
			    ref_V34EchoEstimateDelayLineEnergy(&b.e), i);
	}
	rc |= diff_end();

	diff_begin("v34 echo: one tap, and a lag that wraps");
	/*
	 * taps == 1 is the original's own special case -- it skips the history
	 * shift entirely -- and a lag near the delay line's length is what
	 * exercises the single wrapping subtraction.
	 */
	setup(1, DLEN);
	V34EchoCleanUp(&a.e);
	ref_V34EchoCleanUp(&b.e);
	for (i = 0; i < 200; i++) {
		short lag = (short)(i % (DLEN - 2));

		V34EchoUpdateDelayLine(&a.e, (short)(i * 521 - 3000));
		ref_V34EchoUpdateDelayLine(&b.e, (short)(i * 521 - 3000));
		diff_eq_int("one-tap filter", V34EchoFilter(&a.e, lag),
			    ref_V34EchoFilter(&b.e, lag), i);
		V34EchoAdapt(&a.e, (short)(i - 100));
		ref_V34EchoAdapt(&b.e, (short)(i - 100));
		compare("one tap", i);
	}
	rc |= diff_end();

	diff_begin("v34 echo: ReportCoeff with logging off");
	/*
	 * All this can establish is that the function changes nothing and
	 * returns.  Its output goes to a logger both sides stub out, and the
	 * path that would read past the coefficient array (D28) is gated on a
	 * level slmodemd ships at zero.  Driving that path would mean raising
	 * the level on both sides and would still compare nothing, because
	 * neither logger records anything.
	 */
	setup(TAPS, DLEN);
	V34EchoCleanUp(&a.e);
	ref_V34EchoCleanUp(&b.e);
	V34EchoReportCoeff(&a.e);
	ref_V34EchoReportCoeff(&b.e);
	compare("after ReportCoeff on zero coefficients", 0);
	for (i = 0; i < TAPS; i++)
		a.coeff[i] = b.coeff[i] = (short)(i * 91 - 1000);
	V34EchoReportCoeff(&a.e);
	ref_V34EchoReportCoeff(&b.e);
	compare("after ReportCoeff on live coefficients", 1);
	rc |= diff_end();

	diff_begin("v34 Hilbert transformer");
	memset(state_a, HARNESS_MALLOC_FILL, sizeof(state_a));
	memset(state_b, HARNESS_MALLOC_FILL, sizeof(state_b));
	V34InitHilbertFilter(state_a);
	ref_V34InitHilbertFilter(state_b);
	for (i = 0; i < V34_HILBERT_TAPS; i++)
		diff_eq_int("init zeroed[%ld]", state_a[i], state_b[i], i);

	for (i = 0; i < 3000; i++) {
		short x = (short)((i * 7919) % 65536 - 32768);
		int ra = -1, ia = -1, rb = -2, ib = -2;

		V34HilbertFilter(state_a, x, &ra, &ia);
		ref_V34HilbertFilter(state_b, x, &rb, &ib);
		diff_eq_int("hilbert re", ra, rb, i);
		diff_eq_int("hilbert im", ia, ib, i);
		for (k = 0; k < V34_HILBERT_TAPS; k++)
			diff_eq_int("hilbert state", state_a[k], state_b[k], k);
	}
	rc |= diff_end();

	diff_begin("v34 timing high-pass");
	memset(&t_a, HARNESS_MALLOC_FILL, sizeof(t_a));
	memset(&t_b, HARNESS_MALLOC_FILL, sizeof(t_b));
	memset(t_a.hist, 0, sizeof(t_a.hist));
	memset(t_b.hist, 0, sizeof(t_b.hist));
	for (i = 0; i < 3000; i++) {
		short x = (short)((i * 4409) % 65536 - 32768);

		diff_eq_int("timing hp", V34TimingHPFilter(&t_a, x),
			    ref_V34TimingHPFilter(&t_b, x), i);
		for (k = 0; k < V34_TIMING_HP_TAPS; k++)
			diff_eq_int("timing hp state", t_a.hist[k],
				    t_b.hist[k], k);
	}
	rc |= diff_end();

	diff_begin("v34 filters: the two pointer installers");
	{
		static unsigned char slot_a[0x1000], slot_b[0x1000];
		static const short dummy[4] = { 1, 2, 3, 4 };

		memset(slot_a, HARNESS_MALLOC_FILL, sizeof(slot_a));
		memset(slot_b, HARNESS_MALLOC_FILL, sizeof(slot_b));
		V34EchoPreFilterCopy(slot_a, dummy);
		ref_V34EchoPreFilterCopy(slot_b, dummy);
		V34PremptxCopy(slot_a, dummy);
		ref_V34PremptxCopy(slot_b, dummy);
		for (i = 0; i < (int)sizeof(slot_a); i++)
			diff_eq_int("installed slot", slot_a[i], slot_b[i], i);
	}
	rc |= diff_end();

	diff_begin("v34 echo: coverage");
	printf("  wraps %d, tap sign changes %d, coefficient moves %d\n",
	       saw_wrap, saw_tap_sign, saw_carry);
	diff_eq_int("the delay line wrapped", saw_wrap > 0, 1, saw_wrap);
	diff_eq_int("a coefficient crossed zero", saw_tap_sign > 0, 1,
		    saw_tap_sign);
	diff_eq_int("a fractional part carried", saw_carry > 0, 1, saw_carry);
	rc |= diff_end();

	return rc;
}
