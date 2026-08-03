/*
 * v34digital.c -- ITU-T V.34/V.90: bringing the digital half up.
 *
 * `preinitdigital` clears and arms both shell contexts and both halves of
 * the scrambler pair.  It is what `v34modeminit` and `v34handshakinit` are
 * waiting on, and it took four scramblers and a table to become writable at
 * all -- `callgraph --ready` had it available from the start because it
 * models calls, and every one of those five is installed by ADDRESS.  See
 * finding 177.
 *
 * WHICH TRANSLATION UNIT, NOT SETTLED.  It sits at 0x59760 between `initV34`
 * (0x583f0) and the V.34 handshake's own file, and no local symbol anchors
 * anything in that span, so the extent cannot be recovered the way
 * `V34hshak.c`'s was.  Grouped here by what it does.
 *
 * THE TWO HALVES ARE ONE STRUCT TWICE.  Everything it touches below 0xe50 is
 * a `struct v34_shell` field, once at the receive context's base and once
 * 0x1be0 further on -- which is `V34_SHELL_TX`, already established by
 * finding 137 from `getFrame`'s offsets.  Two functions arriving at the same
 * spacing from opposite directions is the strongest evidence either has.
 *
 * AND IT NAMES THE SHELL'S BIT CALLBACKS.  The pointer at +0xe48 that
 * `getFrame` pulls bits through and `putFrame` pushes them into is filled
 * here with a SCRAMBLER in the transmit context and a DESCRAMBLER in the
 * receive one.  Finding 178.
 */

#include "dsplib/sysdep.h"
#include "dsplib/v34digital.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"

/*
 * Clear one shell context and install the two pointers both of them get.
 *
 * The object emits this twice, once per context, with identical constants;
 * the only thing that differs afterwards is the codec pointer, which the
 * caller overwrites.  Both copies install `scrambleGPC` here -- even the
 * receive one, which is then given a descrambler a few instructions later --
 * so the value written here is a placeholder in one of the two cases and
 * that is the object's doing, not a simplification.
 */
static void
preinit_shell(struct v34_shell *sh)
{
	int i;

	/*
	 * The demapper's three tables.  t1 and t2 are cleared and t3 is set
	 * to -1, which is the "no path" marker a cost table wants and zero
	 * would not be.  All three are 128 entries, which is what finding
	 * 129 had to infer from an adjacent field and this confirms.
	 */
	for (i = 0; i <= 0x7f; i++) {
		sh->t1[i] = 0;
		sh->t2[i] = 0;
		sh->t3[i] = -1;
	}

	/* decodeDepth's delay line, and the six beside it. */
	for (i = 0; i <= 5; i++) {
		sh->fa2c[i] = 0;
		sh->hist[i] = 0;
	}

	sh->fa16 = 0x18;
	sh->scramble = scrambleGPC;
	sh->convolve = Convolve16;
	sh->fa3c = 0;
	sh->prev_k = 0;
	sh->latched = 0;
	sh->fa08 = 0;
}

/*
 * Bring the digital half up.
 *
 * Everything here is a clear except the last four stores, and those are the
 * only thing in the function that depends on anything: `f359c` picks which
 * polynomial goes which way.
 */
void
preinitdigital(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_shell *rx = (struct v34_shell *)obj;
	struct v34_shell *tx = (struct v34_shell *)((char *)obj
						    + V34_SHELL_TX);

	preinit_shell(tx);
	preinit_shell(rx);

	/* The receive context's trellis machinery, cleared wholesale. */
	sysdep_memset(rx->cost, 0, sizeof(rx->cost));
	sysdep_memset(rx->trellis, 0, sizeof(rx->trellis));
	sysdep_memset(rx->state, 0, sizeof(rx->state));

	obj->scrambler.w[0] = 0;
	obj->scrambler.w[1] = 0;
	obj->scrambler.w[2] = 0;
	obj->scrambler.w[3] = 0;
	obj->scrambler.nbits = 0x20;

	obj->descrambler.w[0] = 0;
	obj->descrambler.w[1] = 0;
	obj->descrambler.w[2] = 0;
	obj->descrambler.count = 0;

	rx->state_idx = 0;
	obj->scram_capture = 0;
	obj->faa74 = 0;

	/*
	 * THE ONE DECISION IN THE FUNCTION.  The two ends of a V.34 call
	 * must scramble with opposite polynomials, so this is the
	 * originate/answer flag -- and `setTimingStateParameters` already
	 * reads the same field to pick between two timing ramps, which is
	 * the second use that makes the first one legible.  Finding 177.
	 */
	if (obj->f359c == 0x65) {
		tx->scramble = scrambleGPC;
		rx->scramble = (v34_scramble_fn)descrambleGPA;
	} else {
		tx->scramble = scrambleGPA;
		rx->scramble = (v34_scramble_fn)descrambleGPC;
	}
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  The two contexts' spacing is the load-bearing one.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34DIG_ASSERT(name, type, field, off) \
	typedef char v34dig_off_##name[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V34DIG_ASSERT(fa08,  struct v34_shell,  fa08,       0x0a08);
V34DIG_ASSERT(fa16,  struct v34_shell,  fa16,       0x0a16);
V34DIG_ASSERT(hist,  struct v34_shell,  hist,       0x0a18);
V34DIG_ASSERT(conv,  struct v34_shell,  convolve,   0x0a28);
V34DIG_ASSERT(fa2c,  struct v34_shell,  fa2c,       0x0a2c);
V34DIG_ASSERT(prevk, struct v34_shell,  prev_k,     0x0a38);
V34DIG_ASSERT(fa3c,  struct v34_shell,  fa3c,       0x0a3c);
V34DIG_ASSERT(t1,    struct v34_shell,  t1,         0x0a48);
V34DIG_ASSERT(t3,    struct v34_shell,  t3,         0x0c48);
V34DIG_ASSERT(latch, struct v34_shell,  latched,    0x0e4c);
V34DIG_ASSERT(cost,  struct v34_shell,  cost,       0x0eac);
V34DIG_ASSERT(trel,  struct v34_shell,  trellis,    0x0ecc);
V34DIG_ASSERT(state, struct v34_shell,  state,      0x12cc);
V34DIG_ASSERT(sidx,  struct v34_shell,  state_idx,  0x144c);

/*
 * The transmit context is exactly V34_SHELL_TX past the receive one, which
 * `getFrame` established (finding 137) and this function arrives at
 * independently: its second block is at obj+0x25e0 and its first at
 * obj+0xa00.
 */
typedef char v34dig_tx_spacing[((0x25e0 - 0xa00) == V34_SHELL_TX) ? 1 : -1];

/* And the three memsets must be exactly the arrays, not approximately. */
typedef char v34dig_memsets[
	(sizeof(((struct v34_shell *)0)->cost) == 0x20
	 && sizeof(((struct v34_shell *)0)->trellis) == 0x400
	 && sizeof(((struct v34_shell *)0)->state) == 0x180) ? 1 : -1];

#endif
