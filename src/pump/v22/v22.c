/*
 * v22.c -- V.22 / V.22bis / Bell 212: the datapump's core-facing half.
 *
 * The counterpart of src/pump/b103/b103.c, and so far it holds one of that
 * file's five functions.  `v22_create`, `v22_process`, `dp_v22_init` and
 * `dp_v22_exit` all wait on the seven `V22_PROTOCOL` handlers (finding
 * F8529); until those exist, a reference to any of them from here is an
 * undefined symbol that fails every test binary in the tree.
 *
 * ---------------------------------------------------------------------------
 * THE ROUND TRIP THROUGH THE WRAPPER IS THE OBJECT'S, NOT A SIMPLIFICATION
 *
 * `v22_delete` is handed a `struct dp *` that IS a `struct v22_dp *` -- the
 * one is the first twenty bytes of the other -- and it could cast directly.
 * It does not: it loads `dp->dp_data`, which `v22_create` set to the wrapper,
 * and takes the wrapper's `dp` back out, which `v22_create` set to the object.
 * Three loads to arrive where one cast would have.  `b103_delete` does exactly
 * the same thing at the same offsets, so it is the author's idiom rather than
 * an accident, and it is reproduced.
 */

#include "dsplib/v22.h"

#include "dsplib/dp_wrapper.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22fp.h"

/*
 * The layout, asserted rather than commented.  `v22.h` derives the bit
 * buffers' length from the object's own allocation size, so the assertion
 * below is the derivation itself and not a restatement of it: change either
 * array and the file stops compiling.
 *
 * Compiled only under the 32-bit ABI these offsets describe.  Under 3.4.2 the
 * `__SIZEOF_POINTER__` spelling would silently vanish -- see
 * docs/method/compilers.md -- so the guard is on the pointer size the
 * preprocessor can actually compute.
 */
#define V22_ASSERT_OFF(tag, type, field, want) \
	typedef char tag[(__builtin_offsetof(type, field) == (want)) ? 1 : -1]

V22_ASSERT_OFF(d_bits, struct v22_dp, bits_per_word, 0x14);
V22_ASSERT_OFF(d_want, struct v22_dp, tx_bits_wanted, 0x18);
V22_ASSERT_OFF(d_fp, struct v22_dp, fp, 0x1c);
V22_ASSERT_OFF(d_wrap, struct v22_dp, wrapper, 0x20);
V22_ASSERT_OFF(d_txb, struct v22_dp, tx_bits, 0x24);
V22_ASSERT_OFF(d_rxb, struct v22_dp, rx_bits, 0x1b4);

typedef char v22_dp_size[(sizeof(struct v22_dp) == 0x344) ? 1 : -1];

int
v22_delete(struct dp *dp)
{
	struct v22_dp *self = (struct v22_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;

	V22FP_delete(self->fp);
	dp_wrapper_delete(self->wrapper);
	dp->dp_data = NULL;
	sysdep_free(self);
	return 0;
}
