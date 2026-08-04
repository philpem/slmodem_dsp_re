/*
 * t_v34mp.cpp -- differential test of `getMPrecvdBits`.
 *
 * The first test in this tree that is C++ because the FUNCTION is, rather
 * than because the class under test is: `_Z14getMPrecvdBitsP12tagV34Object`
 * is VPcmV34Main.cpp's one surviving mangled name, and a C source cannot
 * emit it.  Ours is defined in src/pump/v34/v34pcmmain.cpp and reached here
 * through the ordinary C++ declaration in v34pcmif.h; the blob's is reached
 * by its `ref_` alias, and that alias is the interesting part.
 *
 * THE ALIAS NEEDS BOTH GUARDS AT ONCE, AND NOTHING ELSE IN THE TREE DOES.
 *
 *   - `extern "C"`, because `tools/symmap.py` prepends `ref_` to the raw
 *     symbol string and never demangles.  Declared without it, the compiler
 *     mangles the whole already-mangled string a second time and emits
 *     `_Z36ref__Z14getMPrecvdBitsP12tagV34ObjectPv`, whose link error
 *     contains the name you wanted sitting inside the name you did not.
 *     That is finding 225, and harness.h says so at the bottom of its own
 *     `extern "C"` block.
 *
 *   - `regparm(1)`, because the object keeps this function LOCAL, so GCC gave
 *     it the local calling convention and `--globalize-symbols` does not
 *     change that.  0x9250 is `push %ebp; lea 0x4(%eax),%edx` -- the object
 *     arrives in `%eax` and nothing is read off the stack.  Finding 51's
 *     rule: a `t` symbol is a signal that the convention may not be the C
 *     one.
 *
 * Get the first wrong and the link fails with a message that reads as though
 * the blob does not export the function.  Get the SECOND wrong and it links
 * and passes garbage, which is the worse failure of the two.
 *
 * WHAT IS COMPARED.  The whole 44 KB object, byte for byte, because this is
 * thirty-odd scattered stores and a narrower check would pass one that landed
 * in a neighbouring pad.  Three pointer-sized fields are holes:
 *
 *     +0x3548, +0xac3c   the fixture's, one session and one configuration
 *                        block per side, so the two sides necessarily differ
 *     +0xaa6c            written BY the function, and it is a self-pointer:
 *                        each side is given `m + 0xaa3c` of ITS OWN object,
 *                        so the check is against this side's own base and not
 *                        merely against non-null (finding 224)
 *
 * The memory behind the two fixture pointers is compared as well, which is
 * what justifies their holes: the function only reads it, so "the two sides
 * saw the same input and neither wrote to it" is a real check rather than a
 * gap.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"

extern "C" {

extern unsigned int ref_dsplibs_debug_level;

/* See the header comment: both guards, and neither is optional. */
extern void ref__Z14getMPrecvdBitsP12tagV34Object(void *obj)
	__attribute__((regparm(1)));

}

/* --- the two objects and the blocks they point out of --------------------- */

static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];

#define SESS_LEN	0x6200
#define PCM_LEN		0x0520
#define CFG_LEN		0x0080

#define SESS_MP		0x1744
#define SESS_PCM	0x610c
#define SESS_GATE	0x6120
#define PCM_SENS	0x04f8
#define PCM_RATE	0x04fc
#define CFG_RATE	0x003c

#define OB_INFO		0xaa0c
#define OB_CAPS		0xaa3c
#define OB_CAPS_PTR	0xaa6c
#define OB_DMA		0x2a68

static unsigned char sess_a[SESS_LEN], sess_b[SESS_LEN];
static unsigned char pcm_a[PCM_LEN], pcm_b[PCM_LEN];
static unsigned char cfg_a[CFG_LEN], cfg_b[CFG_LEN];

/*
 * The three pointer-sized holes.  +0xaa6c is LAST because it is the only one
 * the code under test writes, and `saw_written` below distinguishes "the
 * function installed it" from "the fill happened to differ".
 */
static const unsigned ptr_skip[] = { 0x3548, 0xac3c, 0xaa6c };
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))

static int saw_ptr_written[NPTR];

static int
skipped(unsigned off)
{
	unsigned k;

	for (k = 0; k < NPTR; k++)
		if (off >= ptr_skip[k] && off < ptr_skip[k] + 4)
			return 1;
	return 0;
}

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy((unsigned char *)&oa + off, &pa, sizeof(pa));
	memcpy(ob + off, &pb, sizeof(pb));
}

static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_short(unsigned off, short v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static short
get_short(const void *base, unsigned off)
{
	short v;

	memcpy(&v, (const unsigned char *)base + off, sizeof(v));
	return v;
}

static void *
get_ptr(const void *base, unsigned off)
{
	void *p;

	memcpy(&p, (const unsigned char *)base + off, sizeof(p));
	return p;
}

/* --- the fixture ---------------------------------------------------------- */

struct mpcase {
	int	v90;		/* +0x24c, and the branch it selects       */
	int	gate;		/* session +0x6120                         */
	int	sens;		/* PCM receiver +0x4f8                     */
	int	pcm_rate;	/* PCM receiver +0x4fc, times 2400         */
	int	cfg_rate;	/* configuration +0x3c, bits per second    */
	int	seed;		/* varies the six flags and seven shorts   */
	short	info0;		/* +0xaa0c before the call                 */
};

/*
 * Both sides get their own three blocks, filled with the same pattern.  The
 * fill is per-side only in ADDRESS, never in content -- an input that differed
 * between the sides would make every later comparison meaningless.
 */
static void
setup(const struct mpcase *c)
{
	int i;

	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));

	memset(sess_a, 0x3c, sizeof(sess_a));
	memset(sess_b, 0x3c, sizeof(sess_b));
	memset(pcm_a, 0x5a, sizeof(pcm_a));
	memset(pcm_b, 0x5a, sizeof(pcm_b));
	memset(cfg_a, 0x27, sizeof(cfg_a));
	memset(cfg_b, 0x27, sizeof(cfg_b));

	/*
	 * The MP block: six bytes and seven shorts, driven off `seed` so the
	 * sweep visits zero, non-zero, a value larger than its field and a
	 * negative one for each.  Byte 1 is masked to four bits and byte 2 to
	 * two, so 0xff there is a truncation test rather than a wild value.
	 */
	for (i = 0; i < 6; i++) {
		unsigned char v = (unsigned char)((c->seed >> (i * 2)) & 3);

		if (v == 3)
			v = 0xff;
		else if (v == 2)
			v = (unsigned char)(0x80 + i);
		sess_a[SESS_MP + i] = v;
		sess_b[SESS_MP + i] = v;
	}
	for (i = 0; i < 7; i++) {
		short w = (short)(0x4d1b * (i + 1) + c->seed * 331);

		memcpy(sess_a + SESS_MP + 6 + i * 2, &w, sizeof(w));
		memcpy(sess_b + SESS_MP + 6 + i * 2, &w, sizeof(w));
	}

	memcpy(sess_a + SESS_GATE, &c->gate, sizeof(c->gate));
	memcpy(sess_b + SESS_GATE, &c->gate, sizeof(c->gate));
	{
		unsigned char *pa = pcm_a, *pb = pcm_b;

		memcpy(sess_a + SESS_PCM, &pa, sizeof(pa));
		memcpy(sess_b + SESS_PCM, &pb, sizeof(pb));
	}
	memcpy(pcm_a + PCM_SENS, &c->sens, sizeof(c->sens));
	memcpy(pcm_b + PCM_SENS, &c->sens, sizeof(c->sens));
	memcpy(pcm_a + PCM_RATE, &c->pcm_rate, sizeof(c->pcm_rate));
	memcpy(pcm_b + PCM_RATE, &c->pcm_rate, sizeof(c->pcm_rate));
	memcpy(cfg_a + CFG_RATE, &c->cfg_rate, sizeof(c->cfg_rate));
	memcpy(cfg_b + CFG_RATE, &c->cfg_rate, sizeof(c->cfg_rate));

	poke_ptr(0x3548, sess_a, sess_b);
	poke_ptr(0xac3c, cfg_a, cfg_b);
	poke_int(0x024c, c->v90);

	/*
	 * The INFO word before the call.  When the V.90 branch is not taken
	 * this is the ONLY thing that decides whether `txrxdmainit` runs, so
	 * it is an input in its own right and not just an output.
	 */
	poke_short(OB_INFO, c->info0);
}

/* Anti-vacuity: every arm that the sweep is supposed to reach. */
static int saw_v90_branch, saw_dma, saw_no_dma;
static int saw_clamped, saw_rate_skip, saw_rate_written;
static int saw_sensitive, saw_regular;

static void
compare(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	unsigned i, k;
	int bad = 0;

	for (k = 0; k < NPTR; k++)
		if (memcmp(p + ptr_skip[k], ob + ptr_skip[k], 4) != 0)
			saw_ptr_written[k] = 1;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i] || skipped(i))
			continue;
		bad++;
		if (bad <= 8) {
			char msg[160];

			snprintf(msg, sizeof(msg),
				 "%s: object byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(msg, p[i], ob[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);

	/*
	 * The self-pointer, against THIS side's own base.  Comparing the two
	 * sides' values would always fail and testing for non-null would pass
	 * on the fill pattern, so neither is the check.
	 */
	diff_eq_int("caps pointer, ours",
		    get_ptr(&oa, OB_CAPS_PTR) == (unsigned char *)&oa + OB_CAPS,
		    1, tag);
	diff_eq_int("caps pointer, blob",
		    get_ptr(ob, OB_CAPS_PTR) == ob + OB_CAPS, 1, tag);

	/*
	 * The blocks behind the two fixture pointers: read-only, and equal --
	 * except the session's own pointer field, which holds each side's own
	 * PCM block and so differs by construction, exactly as +0x3548 does
	 * one level up.  Its content is checked by the block it selects.
	 */
	diff_eq_int("session untouched, below the pointer",
		    memcmp(sess_a, sess_b, SESS_PCM) == 0, 1, tag);
	diff_eq_int("session untouched, above the pointer",
		    memcmp(sess_a + SESS_PCM + 4, sess_b + SESS_PCM + 4,
			   sizeof(sess_a) - SESS_PCM - 4) == 0, 1, tag);
	diff_eq_int("session's PCM pointer, ours",
		    get_ptr(sess_a, SESS_PCM) == pcm_a, 1, tag);
	diff_eq_int("session's PCM pointer, blob's",
		    get_ptr(sess_b, SESS_PCM) == pcm_b, 1, tag);
	diff_eq_int("pcm block untouched",
		    memcmp(pcm_a, pcm_b, sizeof(pcm_a)) == 0, 1, tag);
	diff_eq_int("config untouched",
		    memcmp(cfg_a, cfg_b, sizeof(cfg_a)) == 0, 1, tag);
}

static void
run(const struct mpcase *c, long tag)
{
	short caps;

	setup(c);

	getMPrecvdBits((struct tagV34Object *)&oa);
	ref__Z14getMPrecvdBitsP12tagV34Object(ob);

	compare("getMPrecvdBits", tag);

	if (c->v90 > 0)
		saw_v90_branch = 1;
	if (get_short(&oa, OB_INFO) & 1)
		saw_dma = 1;
	else
		saw_no_dma = 1;

	caps = get_short(&oa, OB_CAPS);
	if (caps == (short)0x9dc3)
		saw_rate_skip = 1;
	else
		saw_rate_written = 1;

	/*
	 * The clamp, restated from the inputs rather than from the answer:
	 * with the sensitive arm live and the PCM cap below the configured
	 * rate, the rate that reaches the four-bit field is the cap.
	 */
	if (c->gate != 0 && c->v90 > 1 && c->sens != 0
	    && c->cfg_rate > c->pcm_rate * 0x960)
		saw_clamped = 1;
}

int
main(void)
{
	static const int v90s[] = { -1, 0, 1, 2, 7 };
	static const int gates[] = { 0, 1 };
	static const int senss[] = { 0, 3 };
	static const int prates[] = { 0, 5, 14, 20 };
	static const int crates[] = {
		-2400, 0, 1, 2400, 9600, 28800, 33599, 33600, 40000
	};
	static const short info0s[] = { 0, 1, (short)0xffff };
	unsigned a, b, c, d, e, f;
	long tag = 0;
	int rc = 0;

	diff_begin("getMPrecvdBits: the V.90 MP sequence into the V.34 object");
	{
		struct mpcase k;

		for (a = 0; a < sizeof(v90s) / sizeof(v90s[0]); a++)
		    for (b = 0; b < sizeof(gates) / sizeof(gates[0]); b++)
			for (c = 0; c < sizeof(senss) / sizeof(senss[0]); c++)
			    for (d = 0; d < sizeof(prates) / sizeof(prates[0]);
				 d++)
				for (e = 0;
				     e < sizeof(crates) / sizeof(crates[0]);
				     e++) {
					k.v90 = v90s[a];
					k.gate = gates[b];
					k.sens = senss[c];
					k.pcm_rate = prates[d];
					k.cfg_rate = crates[e];
					k.seed = (int)tag;
					k.info0 = info0s[tag % 3];
					run(&k, tag);
					tag++;
				}

		/*
		 * And a pass over the six flag bytes on their own, with
		 * everything else held still, so each field's mask is
		 * exercised against a value that overflows it rather than
		 * only against whatever the seed above happened to produce.
		 */
		for (f = 0; f < 64; f++) {
			k.v90 = 2;
			k.gate = 1;
			k.sens = 1;
			k.pcm_rate = 14;
			k.cfg_rate = 28800;
			k.seed = (int)f * 5 + 1;
			k.info0 = 0;
			run(&k, 100000 + (long)f);
		}

		diff_eq_int("the V.90 branch ran", saw_v90_branch, 1, 0);
		diff_eq_int("txrxdmainit ran", saw_dma, 1, 0);
		diff_eq_int("and did not always run", saw_no_dma, 1, 0);
		diff_eq_int("the rate was clamped by the PCM cap",
			    saw_clamped, 1, 0);
		diff_eq_int("the rate field was left alone at least once",
			    saw_rate_skip, 1, 0);
		diff_eq_int("and rewritten at least once", saw_rate_written,
			    1, 0);
		diff_eq_int("the caps pointer was installed",
			    saw_ptr_written[2], 1, 0);
	}
	rc |= diff_end();

	/*
	 * -------------------------------------------------------------------
	 * The transcripts.  Seven of this function's nine diagnostics are
	 * inside the V.90 branch and each carries a field name that nothing
	 * else in the object states, so a dropped site loses the annotation
	 * and no byte comparison can see it -- finding 134.  The two
	 * `edprintf` messages are the pair that names which ISP arm ran,
	 * which is the only external evidence of that branch.
	 */
	diff_begin("getMPrecvdBits: what it says it did");
	{
		struct mpcase k;
		unsigned lvl;
		static char sens_text[4096], reg_text[4096];

		dsplib_debug_capture_on = 1;
		tag = 0;
		for (lvl = 2; lvl <= 3; lvl++) {
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			for (a = 0; a < sizeof(v90s) / sizeof(v90s[0]); a++)
			    for (b = 0; b < sizeof(gates) / sizeof(gates[0]);
				 b++)
				for (c = 0;
				     c < sizeof(senss) / sizeof(senss[0]); c++)
				    for (e = 0;
					 e < sizeof(crates) / sizeof(crates[0]);
					 e++) {
					    const char *ta, *tb;

					    k.v90 = v90s[a];
					    k.gate = gates[b];
					    k.sens = senss[c];
					    k.pcm_rate = 5;
					    k.cfg_rate = crates[e];
					    k.seed = (int)tag * 7 + 3;
					    k.info0 = 0;

					    dsplib_debug_capture_reset();
					    run(&k, 200000 + tag);
					    ta = dsplib_debug_capture_text(0);
					    tb = dsplib_debug_capture_text(1);
					    diff_eq_int("transcript",
							strcmp(ta, tb) == 0, 1,
							tag);
					    diff_eq_int("transcript non-empty",
							dsplib_debug_capture_lines(1)
							> 0, 1, tag);
					    /*
					     * WHICH ARM RAN IS NOT LEGIBLE IN
					     * THE TRANSCRIPT: `edprintf`
					     * prints its message ENCODED, and
					     * the plain-text switch that
					     * would undo that is ours alone
					     * -- the blob has no such flag,
					     * so setting it would make the
					     * two sides differ for a reason
					     * that has nothing to do with the
					     * modem.  So the arm is named
					     * from the INPUTS, and what the
					     * transcript is asked instead is
					     * that the two arms do not print
					     * the same thing.
					     */
					    if (k.gate != 0 && k.v90 > 1
						&& k.sens != 0) {
						    if (!saw_sensitive)
							    snprintf(sens_text,
								     sizeof(sens_text),
								     "%s", tb);
						    saw_sensitive = 1;
					    } else {
						    if (!saw_regular)
							    snprintf(reg_text,
								     sizeof(reg_text),
								     "%s", tb);
						    saw_regular = 1;
					    }
					    tag++;
				    }
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("the sensitive-ISP arm ran", saw_sensitive, 1, 0);
		diff_eq_int("and the regular one", saw_regular, 1, 0);
		diff_eq_int("and the two do not print the same thing",
			    strcmp(sens_text, reg_text) != 0, 1, 0);
		diff_eq_int("both arms printed something",
			    sens_text[0] != 0 && reg_text[0] != 0, 1, 0);
	}
	rc |= diff_end();

	return rc;
}
