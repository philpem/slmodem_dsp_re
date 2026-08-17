/*
 * t_v90prefilter.cpp -- differential test of V90PreFilter's five members,
 * plus `reset` and the constructor and destructor (finding 1233).
 *
 * FOUR BLOCKS OF MEMORY PER SIDE, not one.  The object holds pointers to a
 * V90Parameters and a V90Phase2Info, the Phase 2 block points at the
 * measurement, and V90Parameters' first word points at the block whose +0x4c
 * is the clock deviation.  All four are seeded identically on the two sides
 * and all four are compared afterwards, because `setParamEia6` writes nothing
 * into the object at all -- it reads `this` at exactly one offset and does
 * every one of its forty-eight stores inside V90Parameters.
 *
 * FOUR WORDS CAN NEVER COMPARE EQUAL AS ADDRESSES, and none of them is
 * skipped (finding 224):
 *
 *   fir.history       a sysdep_malloc return; compared as null / not null,
 *                     with the buffer it points at compared in full
 *   phase2, params    each replaced by its offset from THAT side's own block
 *   fir.coefficients  the hard one.  It points into one of three coefficient
 *                     banks, and our banks and the blob's are at different
 *                     addresses in a different order, so neither the pointer
 *                     nor its distance from any one base is comparable.  What
 *                     IS comparable is WHICH bank: the object leaves the row
 *                     behind in `gain` (as the row itself for the 20-tap
 *                     banks, and as row - 20 for the 40-tap one), so each
 *                     side is asked which of its own three candidate
 *                     addresses the pointer equals.  That works for a row
 *                     past the end of a bank as well as for one inside it,
 *                     which matters because two paths here do not clamp.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90PreFilter.h"

extern "C" {
void fir_ctor(void *self, unsigned n, float *c, unsigned b)
	asm("_ZN8FloatFIRC1EjPfj");
void fir_dtor(void *self) asm("_ZN8FloatFIRD1Ev");
void ref_fir_ctor(void *self, unsigned n, float *c, unsigned b)
	asm("ref__ZN8FloatFIRC1EjPfj");
void ref_fir_dtor(void *self) asm("ref__ZN8FloatFIRD1Ev");

void ref_selectFilter(void *self) asm("ref__ZN12V90PreFilter12selectFilterEv");
void ref_reset(void *self) asm("ref__ZN12V90PreFilter5resetEv");
void ref_setParamEia6(void *self) asm("ref__ZN12V90PreFilter12setParamEia6Ev");
int ref_autoSelection(void *self) asm("ref__ZN12V90PreFilter13autoSelectionEv");
int ref_isV90WithEia6(const void *self)
	asm("ref__ZNK12V90PreFilter13isV90WithEia6Ev");
void ref_displayParamEia6(void *self)
	asm("ref__ZN12V90PreFilter16displayParamEia6Ev");

/*
 * The filter-accessor batch.  Return types are not mangled and these
 * declarations are the reconstruction's reading of them, so they are part of
 * what is under test rather than given: a `float *` where the object returned
 * something else would still link.
 */
unsigned int ref_getFilterLength(void *self, unsigned int gain)
	asm("ref__ZN12V90PreFilter15getFilterLengthEj");
float *ref_getFilterPointer(void *self, unsigned int gain)
	asm("ref__ZN12V90PreFilter16getFilterPointerEj");
void ref_setFilterGain(void *self, unsigned int gain)
	asm("ref__ZN12V90PreFilter9setFilterEj");
void ref_setFilterType(void *self, int type, unsigned int gain)
	asm("ref__ZN12V90PreFilter9setFilterE17PreFilterCoefTypej");
int ref_getV90Capability(void *self)
	asm("ref__ZN12V90PreFilter16getV90CapabilityEv");
int ref_getNofRefLoops(const void *self)
	asm("ref__ZNK12V90PreFilter14getNofRefLoopsEv");

extern float ref_coef1[31][20]
	asm("ref__ZN12V90PreFilter18preFilterCoefType1E");
extern float ref_coef2[31][20]
	asm("ref__ZN12V90PreFilter18preFilterCoefType2E");
extern float ref_coef3[31][40]
	asm("ref__ZN12V90PreFilter18preFilterCoefType3E");
extern V90RefLoop ref_loops1[23] asm("ref__ZN12V90PreFilter13refLoopsType1E");
extern V90RefLoop ref_loops2[33] asm("ref__ZN12V90PreFilter13refLoopsType2E");
extern V90RefLoop ref_loops4[36] asm("ref__ZN12V90PreFilter13refLoopsType4E");
extern V90RefLoop ref_loops5[36] asm("ref__ZN12V90PreFilter13refLoopsType5E");
extern V90RefLoop ref_loops6[34] asm("ref__ZN12V90PreFilter13refLoopsType6E");
extern V90RefLoop ref_loops7[34] asm("ref__ZN12V90PreFilter13refLoopsType7E");
extern V90CodecEntry ref_dataBase[17] asm("ref__ZN12V90PreFilter8dataBaseE");
}

/* The FIR shape the class's own constructor uses: 40 taps, 99 of slack. */
#define FIR_TAPS 0x28
#define FIR_SLACK 0x63
#define FIR_BUF (FIR_TAPS + FIR_SLACK)

#define SLOT 64
#define PARMSLOT (V90PARAMETERS_BOUND + 64)
#define PH2SLOT (sizeof(V90Phase2Info) + 32)
#define MEASSLOT 0x80
#define BLKSLOT 0x80

static unsigned char slot[2][SLOT] __attribute__((aligned(8)));
static unsigned char parm[2][PARMSLOT] __attribute__((aligned(8)));
static unsigned char ph2[2][PH2SLOT] __attribute__((aligned(8)));
static unsigned char meas[2][MEASSLOT] __attribute__((aligned(8)));
static unsigned char blk[2][BLKSLOT] __attribute__((aligned(8)));

struct pf_meas {
	unsigned char b[MEASSLOT];
};
struct pf_blk {
	unsigned char b[BLKSLOT];
};
struct pf_hist {
	unsigned int w[FIR_BUF];
};

static V90PreFilter *
P(int side)
{
	return (V90PreFilter *)slot[side];
}

/*
 * The Phase 2 block, as the class it now is.  It used to be an opaque union
 * declared in V90PreFilter.h, so this test reached into it as `b[0x18]` and
 * `w[0x18 / 4]`; the real V90Phase2Info landed with finding 255 and the stub
 * is gone.
 */
static V90Phase2Info *
P2(int side)
{
	return (V90Phase2Info *)ph2[side];
}

static unsigned lfsr_state;

static unsigned
lfsr(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return lfsr_state;
}

static void
fill(unsigned char *a, unsigned char *b, int n, int mode)
{
	int i;

	for (i = 0; i < n; i++) {
		unsigned char v;

		switch (mode) {
		case 0:
			v = (unsigned char)(lfsr() >> 3);
			break;
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = (unsigned char)(i * 7 + 1);
			break;
		default:
			v = (unsigned char)(lfsr() >> 5) ^ 0x3c;
			break;
		}
		a[i] = v;
		b[i] = v;
	}
}

/*
 * Build both objects from the same bytes.  Never zeroed: a zero fill lets a
 * store that never happened compare equal to one that did, which is the
 * failure findings 223 and 224 record.
 */
static void
setup(int trial, int mode)
{
	int side;

	lfsr_state = 0x71c3u + 0x9e37u * (unsigned)trial;
	fill(slot[0], slot[1], SLOT, mode);
	fill(parm[0], parm[1], PARMSLOT, mode);
	fill(ph2[0], ph2[1], PH2SLOT, mode);
	fill(meas[0], meas[1], MEASSLOT, mode);
	fill(blk[0], blk[1], BLKSLOT, mode);

	fir_ctor(slot[0], FIR_TAPS, 0, FIR_SLACK);
	ref_fir_ctor(slot[1], FIR_TAPS, 0, FIR_SLACK);

	for (side = 0; side < 2; side++) {
		P(side)->phase2 = (V90Phase2Info *)ph2[side];
		P(side)->params = (V90Parameters *)parm[side];
		P(side)->codecType = 0;
		P(side)->gain = 0;
		P(side)->refLoop = -1;
		P2(side)->L2 = (float *)meas[side];
		*(void **)&parm[side][0] = blk[side];
	}
}

static void
teardown(void)
{
	fir_dtor(slot[0]);
	ref_fir_dtor(slot[1]);
}

static void
set_int(int off, int v)
{
	*(int *)&parm[0][off] = v;
	*(int *)&parm[1][off] = v;
}

static void
set_meas(int off, float v)
{
	*(float *)&meas[0][off] = v;
	*(float *)&meas[1][off] = v;
}

/* The three candidate coefficient addresses, on the named side. */
static float *
cand(int side, int bank, int row)
{
	if (side == 0) {
		if (bank == 1)
			return &V90PreFilter::preFilterCoefType1[0][0] +
			       20L * row;
		if (bank == 2)
			return &V90PreFilter::preFilterCoefType2[0][0] +
			       20L * row;
		return &V90PreFilter::preFilterCoefType3[0][0] - 800L +
		       40L * row;
	}
	if (bank == 1)
		return &ref_coef1[0][0] + 20L * row;
	if (bank == 2)
		return &ref_coef2[0][0] + 20L * row;
	return &ref_coef3[0][0] - 800L + 40L * row;
}

/*
 * Which bank the coefficient pointer came out of, asked of each side about
 * its own tables.  The row is `gain` for the 20-tap banks and `gain + 20` for
 * the 40-tap one, which is how the object leaves it.
 */
static int
which_bank(int side)
{
	const float *p = P(side)->fir.coefficients;
	int g = P(side)->gain;

	int c30 = ((unsigned int)g > 30) ? 30 : g;
	int c50 = ((unsigned int)g > 50) ? 50 : g;
	static const int bank[] = { 1, 2, 1, 2, 3, 3, 3 };
	int row[7];
	int i;

	if (p == 0)
		return -2;

	/*
	 * The row is not one formula.  It is `gain` on the registry paths
	 * through banks 1 and 2, `gain + 20` on the registry path through
	 * bank 3, and the CLAMPED gain on the automatic paths -- so a single
	 * formula stops comparing the pointer exactly where the clamps live,
	 * which is where the interesting mutations are.  Every row the object
	 * could have used is enumerated instead, and a pointer that matches
	 * none of them reports as unresolved, which is itself a difference
	 * when the other side resolved.
	 *
	 * A wider search does not work: the blob's three banks are contiguous
	 * and 2480 bytes apart, so bank 1 at row r + 31 is the same address as
	 * bank 2 at row r, and ours are neither contiguous nor in that order.
	 * An exhaustive search resolves the same address to different (bank,
	 * row) pairs on the two sides and fails a correct run.
	 */
	row[0] = g;
	row[1] = g;
	row[2] = c30;
	row[3] = c30;
	row[4] = g + 20;
	row[5] = c50;
	row[6] = g;

	for (i = 0; i < 7; i++)
		if (p == cand(side, bank[i], row[i]))
			return bank[i] * 1000 + row[i] + 512;
	return -1;
}

static void
snapshot(void *dst, int side)
{
	V90PreFilter *s = (V90PreFilter *)dst;

	memcpy(dst, slot[side], sizeof(V90PreFilter));
	s->fir.coefficients = (float *)(long)which_bank(side);
	s->fir.history = (float *)(P(side)->fir.history != 0 ? 1 : 0);
	s->phase2 = (V90Phase2Info *)((char *)P(side)->phase2 -
				      (char *)ph2[side]);
	s->params = (V90Parameters *)((char *)P(side)->params -
				      (char *)parm[side]);
}

/* V90Parameters holds one pointer, at +0x00; likewise Phase 2 at +0x18. */
static void
snap_parm(void *dst, int side)
{
	memcpy(dst, parm[side], V90PARAMETERS_BOUND);
	V90PW(dst)[0] = (*(void **)&parm[side][0] == (void *)blk[side]);
}

/*
 * The measurement pointer holds a different address on each side and always
 * will, so it is replaced by the only thing about it that the two sides can
 * agree on: whether it still points where setup() put it.  Same idiom as
 * snap_parm above.  Everything else is compared byte for byte.
 */
static void
snap_ph2(void *dst, int side)
{
	memcpy(dst, ph2[side], sizeof(V90Phase2Info));
	((V90Phase2Info *)dst)->L2 =
	    (float *)(long)(P2(side)->L2 == (float *)meas[side]);
}

static void
compare(const char *what, int trial)
{
	unsigned char a[sizeof(V90PreFilter)], b[sizeof(V90PreFilter)];
	unsigned char pa[V90PARAMETERS_BOUND], pb[V90PARAMETERS_BOUND];
	unsigned char qa[sizeof(V90Phase2Info)], qb[sizeof(V90Phase2Info)];
	struct pf_hist ha, hb;

	snapshot(a, 0);
	snapshot(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90PreFilter", a, b,
		     sizeof(V90PreFilter), (long)trial);
	diff_eq_int("no store past the object (%ld)",
		    memcmp(slot[0] + sizeof(V90PreFilter),
			   slot[1] + sizeof(V90PreFilter),
			   SLOT - sizeof(V90PreFilter)) == 0, 1, trial);
	snap_parm(pa, 0);
	snap_parm(pb, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Parameters", pa, pb,
		     V90PARAMETERS_BOUND, (long)trial);
	diff_eq_int("no store past V90Parameters (%ld)",
		    memcmp(parm[0] + V90PARAMETERS_BOUND,
			   parm[1] + V90PARAMETERS_BOUND,
			   PARMSLOT - V90PARAMETERS_BOUND) == 0, 1, trial);

	snap_ph2(qa, 0);
	snap_ph2(qb, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase2Info", qa, qb,
		     sizeof(V90Phase2Info), (long)trial);
	diff_eq_int("no store past V90Phase2Info (%ld)",
		    memcmp(ph2[0] + sizeof(V90Phase2Info),
			   ph2[1] + sizeof(V90Phase2Info),
			   PH2SLOT - sizeof(V90Phase2Info)) == 0, 1, trial);

	diff_eq_obj(what, struct pf_meas, meas[0], meas[1], trial);
	diff_eq_obj(what, struct pf_blk, blk[0], blk[1], trial);

	memset(&ha, 0, sizeof(ha));
	memset(&hb, 0, sizeof(hb));
	if (P(0)->fir.history != 0)
		memcpy(&ha, P(0)->fir.history, sizeof(ha));
	if (P(1)->fir.history != 0)
		memcpy(&hb, P(1)->fir.history, sizeof(hb));
	diff_eq_obj(what, struct pf_hist, &ha, &hb, trial);
}

/* Compare everything except the coefficient pointer's resolution. */
/* How many reference loops the codec at `c` has. */
static int
nloops(int c)
{
	int n = 0;

	while (V90PreFilter::dataBase[c].loops[n].name[0] != '\0')
		n++;
	return n;
}

static int
run_display(void)
{
	int trial;

	diff_begin("V90PreFilter::displayParamEia6");

	for (trial = 0; trial < 4; trial++) {
		setup(trial, trial);
		P(0)->displayParamEia6();
		ref_displayParamEia6(slot[1]);
		compare("after displayParamEia6", trial);
		teardown();
	}

	return diff_end();
}

static int
run_iseia6(void)
{
	static const int reg[] = { 0, 5, 6, 7, -1 };
	int c, r, k, saw1 = 0, saw0 = 0;

	diff_begin("V90PreFilter::isV90WithEia6");

	for (c = 0; c < 16; c++) {
		int n = nloops(c);
		V90RefLoop keepA = V90PreFilter::dataBase[c].loops[0];
		V90RefLoop keepB = ref_dataBase[c].loops[0];

		/*
		 * No shipped table marks its first entry capability 2, so
		 * `refLoop >= 0` and `refLoop > 0` are indistinguishable on
		 * the data as it ships.  Mark it, symmetrically, for half the
		 * sweep.
		 */
		for (r = -2; r < n; r++) {
			V90PreFilter::dataBase[c].loops[0].capability =
			    ((r & 1) == 0) ? 2 : keepA.capability;
			ref_dataBase[c].loops[0].capability =
			    ((r & 1) == 0) ? 2 : keepB.capability;
			for (k = 0; k < 5; k++) {
				int a, b;

				setup(c * 8 + k, (c + k) % 4);
				P(0)->codecType = P(1)->codecType = c;
				P(0)->refLoop = P(1)->refLoop = (r == -2) ? -5 : r;
				set_int(0x500, reg[k]);

				a = P(0)->isV90WithEia6();
				b = ref_isV90WithEia6(slot[1]);

				diff_eq_int("isV90WithEia6 (codec %ld)", a, b, c);
				compare("after isV90WithEia6", c);
				if (a)
					saw1 = 1;
				else
					saw0 = 1;
				teardown();
			}
		}
		V90PreFilter::dataBase[c].loops[0] = keepA;
		ref_dataBase[c].loops[0] = keepB;
	}

	diff_eq_int("some call said yes", saw1, 1, 0);
	diff_eq_int("some call said no", saw0, 1, 0);

	return diff_end();
}

/*
 * The measurement Phase 2 leaves behind: a reference at +0x38 and six points
 * at +0x3c, of which the search sees only the six differences.
 */
static void
set_measurement(int c, int loop, float scale, float bias)
{
	int i;

	set_meas(0x38, 0.0f);
	if (loop < 0) {
		for (i = 0; i < 6; i++)
			set_meas(0x3c + 4 * i, bias + (float)i * scale);
		return;
	}
	/* Aim at one table entry, displaced by `bias`. */
	for (i = 0; i < 6; i++)
		set_meas(0x3c + 4 * i,
			 -(V90PreFilter::dataBase[c].loops[loop].signature[i] +
			   bias));
}

static int
run_autoselection(void)
{
	int c, loop, saw_stuck = 0, saw_moved = 0;

	diff_begin("V90PreFilter::autoSelection");

	for (c = 0; c < 16; c++) {
		int n = nloops(c);

		for (loop = -1; loop < n; loop++) {
			int mode;

			for (mode = 0; mode < 8; mode++) {
				int a, b, seeded = 3 % (n ? n : 1);

				setup(c * 32 + loop + 2, mode);
				P(0)->codecType = P(1)->codecType = c;
				/*
				 * Seeded to a valid entry, NOT to what the
				 * search should find: nothing here resets it,
				 * and the report at the end reads it back.
				 */
				P(0)->refLoop = P(1)->refLoop = seeded;

				if (mode >= 3) {
					/*
					 * Straddling the initial 1e10.  Six
					 * components of d**2 each, so a
					 * displacement of 40826 is the
					 * threshold: 3e5 is hopeless and
					 * leaves refLoop untouched, 2e4 wins
					 * easily, and 4e4 and 4.2e4 sit either
					 * side of it.  Without these the
					 * starting distance is unmeasurable.
					 */
					static const float bias[5] = {
						3.0e5f, 2.0e4f, 4.0e4f,
						4.2e4f, 4.0826e4f
					};

					set_measurement(c, -1, 0.0f,
							bias[mode - 3]);
				} else
					set_measurement(c, loop < 0 ? 0 : loop,
							1.0f,
							(float)mode * 0.01f);

				a = P(0)->autoSelection();
				b = ref_autoSelection(slot[1]);

				diff_eq_int("autoSelection (codec %ld)", a, b, c);
				compare("after autoSelection", c);

				if (P(0)->refLoop == seeded && mode >= 3)
					saw_stuck = 1;
				if (P(0)->refLoop != seeded)
					saw_moved = 1;
				teardown();
			}
		}
	}

	diff_eq_int("a hopeless measurement left refLoop alone", saw_stuck, 1,
		    0);
	diff_eq_int("a plausible measurement moved it", saw_moved, 1, 0);

	return diff_end();
}

/*
 * selectFilter's registry-driven arms.  `w[0x0c]` 1 and 2 take the row
 * straight out of the registry with no clamp at all, so those are driven both
 * inside and outside the banks; the rest are clamped and stay in range.
 */
static int
run_selectfilter_registry(void)
{
	static const int rows[] = { 0, 1, 18, 19, 20, 21, 28, 29, 30, 31, 48,
				    49, 50, 51, -1, -7, 100 };
	static const int forced[] = { -2, -1, 0, 1, 2, 3, 4 };
	int c, i, j, conn;

	diff_begin("V90PreFilter::selectFilter (registry)");

	for (c = 0; c < 16; c += 3) {
		for (conn = 0; conn <= 2; conn++) {
			for (i = 0; i < (int)(sizeof(rows) / sizeof(rows[0]));
			     i++) {
				for (j = 0; j < (int)(sizeof(forced) /
						      sizeof(forced[0])); j++) {
		
					setup(c * 64 + i * 8 + j, (i + j) % 4);
					P(0)->codecType = P(1)->codecType = c;
					set_int(0x0c, conn);
					set_int(0x4c, rows[i]);
					set_int(0x50, forced[j]);
					set_int(0x54, rows[i]);
					set_int(0x58, rows[(i + 1) %
						 (int)(sizeof(rows) /
						       sizeof(rows[0]))]);

					P(0)->selectFilter();
					ref_selectFilter(slot[1]);

					/*
					 * The unclamped arms can leave the
					 * pointer outside every bank, and an
					 * address outside a bank is not
					 * comparable across two layouts --
					 * `which_bank` still resolves it,
					 * because it asks each side about its
					 * own candidates.
					 */
					compare("after selectFilter",
						i * 8 + j);
					diff_eq_int("both sides resolved the"
						    " bank the same way (%ld)",
						    which_bank(0),
						    which_bank(1), i * 8 + j);
					teardown();
				}
			}
		}
	}

	return diff_end();
}

/*
 * selectFilter's automatic arm.  Reached with registry +0x4c == -1, and the
 * bank then comes from the reference loop the search picks.
 */
static int
run_selectfilter_auto(void)
{
	int c, loop, seedgain, saw_same = 0, saw_bank[4];
	int k;

	for (k = 0; k < 4; k++)
		saw_bank[k] = 0;

	diff_begin("V90PreFilter::selectFilter (automatic)");

	for (c = 0; c < 16; c++) {
		int n = nloops(c);

		for (loop = 0; loop < n; loop++) {
			for (seedgain = 0; seedgain < 3; seedgain++) {
				int want =
				    V90PreFilter::dataBase[c].loops[loop].gain;
				int t = V90PreFilter::dataBase[c]
					    .loops[loop].coefType;

				setup(c * 128 + loop * 4 + seedgain,
				      (c + loop) % 4);
				P(0)->codecType = P(1)->codecType = c;
				set_int(0x0c, 0);
				set_int(0x4c, -1);
				set_int(0x50, 0);
				set_measurement(c, loop, 1.0f, 0.0f);

				/* Equal to the answer on one pass, so the
				 * "nothing changed" early return is driven. */
				P(0)->gain = P(1)->gain =
				    (seedgain == 0) ? want : want + 1 + seedgain;

				P(0)->selectFilter();
				ref_selectFilter(slot[1]);

				compare("after selectFilter (auto)",
					c * 64 + loop);
				diff_eq_int("both sides resolved the bank the"
					    " same way (%ld)",
					    which_bank(0), which_bank(1), loop);

				if (seedgain == 0 && P(0)->fir.taps == FIR_TAPS)
					saw_same = 1;
				if (t >= 0 && t <= 3)
					saw_bank[t] = 1;
				teardown();
			}
		}
	}

	diff_eq_int("the equal-gain early return was taken", saw_same, 1, 0);
	/*
	 * No counted record in any of the six tables has coefType 0 -- only
	 * the terminators do, and the walk stops before them -- so the
	 * unsupported-type arm is unreachable from the shipped data and is
	 * driven by the synthetic run below instead.
	 */
	diff_eq_int("no shipped record has coefType 0", saw_bank[0], 0, 0);
	diff_eq_int("a coefType 1 entry was selected", saw_bank[1], 1, 0);
	diff_eq_int("a coefType 2 entry was selected", saw_bank[2], 1, 0);
	diff_eq_int("a coefType 3 entry was selected", saw_bank[3], 1, 0);

	return diff_end();
}

/*
 * The automatic arm's clamps and its unknown-type message, driven by putting
 * values in a reference loop record that the shipped tables do not contain --
 * every gain over 30 in them has coefType 3, so nothing else can reach a
 * clamp.  Both sides' records are edited identically and restored afterwards.
 */
static int
run_selectfilter_synthetic(void)
{
	static const int types[] = { -1, 0, 1, 2, 3, 4, 9 };
	static const int gains[] = { 0, 20, 29, 30, 31, 49, 50, 51, 80, -3 };
	V90RefLoop saveA = V90PreFilter::refLoopsType1[1];
	V90RefLoop saveB = ref_loops1[1];
	V90RefLoop save0A = V90PreFilter::refLoopsType1[0];
	V90RefLoop save0B = ref_loops1[0];
	int i, j, conn;

	diff_begin("V90PreFilter::selectFilter (synthetic loop record)");

	for (i = 0; i < (int)(sizeof(types) / sizeof(types[0])); i++) {
		for (j = 0; j < (int)(sizeof(gains) / sizeof(gains[0])); j++) {
		    for (conn = 0; conn <= 2; conn++) {
			int ntypes = (int)(sizeof(types) / sizeof(types[0]));
			int ngains = (int)(sizeof(gains) / sizeof(gains[0]));

			V90PreFilter::refLoopsType1[1].coefType = types[i];
			V90PreFilter::refLoopsType1[1].gain = gains[j];
			ref_loops1[1].coefType = types[i];
			ref_loops1[1].gain = gains[j];
			/*
			 * Entry ZERO as well.  The two connection-type arms
			 * take the bank from it and never look at the selected
			 * loop, and no shipped table has an entry 0 whose type
			 * differs from entry 1's -- so without this the two
			 * entries are interchangeable, and so are bank 1 and
			 * the unsupported-type arm.
			 */
			V90PreFilter::refLoopsType1[0].coefType =
			    types[(i + 3) % ntypes];
			ref_loops1[0].coefType = types[(i + 3) % ntypes];

			setup(i * 16 + j, (i + j) % 4);
			/* Codec 1 is AD1821, whose table is refLoopsType1. */
			P(0)->codecType = P(1)->codecType = 1;
			set_int(0x0c, conn);
			set_int(0x4c, -1);
			set_int(0x50, 0);
			set_int(0x54, gains[j]);
			set_int(0x58, gains[(j + 1) % ngains]);
			set_measurement(1, 1, 1.0f, 0.0f);
			P(0)->gain = P(1)->gain = gains[j] + 7;

			P(0)->selectFilter();
			ref_selectFilter(slot[1]);

			compare("after selectFilter (synthetic)",
				i * 16 + j);
			diff_eq_int("both sides resolved the bank the same way"
				    " (%ld)", which_bank(0), which_bank(1),
				    i * 16 + j);
			if (conn == 0)
				diff_eq_int("the synthetic entry was the one"
					    " chosen (%ld)", P(0)->refLoop, 1,
					    i * 16 + j);
			teardown();
		    }
		}
	}

	V90PreFilter::refLoopsType1[1] = saveA;
	ref_loops1[1] = saveB;
	V90PreFilter::refLoopsType1[0] = save0A;
	ref_loops1[0] = save0B;

	return diff_end();
}

/*
 * THE THREE THINGS ABOUT autoSelection's ARITHMETIC THAT THE SHIPPED TABLES
 * CANNOT SHOW.  The search's only observable outputs are a loop index and an
 * integer gain, so a difference of one part in 10**18 changes nothing unless
 * two entries are close enough for it to reorder them -- and no two entries in
 * the shipped tables are.  Each case below puts a pair of entries at a
 * measured distance that differs only in the bits the claim is about, and asks
 * which one wins.  Both sides' records are edited identically and restored.
 *
 *   ACC      the accumulator is an x87 register, 64 significant bits, not a
 *            double: two entries at 1 and 1 + 2**-60 apart reorder if it is
 *            only 53
 *   TARGET   each of the six differences is ROUNDED TO FLOAT before the search
 *            sees it (`fstps`): a measurement whose exact difference needs 25
 *            bits reorders the pair if the rounding is skipped
 *   D        each difference from a signature is NOT rounded -- it stays in
 *            the register -- so a pair whose exact differences need 37 bits
 *            reorders if it is rounded
 *
 * 2**-7, 2**-30 and 100 are all exact as floats, and so is every difference
 * spelled below except the one each case is about.
 */
#define P7 0.0078125f			/* 2**-7  */
#define P30 9.31322574615478515625e-10f	/* 2**-30, one ULP of 2**-7 */
#define P29 1.86264514923095703125e-9f	/* 2**-29 */
#define P31 4.656612873077392578125e-10f /* 2**-31, half an ULP of 2**-7 */

static void
syn(int idx, float s0, float s1, float s2, float s3, float s4, float s5,
    int gain)
{
	V90RefLoop *a = &V90PreFilter::refLoopsType1[idx];
	V90RefLoop *b = &ref_loops1[idx];
	int i;
	float v[6];

	v[0] = s0; v[1] = s1; v[2] = s2; v[3] = s3; v[4] = s4; v[5] = s5;
	for (i = 0; i < 6; i++) {
		a->signature[i] = v[i];
		b->signature[i] = v[i];
	}
	a->name[0] = b->name[0] = 'X';
	a->name[1] = b->name[1] = '\0';
	a->coefType = b->coefType = 1;
	a->gain = b->gain = gain;
	a->capability = b->capability = 1;
}

static int
run_autoselection_x87(void)
{
	V90RefLoop keep1 = V90PreFilter::refLoopsType1[1];
	V90RefLoop keep2 = V90PreFilter::refLoopsType1[2];
	V90RefLoop rkeep1 = ref_loops1[1];
	V90RefLoop rkeep2 = ref_loops1[2];
	V90RefLoop keep0 = V90PreFilter::refLoopsType1[0];
	V90RefLoop rkeep0 = ref_loops1[0];
	int which, mode, a, b;

	diff_begin("V90PreFilter::autoSelection x87");

	for (which = 0; which < 4; which++) {
		for (mode = 0; mode < 4; mode++) {
			setup(which * 4 + mode, mode);
			P(0)->codecType = P(1)->codecType = 1;
			P(0)->refLoop = P(1)->refLoop = (which == 3) ? 5 : 0;

			V90PreFilter::refLoopsType1[0] = keep0;
			ref_loops1[0] = rkeep0;
			if (which == 3) {
				/*
				 * An EMPTY first entry, which no shipped table
				 * has.  The count is then zero, nothing beats
				 * the starting distance, and the report at the
				 * end reads back the seeded index -- which is
				 * the only way to see that the walk starts at
				 * entry 0 rather than at entry 1.
				 */
				V90PreFilter::refLoopsType1[0].name[0] = '\0';
				ref_loops1[0].name[0] = '\0';
			}

			if (which == 0) {
				/* ACC: entry 1 is 1 + 2**-60 away, entry 2 is
				 * exactly 1. */
				set_meas(0x38, 100.0f);
				set_meas(0x3c + 0, 0.0f);
				set_meas(0x3c + 4, 100.0f - P7);
				set_meas(0x3c + 8, 100.0f - P7);
				set_meas(0x3c + 12, 100.0f);
				set_meas(0x3c + 16, 100.0f);
				set_meas(0x3c + 20, 100.0f);
				syn(1, 99.0f, P7 - P30, P7, 0.0f, 0.0f, 0.0f, 5);
				syn(2, 99.0f, P7, P7, 0.0f, 0.0f, 0.0f, 6);
			} else if (which == 1) {
				/* TARGET: the second difference needs 25 bits,
				 * so rounding it to float makes the pair tie
				 * and the first entry keeps the lead. */
				set_meas(0x38, P7);
				set_meas(0x3c + 0, P7 - 100.0f);
				set_meas(0x3c + 4, -P31);
				set_meas(0x3c + 8, 0.0f);
				set_meas(0x3c + 12, P7);
				set_meas(0x3c + 16, P7);
				set_meas(0x3c + 20, P7);
				syn(1, 99.0f, P7 - P30, P7, 0.0f, 0.0f, 0.0f, 5);
				syn(2, 99.0f, P7, P7 - P30, 0.0f, 0.0f, 0.0f, 6);
			} else if (which == 2) {
				/* D: 100 - 2**-30 and 100 - 2**-29 both round
				 * to 100 as floats and do not as registers. */
				set_meas(0x38, 100.0f);
				set_meas(0x3c + 0, 0.0f);
				set_meas(0x3c + 4, 0.0f);
				set_meas(0x3c + 8, 0.0f);
				set_meas(0x3c + 12, 0.0f);
				set_meas(0x3c + 16, 0.0f);
				set_meas(0x3c + 20, 0.0f);
				syn(1, P30, 100.0f, 100.0f, 100.0f, 100.0f,
				    100.0f, 5);
				syn(2, P29, 100.0f, 100.0f, 100.0f, 100.0f,
				    100.0f, 6);
			} else {
				/* Aimed at entry 7, not at the seeded 5: a walk
				 * that skipped entry 0 would find 7 and be
				 * indistinguishable if the two agreed. */
				set_measurement(1, 7, 1.0f, 0.0f);
			}

			a = P(0)->autoSelection();
			b = ref_autoSelection(slot[1]);

			diff_eq_int("autoSelection x87 (case %ld)", a, b,
				    which);
			compare("after autoSelection x87", which * 4 + mode);
			/*
			 * Anti-vacuity: the pair really did decide it.  Each
			 * case's correct winner is entry 2, 1 and 2 in turn --
			 * stated so that a run where some third entry won,
			 * which would make the case prove nothing, fails here
			 * rather than passing quietly.
			 */
			diff_eq_int("the synthetic pair decided it (case %ld)",
				    P(0)->refLoop,
				    (which == 3) ? 5 : ((which == 1) ? 1 : 2),
				    which);
			teardown();
		}
	}

	V90PreFilter::refLoopsType1[1] = keep1;
	V90PreFilter::refLoopsType1[2] = keep2;
	ref_loops1[1] = rkeep1;
	ref_loops1[2] = rkeep2;
	V90PreFilter::refLoopsType1[0] = keep0;
	ref_loops1[0] = rkeep0;

	return diff_end();
}

static int
run_setparam(void)
{
	static const int dev[] = { 0, 1, -1, 7, -7, 999, 1000, 1001, -1000,
				   12345, -12345, 32767, -32768, 1000000,
				   -1000000, 2147483, -2147483 };
	int i, mode, saw_zero = 0, saw_nonzero = 0;

	diff_begin("V90PreFilter::setParamEia6");

	for (i = 0; i < (int)(sizeof(dev) / sizeof(dev[0])); i++) {
		for (mode = 0; mode < 4; mode++) {
			setup(i * 4 + mode, mode);
			*(int *)&blk[0][0x4c] = dev[i];
			*(int *)&blk[1][0x4c] = dev[i];

			P(0)->setParamEia6();
			ref_setParamEia6(slot[1]);

			compare("after setParamEia6", i * 4 + mode);
			if (dev[i] == 0)
				saw_zero = 1;
			else
				saw_nonzero = 1;
			teardown();
		}
	}

	diff_eq_int("a zero deviation was driven", saw_zero, 1, 0);
	diff_eq_int("a non-zero deviation was driven", saw_nonzero, 1, 0);

	return diff_end();
}

/*
 * V90PreFilter::reset -- FloatFIR::reset, two stores, and the type 1 bank at
 * gain 0 (task #88).
 *
 * THE FIR IS THE POINT.  A whole-object comparison would pass on a `reset`
 * that never called `FloatFIR::reset` at all, because both sides start from
 * the same fill and neither writes a coefficient it did not already have --
 * so the check that the FIR really was reloaded is that `fir.coefficients`
 * comes out pointing at `preFilterCoefType1` ROW 0 on the blob's side, read
 * against the blob's OWN copy of the table.  The two copies are at different
 * addresses, which is exactly why the check is worth making: our side must
 * hold ours and the blob's must hold the blob's, and neither may hold the
 * other's.
 *
 * The gain and the reference loop are asserted by value rather than only
 * compared: two never-reset objects agree with each other (finding 1105), and
 * -1 is the one value in `refLoop` that a random fill will not produce twice
 * in a row by accident.
 */
static int
run_reset(void)
{
	int trial, mode;
	int saw_moved = 0;

	diff_begin("V90PreFilter::reset");

	for (mode = 0; mode < 4; mode++)
		for (trial = 0; trial < 8; trial++) {
			long tag = (long)mode * 100 + trial;
			unsigned char before[SLOT];

			setup(trial + 4000, mode);

			/* Something other than what reset must produce. */
			P(0)->gain = P(1)->gain = 17;
			P(0)->refLoop = P(1)->refLoop = 5;

			memcpy(before, slot[1], SLOT);

			P(0)->reset();
			ref_reset(slot[1]);

			compare("after reset", (int)tag);
			diff_eq_int("gain (%ld)", P(1)->gain, 0, tag);
			diff_eq_int("refLoop (%ld)", P(1)->refLoop, -1, tag);

			/*
			 * The FIR was reloaded, and from the FIRST ROW: the
			 * blob's coefficient pointer is the blob's own table
			 * base and ours is ours.
			 */
			diff_eq_int("the blob took its own table (%ld)",
				    P(1)->fir.coefficients == &ref_coef1[0][0],
				    1, tag);
			diff_eq_int("and ours took ours (%ld)",
				    P(0)->fir.coefficients
				    == &V90PreFilter::preFilterCoefType1[0][0],
				    1, tag);
			diff_eq_int("twenty taps (%ld)",
				    (long)P(1)->fir.taps, 20, tag);

			if (memcmp(before, slot[1], SLOT) != 0)
				saw_moved = 1;

			teardown();
		}

	diff_eq_int("reset changed the object", saw_moved, 1, 0);

	return diff_end();
}

/* ================================================================ lifecycle */

/*
 * THE CONSTRUCTOR AND THE DESTRUCTOR, DRIVEN BY SYMBOL.
 *
 * C++ HAS NO SYNTAX FOR RUNNING A CONSTRUCTOR OVER STORAGE THAT ALREADY
 * EXISTS.  `P(0) = V90PreFilter(...)` would construct a temporary over
 * uninitialised stack and copy it in, which destroys the one property the
 * fixture depends on -- that the object is SEEDED and never zeroed, so a slot
 * the constructor does not write is visibly the seed rather than a plausible
 * zero (findings 223, 224).  So both sides are called through asm() labels,
 * and BOTH the C1 and the C2 variant are driven: the blob has them as two
 * identical copies at different addresses and our compiler emits one function
 * under both names, so a test that drove only one would leave half the pair
 * unexercised.  Same for D1 and D2.
 *
 * WHAT THE ALLOCATOR SEES IS PART OF THE ANSWER.  The FIR inside the object
 * takes one `sysdep_malloc` and the destructor gives it back; neither is
 * visible in any field, because the two sides' history pointers hold
 * different addresses and `snapshot` has always reduced that field to
 * null/not-null.  So the allocation COUNT and the exact number of BYTES
 * REQUESTED are compared between the sides as well, and the pair is required
 * to balance.  A constructor that took the wrong buffer size would pass every
 * field comparison in this file and fail here.
 */

extern "C" {
void pf_ctor1(void *self, int codec, void *info, void *parms)
	asm("_ZN12V90PreFilterC1E23__tHardwareCodecTypes__P13V90Phase2Info"
	    "P13V90Parameters");
void pf_ctor2(void *self, int codec, void *info, void *parms)
	asm("_ZN12V90PreFilterC2E23__tHardwareCodecTypes__P13V90Phase2Info"
	    "P13V90Parameters");
void ref_pf_ctor1(void *self, int codec, void *info, void *parms)
	asm("ref__ZN12V90PreFilterC1E23__tHardwareCodecTypes__P13V90Phase2Info"
	    "P13V90Parameters");
void ref_pf_ctor2(void *self, int codec, void *info, void *parms)
	asm("ref__ZN12V90PreFilterC2E23__tHardwareCodecTypes__P13V90Phase2Info"
	    "P13V90Parameters");

void pf_dtor1(void *self) asm("_ZN12V90PreFilterD1Ev");
void pf_dtor2(void *self) asm("_ZN12V90PreFilterD2Ev");
void ref_pf_dtor1(void *self) asm("ref__ZN12V90PreFilterD1Ev");
void ref_pf_dtor2(void *self) asm("ref__ZN12V90PreFilterD2Ev");

extern unsigned int ref_dsplibs_debug_level;

void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);
}

/*
 * The last index the constructor's range check will accept: `dataBase` is
 * walked to its first empty name and ONE IS SUBTRACTED, so the last entry of
 * the table cannot be selected through the argument.  Computed here rather
 * than written down, because the expectation has to follow the table.
 */
static int
last_codec(void)
{
	int n = 0;

	while (V90PreFilter::dataBase[n].name[0] != '\0')
		n++;
	return n - 1;
}

/* What the constructor must leave in `codecType`, from its two inputs. */
static int
expected_codec(int hw, int codec)
{
	int c;

	if (hw < 0)
		c = codec;
	else
		c = (hw >= 0 && hw <= 15) ? hw : 0;

	if (c > last_codec())
		c = 0;
	return c;
}

/*
 * The fill and the wiring `setup` does, WITHOUT the FloatFIR constructor and
 * without the five field stores: the object's own constructor is what is
 * under test and it does both itself.  The object slot keeps its seed.
 */
static void
setup_bare(int trial, int mode)
{
	int side;

	lfsr_state = 0x71c3u + 0x9e37u * (unsigned)trial;
	fill(slot[0], slot[1], SLOT, mode);
	fill(parm[0], parm[1], PARMSLOT, mode);
	fill(ph2[0], ph2[1], PH2SLOT, mode);
	fill(meas[0], meas[1], MEASSLOT, mode);
	fill(blk[0], blk[1], BLKSLOT, mode);

	for (side = 0; side < 2; side++) {
		P2(side)->L2 = (float *)meas[side];
		*(void **)&parm[side][0] = blk[side];
	}
}

static int
run_ctor(void)
{
	/*
	 * The registry's codec type.  Negative means "not configured" and
	 * hands the decision to the argument; 0 .. 15 are the switch's arms;
	 * 16 and up take its default.
	 */
	static const int hw_v[] = {
		-1, -3, (int)0x80000000, 0, 1, 7, 15, 16, 99, 0x7fffffff
	};
	static const int codec_v[] = { 0, 3, 15, 16, 40 };
	int hi, ci, mode, lvl;
	long trial = 900000;
	int saw_arg = 0, saw_switch = 0, saw_default = 0;
	int saw_over = 0, saw_inrange = 0;

	diff_begin("V90PreFilter::V90PreFilter");

	dsplib_debug_capture_on = 1;

	for (hi = 0; hi < (int)(sizeof(hw_v) / sizeof(hw_v[0])); hi++)
	    for (ci = 0; ci < (int)(sizeof(codec_v) / sizeof(codec_v[0])); ci++)
		for (mode = 0; mode < 4; mode++)
		    for (lvl = 0; lvl < 2; lvl++) {
			struct alloc_log a0, a1, a2, a3, a4;
			int codec = codec_v[ci];
			int hw = hw_v[hi];

			trial++;

			dsplibs_debug_level = ref_dsplibs_debug_level =
			    lvl ? 2u : 0u;

			setup_bare((int)trial, mode);
			set_int(0x08, hw);
			dsplib_debug_capture_reset();

			a0 = harness_alloc;
			if (trial & 1)
				pf_ctor1(slot[0], codec, ph2[0], parm[0]);
			else
				pf_ctor2(slot[0], codec, ph2[0], parm[0]);
			a1 = harness_alloc;
			if (trial & 1)
				ref_pf_ctor1(slot[1], codec, ph2[1], parm[1]);
			else
				ref_pf_ctor2(slot[1], codec, ph2[1], parm[1]);
			a2 = harness_alloc;

			compare("after the constructor", (int)trial);

			diff_eq_int("allocations (%ld)",
				    a1.allocs - a0.allocs,
				    a2.allocs - a1.allocs, trial);
			diff_eq_int("bytes asked for (%ld)",
				    (long)(a1.bytes - a0.bytes),
				    (long)(a2.bytes - a1.bytes), trial);
			diff_eq_int("one allocation, the FIR's (%ld)",
				    a1.allocs - a0.allocs, 1, trial);
			diff_eq_int("and it is (taps + slack) floats (%ld)",
				    (long)(a1.bytes - a0.bytes),
				    (long)(FIR_BUF * sizeof(float)), trial);

			diff_eq_int("transcript (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, trial);
			diff_eq_int("the level gates the transcript (%ld)",
				    dsplib_debug_capture_lines(0) != 0,
				    lvl ? 1 : 0, trial);

			/*
			 * The state the constructor is required to leave,
			 * asserted and not only compared: two never-touched
			 * objects would agree with each other.
			 */
			diff_eq_int("gain (%ld)", P(1)->gain, 0, trial);
			diff_eq_int("refLoop (%ld)", P(1)->refLoop, -1, trial);
			diff_eq_int("taps (%ld)", (long)P(1)->fir.taps, 20,
				    trial);
			diff_eq_int("bufferLength (%ld)",
				    (long)P(1)->fir.bufferLength, FIR_BUF,
				    trial);
			diff_eq_int("phase2 (%ld)",
				    P(1)->phase2 == P2(1), 1, trial);
			diff_eq_int("params (%ld)",
				    (void *)P(1)->params ==
				    (void *)parm[1], 1, trial);
			diff_eq_int("coefficients are bank 1 row 0 (%ld)",
				    P(0)->fir.coefficients ==
				    &V90PreFilter::preFilterCoefType1[0][0],
				    1, trial);
			diff_eq_int("and the blob's are its own (%ld)",
				    P(1)->fir.coefficients == &ref_coef1[0][0],
				    1, trial);

			diff_eq_int("codecType (%ld)", P(1)->codecType,
				    expected_codec(hw, codec), trial);
			diff_eq_int("and ours agrees (%ld)", P(0)->codecType,
				    expected_codec(hw, codec), trial);

			if (hw < 0) {
				saw_arg = 1;
				if (codec > last_codec())
					saw_over = 1;
				else
					saw_inrange = 1;
			} else if (hw <= 15) {
				saw_switch = 1;
			} else {
				saw_default = 1;
			}

			/* And the destructor gives the one block back. */
			a3 = harness_alloc;
			if (trial & 1)
				pf_dtor1(slot[0]);
			else
				pf_dtor2(slot[0]);
			a4 = harness_alloc;
			if (trial & 1)
				ref_pf_dtor1(slot[1]);
			else
				ref_pf_dtor2(slot[1]);

			diff_eq_int("frees (%ld)", a4.frees - a3.frees,
				    harness_alloc.frees - a4.frees, trial);
			diff_eq_int("the pair balances (%ld)",
				    harness_alloc.frees - a3.frees,
				    a2.allocs - a0.allocs, trial);
			diff_eq_int("nothing wild was freed (%ld)",
				    harness_alloc.bad_free - a3.bad_free, 0,
				    trial);
			diff_eq_int("live back to where it started (%ld)",
				    harness_alloc.live, a0.live, trial);

			/*
			 * The destructor changes NOTHING in the object -- it
			 * does not clear the pointer it just freed -- so the
			 * two sides still agree, and `fir.history` is still
			 * not null on either.
			 */
			diff_eq_int("the destructor left the object alone "
				    "(%ld)",
				    P(0)->fir.history != 0 &&
				    P(1)->fir.history != 0, 1, trial);
		    }

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("the argument was used", saw_arg, 1, 0);
	diff_eq_int("the switch was used", saw_switch, 1, 0);
	diff_eq_int("the switch's default was used", saw_default, 1, 0);
	diff_eq_int("an index past the table was seen", saw_over, 1, 0);
	diff_eq_int("an index inside it was seen", saw_inrange, 1, 0);

	return diff_end();
}

/*
 * The destructor on its own, with the FIR's history pointer chosen rather
 * than allocated: null on one arm and a real block on the other, so the null
 * test inside `FloatFIR::~FloatFIR` is exercised in both directions and a
 * missing guard shows up as `free_null` moving.
 */
static int
run_dtor(void)
{
	int mode, null, which;
	long trial = 950000;
	int saw_freed = 0, saw_null = 0;

	diff_begin("V90PreFilter::~V90PreFilter");

	for (mode = 0; mode < 4; mode++)
	    for (null = 0; null < 2; null++)
		for (which = 0; which < 2; which++) {
			struct alloc_log a0, a1, a2;
			unsigned char before[SLOT];

			trial++;
			setup_bare((int)trial, mode);

			P(0)->fir.history = P(1)->fir.history = 0;
			if (!null) {
				P(0)->fir.history =
				    (float *)sysdep_malloc(FIR_BUF *
							   sizeof(float));
				P(1)->fir.history =
				    (float *)sysdep_malloc(FIR_BUF *
							   sizeof(float));
			}
			memcpy(before, slot[1], SLOT);

			a0 = harness_alloc;
			if (which)
				pf_dtor1(slot[0]);
			else
				pf_dtor2(slot[0]);
			a1 = harness_alloc;
			if (which)
				ref_pf_dtor1(slot[1]);
			else
				ref_pf_dtor2(slot[1]);
			a2 = harness_alloc;

			diff_eq_int("frees (%ld)", a1.frees - a0.frees,
				    a2.frees - a1.frees, trial);
			diff_eq_int("free(NULL) (%ld)",
				    a1.free_null - a0.free_null,
				    a2.free_null - a1.free_null, trial);
			diff_eq_int("wild frees (%ld)",
				    a1.bad_free - a0.bad_free,
				    a2.bad_free - a1.bad_free, trial);
			diff_eq_int("freed what it was given (%ld)",
				    a1.frees - a0.frees, null ? 0 : 1, trial);
			diff_eq_int("and never freed a null (%ld)",
				    a2.free_null - a0.free_null, 0, trial);
			diff_eq_int("the object is untouched (%ld)",
				    memcmp(before, slot[1], SLOT) == 0, 1,
				    trial);

			if (null)
				saw_null = 1;
			else
				saw_freed = 1;
		}

	diff_eq_int("a block was freed", saw_freed, 1, 0);
	diff_eq_int("a null was skipped", saw_null, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * THE FILTER ACCESSORS -- `getNofRefLoops`, `getFilterLength`,
 * `getFilterPointer` and the two `setFilter` overloads, plus
 * `getV90Capability`.
 *
 * WHAT MAKES THESE HARD TO TEST IS THAT FOUR OF THE SIX RETURN SOMETHING.  A
 * returned value is not in the object, so the whole-object comparison above
 * says nothing about it -- a `getFilterPointer` that returned the wrong bank
 * and stored nothing would pass every check `compare()` makes.  Each suite
 * below therefore compares the RETURN as well, and the pointer is compared by
 * resolving it against each side's own tables (see `resolve_ptr`).
 *
 * THE TRANSCRIPT IS THE SECOND OBSERVABLE, and it is the only one that can
 * see the unsupported-type arm.  `getFilterLength` and `getFilterPointer`
 * both answer 20 taps / bank 1 for an unrecognised `coefType`, exactly as
 * they answer for type 1, so the two arms differ ONLY in the `BUGMSG` that
 * one of them prints.  Both levels are raised and both transcripts compared.
 *
 * TWO ARMS FORM A COEFFICIENT POINTER OUTSIDE ITS BANK, and the trials below
 * drive them, deliberately, exactly as `run_selectfilter_synthetic` above
 * already does -- `getFilterPointer` skips its clamp entirely when no
 * reference loop is selected, and neither `setFilter` overload clamps at all.
 * The pointer is arithmetic and is never dereferenced (`FloatFIR::
 * setCoefficients` stores it and reads nothing through it), which is the
 * ground on which V90PreFilter.cpp's `bank1`/`bank2`/`bank3` already say
 * "`row` is deliberately not range-checked".  docs/deviations.md D670.
 *
 * ONE PATH IS EXCLUDED AND IT IS A REAL DEREFERENCE, not arithmetic:
 * `getV90Capability` reads `loops[refLoop].capability` after `autoSelection`
 * has run, and if the search matched nothing `refLoop` is still negative
 * there.  Every trial in `run_getv90capability` below leaves a well-formed
 * measurement, so the search always lands on a record; a trial that did not
 * would be testing OUR undefined behaviour and not the object's (D561).
 * ===========================================================================
 */

/*
 * Which (bank, row) a coefficient pointer names, asked of each side about its
 * own tables.  Same argument as `which_bank` above and the same hazard: the
 * blob's three banks are contiguous and ours are neither contiguous nor in
 * that order, so an exhaustive search resolves one address to different
 * (bank, row) pairs on the two sides and fails a correct run.  Only the four
 * pairs these two functions can produce are tried, in a fixed order.
 */
static long
resolve_ptr(int side, const float *p, unsigned int gain)
{
	static const int bank[4] = { 1, 1, 2, 3 };
	int c30 = (gain > 30) ? 30 : (int)gain;
	int c50 = (gain > 50) ? 50 : (int)gain;
	int row[4];
	int i;

	row[0] = (int)gain;	/* refLoop == -1: no clamp on this arm  */
	row[1] = c30;		/* bank 1, clamped                      */
	row[2] = c30;		/* bank 2, clamped                      */
	row[3] = c50;		/* bank 3, clamped; its row 0 is index 20 */

	for (i = 0; i < 4; i++)
		if (p == cand(side, bank[i], row[i]))
			return bank[i] * 1000L + row[i] + 512;

	return -1;
}

/*
 * `getNofRefLoops` -- the table walk, and the empty name that ends it.
 *
 * The shipped tables give the count for free, but they cannot show that the
 * TERMINATOR is what stops the walk rather than a fixed length: every codec
 * that shares a table has the same count.  So the second half of the sweep
 * blanks a name in the middle of one table, symmetrically on both sides, and
 * asks again.
 */
static int
run_getnofrefloops(void)
{
	int c, k, trial = 0;
	int distinct = 0, prev = -1, sawcut = 0;

	diff_begin("V90PreFilter::getNofRefLoops");

	for (c = 0; c < 16; c++)
		for (k = 0; k < 3; k++) {
			int a, b;

			trial++;
			setup(trial, (c + k) % 4);
			P(0)->codecType = P(1)->codecType = c;

			a = P(0)->getNofRefLoops();
			b = ref_getNofRefLoops(slot[1]);

			diff_eq_int("getNofRefLoops (%ld)", a, b, trial);
			diff_eq_int("and it is the table's length (%ld)", a,
				    nloops(c), trial);
			compare("after getNofRefLoops", trial);

			if (a != prev) {
				distinct++;
				prev = a;
			}
			teardown();
		}

	/* The terminator, moved. */
	for (c = 0; c < 16; c++) {
		V90RefLoop keepA = V90PreFilter::dataBase[c].loops[1];
		V90RefLoop keepB = ref_dataBase[c].loops[1];
		int a, b;

		if (nloops(c) < 3)
			continue;

		trial++;
		V90PreFilter::dataBase[c].loops[1].name[0] = '\0';
		ref_dataBase[c].loops[1].name[0] = '\0';

		setup(trial, c % 4);
		P(0)->codecType = P(1)->codecType = c;

		a = P(0)->getNofRefLoops();
		b = ref_getNofRefLoops(slot[1]);

		diff_eq_int("getNofRefLoops, table cut short (%ld)", a, b,
			    trial);
		diff_eq_int("and the cut is where it stopped (%ld)", a, 1,
			    trial);
		if (a == 1)
			sawcut++;
		teardown();

		V90PreFilter::dataBase[c].loops[1] = keepA;
		ref_dataBase[c].loops[1] = keepB;
	}

	/*
	 * Anti-vacuity, and every counter below counts trials whose RETURNED
	 * VALUE differed -- never a branch believed taken (finding 3509).
	 */
	diff_eq_int("more than one distinct count was returned (%ld)",
		    distinct > 1, 1, 0);
	diff_eq_int("the terminator was moved and seen (%ld)", sawcut > 0, 1,
		    0);

	return diff_end();
}

/*
 * `getFilterLength` and `getFilterPointer`, over a synthetic `coefType` that
 * the shipped tables do not contain.
 */
static int
run_filteraccessors(void)
{
	static const int types[] = { -1, 0, 1, 2, 3, 4, 9 };
	static const unsigned int gains[] = {
		0, 1, 5, 20, 29, 30, 31, 35, 49, 50, 51, 80
	};
	static const int loopsel[] = { -1, 0, 1 };
	V90RefLoop saveA = V90PreFilter::refLoopsType1[1];
	V90RefLoop saveB = ref_loops1[1];
	V90RefLoop save0A = V90PreFilter::refLoopsType1[0];
	V90RefLoop save0B = ref_loops1[0];
	int ntypes = (int)(sizeof(types) / sizeof(types[0]));
	int ngains = (int)(sizeof(gains) / sizeof(gains[0]));
	int nsel = (int)(sizeof(loopsel) / sizeof(loopsel[0]));
	int i, j, l, trial = 0;
	int saw20 = 0, saw40 = 0;
	int sawbank1 = 0, sawbank2 = 0, sawbank3 = 0;
	int sawmsg = 0, sawquiet = 0;
	int sawclamped = 0, sawunclamped = 0;

	diff_begin("V90PreFilter::getFilterLength / getFilterPointer");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

	for (i = 0; i < ntypes; i++)
		for (j = 0; j < ngains; j++)
			for (l = 0; l < nsel; l++) {
				unsigned int la, lb;
				float *pa, *pb;
				long ra, rb;

				trial++;

				V90PreFilter::refLoopsType1[1].coefType =
				    types[i];
				ref_loops1[1].coefType = types[i];
				V90PreFilter::refLoopsType1[0].coefType =
				    types[(i + 3) % ntypes];
				ref_loops1[0].coefType = types[(i + 3) % ntypes];

				setup(trial, (i + j + l) % 4);
				/* Codec 1 is AD1821, whose table is type 1. */
				P(0)->codecType = P(1)->codecType = 1;
				P(0)->refLoop = P(1)->refLoop = loopsel[l];
				P(0)->gain = P(1)->gain = (int)gains[j] + 7;

				dsplib_debug_capture_reset();

				la = P(0)->getFilterLength(gains[j]);
				lb = ref_getFilterLength(slot[1], gains[j]);
				pa = P(0)->getFilterPointer(gains[j]);
				pb = ref_getFilterPointer(slot[1], gains[j]);

				ra = resolve_ptr(0, pa, gains[j]);
				rb = resolve_ptr(1, pb, gains[j]);

				diff_eq_int("getFilterLength (%ld)", (long)la,
					    (long)lb, trial);
				diff_eq_int("getFilterPointer (%ld)", ra, rb,
					    trial);
				diff_eq_int("both sides resolved the pointer"
					    " (%ld)", ra >= 0, 1, trial);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, trial);
				compare("after the filter accessors", trial);

				if (la == 20)
					saw20++;
				if (la == 40)
					saw40++;
				if (ra / 1000 == 1)
					sawbank1++;
				if (ra / 1000 == 2)
					sawbank2++;
				if (ra / 1000 == 3)
					sawbank3++;
				if (dsplib_debug_capture_lines(0) != 0)
					sawmsg++;
				else
					sawquiet++;
				if (ra >= 0 &&
				    (ra % 1000) - 512 != (int)gains[j])
					sawclamped++;
				if (ra >= 0 &&
				    (ra % 1000) - 512 == (int)gains[j])
					sawunclamped++;

				teardown();
			}

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	V90PreFilter::refLoopsType1[1] = saveA;
	ref_loops1[1] = saveB;
	V90PreFilter::refLoopsType1[0] = save0A;
	ref_loops1[0] = save0B;

	/*
	 * Anti-vacuity.  Every one of these counts trials whose RETURNED
	 * VALUE or PRINTED OUTPUT differed from another trial's, which is what
	 * finding 3509 asks for; none of them counts a branch believed taken.
	 */
	diff_eq_int("both tap counts were returned (%ld)",
		    saw20 > 0 && saw40 > 0, 1, 0);
	diff_eq_int("all three banks were returned (%ld)",
		    sawbank1 > 0 && sawbank2 > 0 && sawbank3 > 0, 1, 0);
	diff_eq_int("the unsupported-type message fired, and did not (%ld)",
		    sawmsg > 0 && sawquiet > 0, 1, 0);
	diff_eq_int("a clamp fired, and did not (%ld)",
		    sawclamped > 0 && sawunclamped > 0, 1, 0);

	return diff_end();
}

/*
 * `setFilter(unsigned)` -- the same two accessors, installed, and the guard
 * that makes a repeat call do nothing at all.
 *
 * THE GUARD NEEDS TWO CALLS TO BE VISIBLE.  Half the trials below enter with
 * `gain` already equal to the argument, where the object stores nothing and
 * leaves `fir.coefficients` at the NULL the FIR constructor left; the other
 * half enter with it different.  Every trial then calls a SECOND time with
 * the same argument, which must change nothing whatever the first call did.
 */
static int
run_setfilter_gain(void)
{
	static const int types[] = { -1, 1, 2, 3, 4 };
	static const unsigned int gains[] = {
		0, 5, 20, 29, 30, 31, 35, 49, 50, 51, 80
	};
	static const int loopsel[] = { -1, 0, 1 };
	V90RefLoop saveA = V90PreFilter::refLoopsType1[1];
	V90RefLoop saveB = ref_loops1[1];
	int ntypes = (int)(sizeof(types) / sizeof(types[0]));
	int ngains = (int)(sizeof(gains) / sizeof(gains[0]));
	int nsel = (int)(sizeof(loopsel) / sizeof(loopsel[0]));
	int i, j, l, trial = 0;
	int sawinstalled = 0, sawskipped = 0;

	diff_begin("V90PreFilter::setFilter(unsigned)");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

	for (i = 0; i < ntypes; i++)
		for (j = 0; j < ngains; j++)
			for (l = 0; l < nsel; l++) {
				int same = (trial & 1);

				trial++;

				V90PreFilter::refLoopsType1[1].coefType =
				    types[i];
				ref_loops1[1].coefType = types[i];

				setup(trial, (i + j + l) % 4);
				P(0)->codecType = P(1)->codecType = 1;
				P(0)->refLoop = P(1)->refLoop = loopsel[l];
				P(0)->gain = P(1)->gain =
				    same ? (int)gains[j] : (int)gains[j] + 7;

				dsplib_debug_capture_reset();

				P(0)->setFilter(gains[j]);
				ref_setFilterGain(slot[1], gains[j]);

				compare("after setFilter(gain)", trial);
				diff_eq_int("both sides resolved the bank the"
					    " same way (%ld)", which_bank(0),
					    which_bank(1), trial);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, trial);

				if (P(1)->fir.coefficients == 0)
					sawskipped++;
				else
					sawinstalled++;

				/* Again, with the same gain: nothing may move. */
				P(0)->setFilter(gains[j]);
				ref_setFilterGain(slot[1], gains[j]);
				compare("after setFilter(gain) repeated", trial);

				teardown();
			}

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	V90PreFilter::refLoopsType1[1] = saveA;
	ref_loops1[1] = saveB;

	diff_eq_int("the guard skipped a call, and let one through (%ld)",
		    sawskipped > 0 && sawinstalled > 0, 1, 0);

	return diff_end();
}

/*
 * `setFilter(PreFilterCoefType, unsigned)` -- the caller names the bank, and
 * nothing here consults a reference loop or clamps anything.
 *
 * `gain` IS THE OBSERVABLE FOR BANK 3.  Only that arm corrects the field to
 * `gain - 20` after the call, and only the whole-object comparison sees it.
 */
static int
run_setfilter_type(void)
{
	static const int types[] = { -3, 0, 1, 2, 3, 4, 7 };
	static const unsigned int gains[] = {
		0, 5, 20, 29, 30, 31, 35, 49, 50, 51, 80
	};
	int ntypes = (int)(sizeof(types) / sizeof(types[0]));
	int ngains = (int)(sizeof(gains) / sizeof(gains[0]));
	int i, j, trial = 0;
	int sawcorrected = 0, sawplain = 0;

	diff_begin("V90PreFilter::setFilter(PreFilterCoefType, unsigned)");

	for (i = 0; i < ntypes; i++)
		for (j = 0; j < ngains; j++) {
			trial++;

			setup(trial, (i + j) % 4);
			P(0)->codecType = P(1)->codecType = 1;
			P(0)->refLoop = P(1)->refLoop = 1;
			P(0)->gain = P(1)->gain = (int)gains[j] + 7;

			P(0)->setFilter((PreFilterCoefType)types[i], gains[j]);
			ref_setFilterType(slot[1], types[i], gains[j]);

			compare("after setFilter(type, gain)", trial);
			diff_eq_int("both sides resolved the bank the same way"
				    " (%ld)", which_bank(0), which_bank(1),
				    trial);

			if (P(1)->gain == (int)gains[j] - 20)
				sawcorrected++;
			if (P(1)->gain == (int)gains[j])
				sawplain++;

			teardown();
		}

	diff_eq_int("bank 3 corrected the row, and the others did not (%ld)",
		    sawcorrected > 0 && sawplain > 0, 1, 0);

	return diff_end();
}

/*
 * `getV90Capability` -- 1 when the connection is EIA-6 by either of
 * `isV90WithEia6`'s two routes, and the reference loop's own capability word
 * otherwise.
 *
 * NO TRIAL LEAVES `refLoop` NEGATIVE ACROSS THE SEARCH.  The negative arms
 * below hand `autoSelection` a well-formed measurement aimed at a real
 * record, so it always selects one; see the block comment above for why a
 * trial that did not would not be a trial.
 */
static int
run_getv90capability(void)
{
	static const int caps[] = { 0, 1, 2, 3, -1 };
	static const int reg[] = { 0, 5, 6, 7, -1 };
	static const int sel[] = { 0, 1, -1, -5 };
	int nsel = (int)(sizeof(sel) / sizeof(sel[0]));
	int c, s, k, trial = 0;
	int sawone = 0, sawother = 0, distinct = 0, prev = -99;

	diff_begin("V90PreFilter::getV90Capability");

	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = ref_dsplibs_debug_level = 2u;

	for (c = 0; c < 16; c++) {
		int n = nloops(c);
		V90RefLoop keep0A = V90PreFilter::dataBase[c].loops[0];
		V90RefLoop keep0B = ref_dataBase[c].loops[0];
		V90RefLoop keep1A = V90PreFilter::dataBase[c].loops[1];
		V90RefLoop keep1B = ref_dataBase[c].loops[1];

		if (n < 2)
			continue;

		for (s = 0; s < nsel; s++)
			for (k = 0; k < 5; k++) {
				int a, b, cap = caps[(s + k) % 5];

				trial++;

				V90PreFilter::dataBase[c].loops[0].capability =
				    cap;
				ref_dataBase[c].loops[0].capability = cap;
				V90PreFilter::dataBase[c].loops[1].capability =
				    caps[(s + k + 2) % 5];
				ref_dataBase[c].loops[1].capability =
				    caps[(s + k + 2) % 5];

				setup(trial, (c + k) % 4);
				P(0)->codecType = P(1)->codecType = c;
				P(0)->refLoop = P(1)->refLoop = sel[s];
				set_int(0x500, reg[k]);
				/*
				 * Aimed at entry 1, so the search lands on a
				 * record whatever the shipped signatures are.
				 */
				set_measurement(c, 1, 1.0f, 0.0f);

				dsplib_debug_capture_reset();

				a = P(0)->getV90Capability();
				b = ref_getV90Capability(slot[1]);

				diff_eq_int("getV90Capability (%ld)", a, b,
					    trial);
				diff_eq_int("a reference loop was selected"
					    " (%ld)", P(1)->refLoop >= 0, 1,
					    trial);
				diff_eq_int("transcript (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, trial);
				compare("after getV90Capability", trial);

				if (a == 1)
					sawone++;
				else
					sawother++;
				if (a != prev) {
					distinct++;
					prev = a;
				}

				teardown();
			}

		V90PreFilter::dataBase[c].loops[0] = keep0A;
		ref_dataBase[c].loops[0] = keep0B;
		V90PreFilter::dataBase[c].loops[1] = keep1A;
		ref_dataBase[c].loops[1] = keep1B;
	}

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	diff_eq_int("it returned 1, and it returned something else (%ld)",
		    sawone > 0 && sawother > 0, 1, 0);
	diff_eq_int("more than two distinct answers were returned (%ld)",
		    distinct > 2, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_ctor();
	rc |= run_dtor();
	rc |= run_reset();
	rc |= run_display();
	rc |= run_iseia6();
	rc |= run_autoselection();
	rc |= run_autoselection_x87();
	rc |= run_setparam();
	rc |= run_selectfilter_registry();
	rc |= run_selectfilter_auto();
	rc |= run_selectfilter_synthetic();
	rc |= run_getnofrefloops();
	rc |= run_filteraccessors();
	rc |= run_setfilter_gain();
	rc |= run_setfilter_type();
	rc |= run_getv90capability();

	return rc;
}
