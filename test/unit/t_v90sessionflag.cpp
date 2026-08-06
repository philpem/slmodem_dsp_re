/*
 * t_v90sessionflag.cpp -- the five members of the V.90 setSessionFlag chain,
 * against the blob.
 *
 * WHAT MAKES THIS TEST NON-TRIVIAL IS THE GRAPH, NOT THE ARITHMETIC.  Each of
 * these methods is a store and one or two calls; what can go wrong is which
 * offset is stored, which object is called, and whether the callee is reached
 * through a pointer or by adding a constant to `this`.  So every block is
 * allocated separately on each side, seeded identically with varied bytes,
 * and compared whole -- a store to the wrong offset lands in a `pad_` region
 * that both sides would otherwise still agree on.
 *
 * THE POINTERS HOLD DIFFERENT ADDRESSES ON THE TWO SIDES and always will.
 * Each is replaced, before the compare, by the only thing the two sides can
 * agree about it: whether it still points where setup() put it.  Same idiom
 * as snap_parm/snap_ph2 in t_v90prefilter.cpp.
 *
 * NO SIZE IS ASSERTED anywhere here, because none is settled -- see the
 * header.  Each slot is bigger than the modelled prefix and the whole slot is
 * compared, which catches a store past the last modelled field as well as one
 * inside it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90SessionFlag.h"

/*
 * Our `dsplibs_debug_level` comes from dsplib/debug.h; the object's own copy
 * is renamed like every other symbol in the reference object.  Both are set
 * together or the two sides gate differently.
 */
extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_p3d_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN20V90Phase3Demodulator14setSessionFlagEj");
void ref_p4d_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN20V90Phase4Demodulator14setSessionFlagEj");
void ref_mod_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN12V90Modulator14setSessionFlagEj");
void ref_dem_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN14V90Demodulator14setSessionFlagEj");
void ref_mdm_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN8V90Modem14setSessionFlagEj");
}

/*
 * Slots, each with slack past the modelled prefix so a store that overruns
 * shows up as a difference rather than as memory nobody looks at.
 */
#define SLACK	64

#define P3M_SLOT (0x398 + SLACK)
#define P4M_SLOT (0x2fac + SLACK)
#define P3D_SLOT (0x34 + 0x398 + SLACK)
#define P4D_SLOT (0x50 + 0x2fac + SLACK)
#define MOD_SLOT (0x40 + SLACK)
#define DEM_SLOT (0x1e4 + SLACK)
#define MDM_SLOT (0x49c0 + SLACK)

static unsigned char p3m[2][P3M_SLOT] __attribute__((aligned(8)));
static unsigned char p4m[2][P4M_SLOT] __attribute__((aligned(8)));
static unsigned char p3d[2][P3D_SLOT] __attribute__((aligned(8)));
static unsigned char p4d[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char mod[2][MOD_SLOT] __attribute__((aligned(8)));
static unsigned char dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char mdm[2][MDM_SLOT] __attribute__((aligned(8)));

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * VARIED BYTES, NEVER ZEROS (finding 230).  A zero fill makes a field the
 * function never writes compare equal for the wrong reason, and every one of
 * these objects is mostly fields these methods never write.
 */
static void
fill_pair(unsigned char *a, unsigned char *b, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		a[i] = b[i] = next_byte();
}

static void
seed(int trial)
{
	lfsr_state = 0x4d1u + 0x9e37u * (unsigned)trial;

	fill_pair(p3m[0], p3m[1], P3M_SLOT);
	fill_pair(p4m[0], p4m[1], P4M_SLOT);
	fill_pair(p3d[0], p3d[1], P3D_SLOT);
	fill_pair(p4d[0], p4d[1], P4D_SLOT);
	fill_pair(mod[0], mod[1], MOD_SLOT);
	fill_pair(dem[0], dem[1], DEM_SLOT);
	fill_pair(mdm[0], mdm[1], MDM_SLOT);
}

/* Each side's own graph, wired to that side's own blocks. */
static void
wire(int side)
{
	V90Modulator *m = (V90Modulator *)mod[side];
	V90Demodulator *d = (V90Demodulator *)dem[side];
	V90Modem *k = (V90Modem *)mdm[side];

	m->phase3Modulator = (V90Phase3Modulator *)p3m[side];
	m->phase4Modulator = (V90Phase4Modulator *)p4m[side];
	d->phase3Demodulator = (V90Phase3Demodulator *)p3d[side];
	d->phase4Demodulator = (V90Phase4Demodulator *)p4d[side];
	k->modulator = m;
	k->demodulator = d;
}

/*
 * Copy a block for comparison, replacing every pointer in it with whether it
 * still points where wire() put it.  `dst` is a scratch copy; the live block
 * is not disturbed.
 */
static void
snap_mod(unsigned char *dst, int side)
{
	V90Modulator *s;

	memcpy(dst, mod[side], MOD_SLOT);
	s = (V90Modulator *)dst;
	s->phase3Modulator = (V90Phase3Modulator *)(long)
	    (((V90Modulator *)mod[side])->phase3Modulator
	     == (V90Phase3Modulator *)p3m[side]);
	s->phase4Modulator = (V90Phase4Modulator *)(long)
	    (((V90Modulator *)mod[side])->phase4Modulator
	     == (V90Phase4Modulator *)p4m[side]);
}

static void
snap_dem(unsigned char *dst, int side)
{
	V90Demodulator *s;

	memcpy(dst, dem[side], DEM_SLOT);
	s = (V90Demodulator *)dst;
	s->phase3Demodulator = (V90Phase3Demodulator *)(long)
	    (((V90Demodulator *)dem[side])->phase3Demodulator
	     == (V90Phase3Demodulator *)p3d[side]);
	s->phase4Demodulator = (V90Phase4Demodulator *)(long)
	    (((V90Demodulator *)dem[side])->phase4Demodulator
	     == (V90Phase4Demodulator *)p4d[side]);
}

static void
snap_mdm(unsigned char *dst, int side)
{
	V90Modem *s;

	memcpy(dst, mdm[side], MDM_SLOT);
	s = (V90Modem *)dst;
	s->modulator = (V90Modulator *)(long)
	    (((V90Modem *)mdm[side])->modulator == (V90Modulator *)mod[side]);
	s->demodulator = (V90Demodulator *)(long)
	    (((V90Modem *)mdm[side])->demodulator == (V90Demodulator *)dem[side]);
}

/* Every block, compared whole. */
static void
compare_all(const char *what, long tag)
{
	static unsigned char a[MDM_SLOT], b[MDM_SLOT];

	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Modulator block",
		     p3m[0], p3m[1], P3M_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Modulator block",
		     p4m[0], p4m[1], P4M_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Demodulator block",
		     p3d[0], p3d[1], P3D_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Demodulator block",
		     p4d[0], p4d[1], P4D_SLOT, tag);

	snap_mod(a, 0);
	snap_mod(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Modulator block",
		     a, b, MOD_SLOT, tag);
	snap_dem(a, 0);
	snap_dem(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Demodulator block",
		     a, b, DEM_SLOT, tag);
	snap_mdm(a, 0);
	snap_mdm(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Modem block",
		     a, b, MDM_SLOT, tag);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * The flags fed in.  A store of the wrong WIDTH is the failure a single value
 * hides, so these span one byte, two bytes and the top bit.
 */
static const unsigned int flag_v[] = {
	0u, 1u, 2u, 0x7fu, 0x80u, 0xffu, 0x100u, 0xfffeu, 0xffffu,
	0x10000u, 0x5a5a5a5au, 0x7fffffffu, 0x80000000u, 0xffffffffu
};
#define NFLAG ((int)(sizeof(flag_v) / sizeof(flag_v[0])))

static int
run_leaves(void)
{
	int i, lvl;

	diff_begin("the two demodulators' setSessionFlag");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level((unsigned int)lvl);
		for (i = 0; i < NFLAG; i++) {
			long tag = (long)lvl * 100 + i;

			seed(i + lvl * NFLAG);
			wire(0);
			wire(1);

			((V90Phase3Demodulator *)p3d[0])
			    ->setSessionFlag(flag_v[i]);
			ref_p3d_setSessionFlag(p3d[1], flag_v[i]);

			((V90Phase4Demodulator *)p4d[0])
			    ->setSessionFlag(flag_v[i]);
			ref_p4d_setSessionFlag(p4d[1], flag_v[i]);

			compare_all("after the two demodulators", tag);

			/*
			 * The embedded subobject is the claim worth stating
			 * separately: the flag has to land at +0x34 and +0x50
			 * of these blocks, not through a pointer.
			 */
			diff_eq_int("phase 3 modulator saw it at +0x34 (%ld)",
				    (long)((V90Phase3Demodulator *)p3d[1])
					->phase3Modulator.sessionFlag,
				    (long)flag_v[i], tag);
			diff_eq_int("phase 4 modulator saw it at +0x50 (%ld)",
				    (long)((V90Phase4Demodulator *)p4d[1])
					->phase4Modulator.sessionFlag,
				    (long)flag_v[i], tag);
		}
	}

	set_level(0);
	return diff_end();
}

static int
run_modulator(void)
{
	int i, lvl;

	diff_begin("V90Modulator::setSessionFlag");

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level((unsigned int)lvl);
		for (i = 0; i < NFLAG; i++) {
			long tag = (long)lvl * 100 + i;

			seed(i + 7 + lvl * NFLAG);
			wire(0);
			wire(1);

			((V90Modulator *)mod[0])->setSessionFlag(flag_v[i]);
			ref_mod_setSessionFlag(mod[1], flag_v[i]);

			compare_all("after V90Modulator", tag);
			diff_eq_int("it reached the phase 3 modulator (%ld)",
				    (long)((V90Phase3Modulator *)p3m[1])
					->sessionFlag,
				    (long)flag_v[i], tag);
			diff_eq_int("it reached the phase 4 modulator (%ld)",
				    (long)((V90Phase4Modulator *)p4m[1])
					->sessionFlag,
				    (long)flag_v[i], tag);
		}
	}

	set_level(0);
	return diff_end();
}

static int
run_demodulator(void)
{
	int i, lvl, printed = 0;

	diff_begin("V90Demodulator::setSessionFlag");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level((unsigned int)lvl);
		for (i = 0; i < NFLAG; i++) {
			long tag = (long)lvl * 100 + i;

			seed(i + 19 + lvl * NFLAG);
			wire(0);
			wire(1);
			dsplib_debug_capture_reset();

			((V90Demodulator *)dem[0])->setSessionFlag(flag_v[i]);
			ref_dem_setSessionFlag(dem[1], flag_v[i]);

			compare_all("after V90Demodulator", tag);

			diff_eq_int("transcript line count (%ld)",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1), tag);
			diff_eq_int("transcript text (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			printed += (int)dsplib_debug_capture_lines(0);

			diff_eq_int("it reached the phase 3 demodulator (%ld)",
				    (long)((V90Phase3Demodulator *)p3d[1])
					->sessionFlag,
				    (long)flag_v[i], tag);
			diff_eq_int("it reached the phase 4 demodulator (%ld)",
				    (long)((V90Phase4Demodulator *)p4d[1])
					->sessionFlag,
				    (long)flag_v[i], tag);
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	/* The one diagnostic in the chain has to have actually been emitted. */
	diff_eq_int("the diagnostic was reached", printed > 0, 1, 0);
	return diff_end();
}

/*
 * The only branch in the batch.  Side 0 takes the modulator, 1 the
 * demodulator, and ANYTHING ELSE stores the flag and calls nothing -- so the
 * values below have to include some of that third kind, or the fall-through
 * is never executed and a mutation that removes it survives.
 */
static const int side_v[] = { 0, 1, 2, 3, -1, 0x7fffffff, (int)0x80000000 };
#define NSIDE ((int)(sizeof(side_v) / sizeof(side_v[0])))

static int
run_modem(void)
{
	int i, s, lvl, tookNeither = 0;

	diff_begin("V90Modem::setSessionFlag");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl++) {
		set_level((unsigned int)lvl);
		for (s = 0; s < NSIDE; s++) {
			for (i = 0; i < NFLAG; i++) {
				long tag = (long)lvl * 10000 + s * 100 + i;

				seed(i + 31 * s + lvl * NFLAG);
				wire(0);
				wire(1);
				((V90Modem *)mdm[0])->side = side_v[s];
				((V90Modem *)mdm[1])->side = side_v[s];
				dsplib_debug_capture_reset();

				((V90Modem *)mdm[0])
				    ->setSessionFlag(flag_v[i]);
				ref_mdm_setSessionFlag(mdm[1], flag_v[i]);

				compare_all("after V90Modem", tag);
				diff_eq_int("transcript text (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				/*
				 * The flag lands at +0x49b8 whichever way the
				 * branch goes, including the way that calls
				 * nothing.
				 */
				diff_eq_int("the modem stored the flag (%ld)",
					    (long)((V90Modem *)mdm[1])
						->sessionFlag,
					    (long)flag_v[i], tag);
				if (side_v[s] != 0 && side_v[s] != 1)
					tookNeither = 1;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("a side that calls neither half was reached",
		    tookNeither, 1, 0);
	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_leaves();
	bad |= run_modulator();
	bad |= run_demodulator();
	bad |= run_modem();

	return bad;
}
