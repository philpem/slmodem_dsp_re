/*
 * t_v34info.c -- differential test of the V.34 INFO0/INFO1a codecs.
 *
 * These five functions write in four places, not one: the V.34 object, the
 * caller's message buffer, the session object at +0x3548 and two blocks the
 * session points at.  A test that compared only the V.34 object would miss
 * the most interesting store in three of the five -- `V34GiveINFO1aBits`
 * clears `session + 0x611c` on EVERY call and writes three bytes into the
 * capability block, and none of that is in the object at all.  So every
 * block each side owns is compared byte for byte.
 *
 * GUARD BANDS.  This is the first batch that reads past +0xac10, which is
 * where `struct v34_object` used to end, and 0xac1c is a lower bound rather
 * than a size: the object's real length is not known.  Each reference-side
 * block therefore has a filled band after it that must come back unchanged,
 * which turns "the blob wrote past our map" from silent corruption of the
 * next static into a failed check.  The message buffer gets the same
 * treatment, because `V34SetINFO0dBits` writes index 12 and nothing in the
 * object bounds it (finding 129).
 *
 * POINTERS are skipped in the byte compare and checked by what they select,
 * as in t_v34hshak -- and here the session object has pointers inside it
 * too, so the same discipline applies one level down.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34info.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_V34GiveProbeResults(void *obj, const void *src);
extern void ref_V34SetINFO0aBits(void *obj, short *bits);
extern void ref_V34SetINFO0dBits(void *obj, short *bits);
extern void ref_V34GiveINFO0dBits(void *obj, const short *bits);
extern int ref_V34GiveINFO1aBits(void *obj, const short *bits);
extern void ref_VPcmV34SetMohMessageBits(void *obj, short *bits);

/* --- the blocks, each with a guard band on the reference side ------------- */

#define GUARD		64
#define GUARD_FILL	0x3c

#define SESSION_LEN	0x6140
#define SUB_LEN		0x40

/*
 * EACH BLOCK AND ITS GUARD ARE ONE OBJECT, not two statics.  Nothing
 * requires the linker to lay two statics out adjacently -- it may reorder
 * them, align them apart, or separate them entirely -- and a guard band that
 * did not follow the block it guards would pass unconditionally and quietly
 * turn this whole mechanism into decoration.  A struct member's position is
 * the language's problem instead of the linker's.
 *
 * The aliases below keep every use site reading as the plain array it is
 * about; they are the only reason this is not written out longhand.
 */
static struct { struct v34_object o; unsigned char g[GUARD]; } oa_blk;
static struct { unsigned char o[sizeof(struct v34_object)];
		unsigned char g[GUARD]; } ob_blk;
static struct { unsigned char o[SESSION_LEN]; unsigned char g[GUARD]; }
	sess_a_blk, sess_b_blk;
static struct { unsigned char o[SUB_LEN]; unsigned char g[GUARD]; }
	caps_a_blk, caps_b_blk, up_a_blk, up_b_blk, pcm_a_blk, pcm_b_blk;
static struct { short o[V34_INFO_MSG_SHORTS]; unsigned char g[GUARD]; }
	msg_a_blk, msg_b_blk;

#define oa		oa_blk.o
#define oa_guard	oa_blk.g
#define ob		ob_blk.o
#define ob_guard	ob_blk.g
#define sess_a		sess_a_blk.o
#define sess_a_guard	sess_a_blk.g
#define sess_b		sess_b_blk.o
#define sess_b_guard	sess_b_blk.g
#define caps_a		caps_a_blk.o
#define caps_a_guard	caps_a_blk.g
#define caps_b		caps_b_blk.o
#define caps_b_guard	caps_b_blk.g
#define up_a		up_a_blk.o
#define up_a_guard	up_a_blk.g
#define up_b		up_b_blk.o
#define up_b_guard	up_b_blk.g
#define pcm_a		pcm_a_blk.o
#define pcm_a_guard	pcm_a_blk.g
#define pcm_b		pcm_b_blk.o
#define pcm_b_guard	pcm_b_blk.g
#define msg_a		msg_a_blk.o
#define msg_a_guard	msg_a_blk.g
#define msg_b		msg_b_blk.o
#define msg_b_guard	msg_b_blk.g

/*
 * The probe source.  Read-only and therefore SHARED: one buffer means the
 * two sides cannot be handed different inputs, and it is sized to exactly
 * the last byte the copy reads so an over-run faults instead of quietly
 * agreeing on whatever follows (finding 129).
 */
#define PROBE_SRC_LEN \
	(V34_PROBE_OFFSET + (V34_PROBE_RESULTS - 1) * V34_PROBE_STRIDE \
	 + (int)sizeof(double))
static unsigned char probe_src[PROBE_SRC_LEN];

/* Object offsets of the two pointer fields these functions read. */
#define OFF_P3548	0x3548
#define OFF_PAC18	0xac18

/* Session offsets: the two pointers, and the int the codecs use. */
#define OFF_CAPS	0x612c
#define OFF_UPSTREAM	0x1760

static int
obj_skip(unsigned off)
{
	return (off >= OFF_P3548 && off < OFF_P3548 + 4)
	    || (off >= OFF_PAC18 && off < OFF_PAC18 + 4);
}

static int
sess_skip(unsigned off)
{
	return (off >= OFF_CAPS && off < OFF_CAPS + 4)
	    || (off >= OFF_UPSTREAM && off < OFF_UPSTREAM + 4);
}

static int saw_p3548, saw_pac18, saw_caps, saw_upstream;

static void
put_ptr(unsigned char *base, unsigned off, void *p)
{
	memcpy(base + off, &p, sizeof(p));
}

static void
setup(void)
{
	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
	memset(sess_a, 0x11, sizeof(sess_a));
	memset(sess_b, 0x11, sizeof(sess_b));
	memset(caps_a, 0x22, sizeof(caps_a));
	memset(caps_b, 0x22, sizeof(caps_b));
	memset(up_a, 0x33, sizeof(up_a));
	memset(up_b, 0x33, sizeof(up_b));
	memset(pcm_a, 0x44, sizeof(pcm_a));
	memset(pcm_b, 0x44, sizeof(pcm_b));
	memset(msg_a, 0x66, sizeof(msg_a));
	memset(msg_b, 0x66, sizeof(msg_b));

	memset(oa_guard, GUARD_FILL, GUARD);
	memset(ob_guard, GUARD_FILL, GUARD);
	memset(sess_a_guard, GUARD_FILL, GUARD);
	memset(sess_b_guard, GUARD_FILL, GUARD);
	memset(caps_a_guard, GUARD_FILL, GUARD);
	memset(caps_b_guard, GUARD_FILL, GUARD);
	memset(up_a_guard, GUARD_FILL, GUARD);
	memset(up_b_guard, GUARD_FILL, GUARD);
	memset(pcm_a_guard, GUARD_FILL, GUARD);
	memset(pcm_b_guard, GUARD_FILL, GUARD);
	memset(msg_a_guard, GUARD_FILL, GUARD);
	memset(msg_b_guard, GUARD_FILL, GUARD);

	put_ptr((unsigned char *)&oa, OFF_P3548, sess_a);
	put_ptr(ob, OFF_P3548, sess_b);
	put_ptr((unsigned char *)&oa, OFF_PAC18, pcm_a);
	put_ptr(ob, OFF_PAC18, pcm_b);
	put_ptr(sess_a, OFF_CAPS, caps_a);
	put_ptr(sess_b, OFF_CAPS, caps_b);
	put_ptr(sess_a, OFF_UPSTREAM, up_a);
	put_ptr(sess_b, OFF_UPSTREAM, up_b);
}

/* Compare one block, optionally with a skip predicate, and report offsets. */
static void
compare_block(const char *what, const void *pa, const void *pb, unsigned len,
	      int (*skip)(unsigned), long tag)
{
	const unsigned char *a = (const unsigned char *)pa;
	const unsigned char *b = (const unsigned char *)pb;
	unsigned i;
	int bad = 0;

	for (i = 0; i < len; i++) {
		if (a[i] == b[i] || (skip != NULL && skip(i)))
			continue;
		bad++;
		if (bad <= 6) {
			char m[160];

			snprintf(m, sizeof(m), "%s: byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(m, a[i], b[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);
}

static void
check_guards(long tag)
{
	static const unsigned char want[GUARD] = { 0 };
	unsigned char ref[GUARD];
	unsigned i;

	(void)want;
	memset(ref, GUARD_FILL, GUARD);

	diff_eq_int("object guard", memcmp(ob_guard, ref, GUARD) == 0, 1, tag);
	diff_eq_int("our object guard",
		    memcmp(oa_guard, ref, GUARD) == 0, 1, tag);
	diff_eq_int("session guard",
		    memcmp(sess_b_guard, ref, GUARD) == 0
		    && memcmp(sess_a_guard, ref, GUARD) == 0, 1, tag);
	diff_eq_int("caps guard",
		    memcmp(caps_b_guard, ref, GUARD) == 0
		    && memcmp(caps_a_guard, ref, GUARD) == 0, 1, tag);
	diff_eq_int("upstream guard",
		    memcmp(up_b_guard, ref, GUARD) == 0
		    && memcmp(up_a_guard, ref, GUARD) == 0, 1, tag);
	diff_eq_int("pcm guard",
		    memcmp(pcm_b_guard, ref, GUARD) == 0
		    && memcmp(pcm_a_guard, ref, GUARD) == 0, 1, tag);
	diff_eq_int("message guard",
		    memcmp(msg_b_guard, ref, GUARD) == 0
		    && memcmp(msg_a_guard, ref, GUARD) == 0, 1, tag);
	for (i = 0; i < GUARD; i++)
		if (ob_guard[i] != GUARD_FILL)
			diff_eq_int("object guard byte", ob_guard[i],
				    GUARD_FILL, (long)i);
}

/* Compare everything both sides own. */
static void
compare_all(const char *what, long tag)
{
	char m[96];

	snprintf(m, sizeof(m), "%s: object", what);
	compare_block(m, &oa, ob, sizeof(oa), obj_skip, tag);
	snprintf(m, sizeof(m), "%s: session", what);
	compare_block(m, sess_a, sess_b, SESSION_LEN, sess_skip, tag);
	snprintf(m, sizeof(m), "%s: caps", what);
	compare_block(m, caps_a, caps_b, SUB_LEN, NULL, tag);
	snprintf(m, sizeof(m), "%s: upstream", what);
	compare_block(m, up_a, up_b, SUB_LEN, NULL, tag);
	snprintf(m, sizeof(m), "%s: pcm", what);
	compare_block(m, pcm_a, pcm_b, SUB_LEN, NULL, tag);
	snprintf(m, sizeof(m), "%s: message", what);
	compare_block(m, msg_a, msg_b, sizeof(msg_a), NULL, tag);

	if (memcmp((unsigned char *)&oa + OFF_P3548, ob + OFF_P3548, 4) != 0)
		saw_p3548 = 1;
	if (memcmp((unsigned char *)&oa + OFF_PAC18, ob + OFF_PAC18, 4) != 0)
		saw_pac18 = 1;
	if (memcmp(sess_a + OFF_CAPS, sess_b + OFF_CAPS, 4) != 0)
		saw_caps = 1;
	if (memcmp(sess_a + OFF_UPSTREAM, sess_b + OFF_UPSTREAM, 4) != 0)
		saw_upstream = 1;

	check_guards(tag);
}

static void
poke_short(unsigned off, short v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_byte(unsigned off, unsigned char v)
{
	*((unsigned char *)&oa + off) = v;
	ob[off] = v;
}

static void
poke_sess_int(unsigned off, int v)
{
	memcpy(sess_a + off, &v, sizeof(v));
	memcpy(sess_b + off, &v, sizeof(v));
}

static void
poke_sub(unsigned char *a, unsigned char *b, unsigned off, unsigned char v)
{
	a[off] = v;
	b[off] = v;
}

static void
poke_sub_int(unsigned char *a, unsigned char *b, unsigned off, int v)
{
	memcpy(a + off, &v, sizeof(v));
	memcpy(b + off, &v, sizeof(v));
}

static void
set_msg(const short *src)
{
	int i;

	for (i = 0; i < V34_INFO_MSG_SHORTS; i++)
		msg_a[i] = msg_b[i] = src[i];
}

int
main(void)
{
	unsigned i;
	int rc = 0;
	int v90, k56, variant, ls, lv, cap;

	diff_begin("v34 info: the guard bands are where they claim to be");
	{
		/*
		 * The struct declarations above make this true by
		 * construction; asserted anyway, because the whole
		 * over-run check rests on it and a silent pass here would
		 * be indistinguishable from a silent pass everywhere else.
		 */
		diff_eq_int("object guard follows the object",
			    (int)((unsigned char *)ob_guard
				  - (unsigned char *)ob),
			    (int)sizeof(ob), 0);
		diff_eq_int("session guard follows the session",
			    (int)(sess_b_guard - sess_b), SESSION_LEN, 0);
		diff_eq_int("message guard follows the message",
			    (int)((unsigned char *)msg_b_guard
				  - (unsigned char *)msg_b),
			    (int)sizeof(msg_b), 0);
		diff_eq_int("caps guard follows the caps block",
			    (int)(caps_b_guard - caps_b), SUB_LEN, 0);
	}
	rc |= diff_end();

	diff_begin("v34 info: V34GiveProbeResults");
	{
		/*
		 * Distinct, exactly representable values so a copy that read
		 * the wrong stride lands on a number that identifies which
		 * record it came from.
		 */
		for (i = 0; i < V34_PROBE_RESULTS; i++) {
			double d = (double)i + 0.5;

			memcpy(probe_src + V34_PROBE_OFFSET
			       + i * V34_PROBE_STRIDE, &d, sizeof(d));
		}

		for (v90 = 0; v90 <= 1; v90++)
		for (k56 = 0; k56 <= 1; k56++) {
			long tag = v90 * 10 + k56;
			int got, want;

			setup();
			poke_int(0x24c, v90 ? 3 : 0);
			poke_int(0x250, k56 ? 5 : 0);
			got = V34GiveProbeResults(&oa, probe_src);
			want = ref_V34GiveProbeResults(ob, probe_src);
			diff_eq_int("GiveProbeResults return", got, want, tag);
			compare_all("GiveProbeResults", tag);
		}

		/* The copy must actually have moved the last record. */
		poke_int(0x24c, 3);
		V34GiveProbeResults(&oa, probe_src);
		diff_eq_int("the last probe result arrived",
			    oa.probe_results[V34_PROBE_RESULTS - 1]
			    == (double)(V34_PROBE_RESULTS - 1) + 0.5, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 info: V34SetINFO0dBits");
	{
		static const short m0[V34_INFO_MSG_SHORTS] = { 0 };

		for (v90 = 0; v90 <= 2; v90++) {
			setup();
			set_msg(m0);
			poke_int(0x24c, v90 == 2 ? -1 : v90);
			V34SetINFO0dBits(&oa, msg_a);
			ref_V34SetINFO0dBits(ob, msg_b);
			compare_all("SetINFO0dBits", v90);
		}
	}
	rc |= diff_end();

	diff_begin("v34 info: V34SetINFO0aBits");
	{
		static const short m0[V34_INFO_MSG_SHORTS] = { 0 };

		/*
		 * Every combination of the six flags that steer it.
		 *
		 * `local_v92` and the capability byte at +0x11 ARE SEPARATE
		 * DIMENSIONS, and that is the whole point of this sweep:
		 * the two branches read a different one of them for the same
		 * "indicating V.92 capabilities" decision, so a fixture that
		 * drove both from one variable would pass a reconstruction
		 * that read the wrong one.  It did, until this loop grew a
		 * sixth dimension.
		 */
		for (variant = 0; variant <= 1; variant++)
		for (v90 = 0; v90 <= 1; v90++)
		for (ls = 0; ls <= 1; ls++)
		for (lv = 0; lv <= 1; lv++)
		for (cap = 0; cap <= 1; cap++)
		for (k56 = 0; k56 <= 1; k56++) {
			long tag = (((((variant * 2 + v90) * 2 + ls) * 2 + lv)
				     * 2 + cap) * 2 + k56);

			setup();
			set_msg(m0);
			poke_sess_int(0x6120, variant);
			poke_int(0x24c, v90 ? 3 : 0);
			poke_int(0x250, k56 ? 5 : 0);
			poke_short(0xabca, (short)ls);
			poke_short(0xabc6, (short)lv);
			poke_sub(caps_a, caps_b, 0x11, (unsigned char)cap);
			poke_sub(up_a, up_b, 9, 0x15);
			poke_sub_int(up_a, up_b, 0x0c, 1);
			poke_sub_int(up_a, up_b, 0x00, 1);

			V34SetINFO0aBits(&oa, msg_a);
			ref_V34SetINFO0aBits(ob, msg_b);
			compare_all("SetINFO0aBits", tag);
		}

		/*
		 * And the rate field, which only the zero-variant branch
		 * builds: every value of the byte it bit-reverses, against
		 * both settings of the two ints that decorate it.
		 */
		for (i = 0; i < 256; i++) {
			int a, b;

			for (a = 0; a <= 2; a++)
			for (b = 0; b <= 2; b++) {
				setup();
				set_msg(m0);
				poke_sess_int(0x6120, 0);
				poke_int(0x24c, 3);
				poke_short(0xabca, 0);
				poke_short(0xabc6, 0);
				poke_sub(caps_a, caps_b, 0x11, 0);
				poke_sub(up_a, up_b, 9, (unsigned char)i);
				poke_sub_int(up_a, up_b, 0x0c, a);
				poke_sub_int(up_a, up_b, 0x00, b);

				V34SetINFO0aBits(&oa, msg_a);
				ref_V34SetINFO0aBits(ob, msg_b);
				compare_all("SetINFO0aBits rate",
					    1000 + (long)i * 10 + a * 3 + b);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 info: V34GiveINFO0dBits");
	{
		short m[V34_INFO_MSG_SHORTS];

		for (variant = 0; variant <= 1; variant++)
		for (v90 = 0; v90 <= 1; v90++)
		for (ls = 0; ls <= 1; ls++)
		for (lv = 0; lv <= 1; lv++)
		for (i = 0; i < 256; i++) {
			long tag = (((((long)variant * 2 + v90) * 2 + ls) * 2
				     + lv) * 1000) + i;
			int j;

			setup();
			for (j = 0; j < V34_INFO_MSG_SHORTS; j++)
				m[j] = (short)(i * 257 + j * 1013);
			/*
			 * Index 1 carries bit-vector entries 26 and 27, the
			 * two the variant swaps, so it gets the full sweep
			 * and the others get a value derived from it.
			 */
			m[1] = (short)i;
			set_msg(m);
			poke_sess_int(0x6120, variant);
			poke_int(0x24c, v90 ? 3 : 0);
			poke_short(0xabca, (short)ls);
			poke_short(0xabc6, (short)lv);

			V34GiveINFO0dBits(&oa, msg_a);
			ref_V34GiveINFO0dBits(ob, msg_b);
			compare_all("GiveINFO0dBits", tag);
		}
	}
	rc |= diff_end();

	diff_begin("v34 info: V34GiveINFO1aBits");
	{
		short m[V34_INFO_MSG_SHORTS];
		int saw_uinfo6 = 0;

		for (v90 = 0; v90 <= 1; v90++)
		for (k56 = 0; k56 <= 2; k56++)
		for (i = 0; i < 256; i++) {
			long tag = ((long)v90 * 3 + k56) * 1000 + i;
			int j, got, want;

			setup();
			for (j = 0; j < V34_INFO_MSG_SHORTS; j++)
				m[j] = (short)(i * 131 + j * 719);
			m[0] = (short)i;
			/*
			 * Index 2 supplies two of Uinfo's three bits and the
			 * baud index's low nibble; index 3 the third Uinfo
			 * bit and K56Flex's enable.  Both are driven from the
			 * same counter so the sweep reaches Uinfo 6.
			 */
			m[1] = (short)(i >> 5);
			m[2] = (short)(i & 0xf3);
			m[3] = (short)(((i & 1) << 7) | ((i & 2) << 2)
				       | (i & 2));
			set_msg(m);
			poke_int(0x24c, v90 ? 3 : 0);
			poke_int(0x250, k56);
			poke_sub_int(pcm_a, pcm_b, 0x0c, (int)i - 100);

			got = V34GiveINFO1aBits(&oa, msg_a);
			want = ref_V34GiveINFO1aBits(ob, msg_b);
			diff_eq_int("GiveINFO1aBits return", got, want, tag);
			compare_all("GiveINFO1aBits", tag);
			if (got == 1)
				saw_uinfo6 = 1;
		}

		/*
		 * Uinfo 6 is the only path that writes the capability block,
		 * so a sweep that never reached it would be testing half the
		 * function.
		 */
		diff_eq_int("the sweep reached Uinfo 6", saw_uinfo6, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 info: VPcmV34SetMohMessageBits");
	{
		/*
		 * The selector is swept past its bound in BOTH directions:
		 * the object's test is unsigned, so a negative value is out
		 * of range too, and the out-of-range case must leave the
		 * caller's buffer untouched rather than clearing it.
		 */
		static const int sel[] = { 0, 1, 2, 3, 4, 5, 6, 7, 100,
					   -1, -1000, 0x7fffffff };
		unsigned si, r, v;

		for (si = 0; si < sizeof(sel) / sizeof(sel[0]); si++)
		for (r = 0; r < 4; r++)
		for (v = 0; v < 4; v++) {
			static const unsigned char reasons[] = { 0, 1, 2,
								 0xff };
			static const short adds[] = { 0, 1, 0x0f,
						      (short)0xffff };
			long tag = (long)si * 100 + r * 10 + v;
			short m[V34_INFO_MSG_SHORTS];
			int j;

			setup();
			for (j = 0; j < V34_INFO_MSG_SHORTS; j++)
				m[j] = (short)(0x7000 + j);
			set_msg(m);
			poke_int(0xabf0, sel[si]);
			poke_byte(0xabfa, reasons[r]);
			poke_short(0xabe0, adds[v]);

			VPcmV34SetMohMessageBits(&oa, msg_a);
			ref_VPcmV34SetMohMessageBits(ob, msg_b);
			compare_all("SetMohMessageBits", tag);
		}
	}
	rc |= diff_end();

	diff_begin("v34 info: the same, with the debug sites live");
	{
		short m[V34_INFO_MSG_SHORTS];
		int j;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (variant = 0; variant <= 1; variant++)
		for (v90 = 0; v90 <= 1; v90++)
		for (ls = 0; ls <= 1; ls++)
		for (lv = 0; lv <= 1; lv++)
		for (k56 = 0; k56 <= 1; k56++) {
			long tag = ((((variant * 2 + v90) * 2 + ls) * 2 + lv)
				    * 2 + k56);

			for (j = 0; j < V34_INFO_MSG_SHORTS; j++)
				m[j] = (short)(0x1234 + j * 4919);
			m[3] = (short)(0x88 | (k56 ? 2 : 0));

			setup();
			set_msg(m);
			dsplib_debug_capture_reset();
			poke_sess_int(0x6120, variant);
			poke_int(0x24c, v90 ? 3 : 0);
			poke_int(0x250, k56 ? 5 : 0);
			poke_short(0xabca, (short)ls);
			poke_short(0xabc6, (short)lv);
			/* Deliberately the complement of local_v92. */
			poke_sub(caps_a, caps_b, 0x11, (unsigned char)!lv);
			poke_sub(up_a, up_b, 9, 0x15);
			poke_sub_int(up_a, up_b, 0x0c, 1);
			poke_sub_int(up_a, up_b, 0x00, 1);
			poke_sub_int(pcm_a, pcm_b, 0x0c, 1);

			V34SetINFO0aBits(&oa, msg_a);
			ref_V34SetINFO0aBits(ob, msg_b);
			diff_eq_int("SetINFO0aBits transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			dsplib_debug_capture_reset();
			V34SetINFO0dBits(&oa, msg_a);
			ref_V34SetINFO0dBits(ob, msg_b);
			diff_eq_int("SetINFO0dBits transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			set_msg(m);
			dsplib_debug_capture_reset();
			V34GiveINFO0dBits(&oa, msg_a);
			ref_V34GiveINFO0dBits(ob, msg_b);
			diff_eq_int("GiveINFO0dBits transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			dsplib_debug_capture_reset();
			V34GiveINFO1aBits(&oa, msg_a);
			ref_V34GiveINFO1aBits(ob, msg_b);
			diff_eq_int("GiveINFO1aBits transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);

			compare_all("debug pass", tag);
		}

		diff_eq_int("transcript non-empty",
			    dsplib_debug_capture_text(1)[0] != 0, 1, 0);
		diff_eq_int("ours printed too",
			    dsplib_debug_capture_text(0)[0] != 0, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("v34 info: every skipped pointer field was written");
	{
		diff_eq_int("object +0x3548 differed", saw_p3548, 1, 0);
		diff_eq_int("object +0xac18 differed", saw_pac18, 1, 0);
		diff_eq_int("session +0x612c differed", saw_caps, 1, 0);
		diff_eq_int("session +0x1760 differed", saw_upstream, 1, 0);
	}
	rc |= diff_end();

	return rc;
}
