/*
 * v8hs.c -- the V.8 interface.
 *
 * `V8Create` allocates the object and plants a handful of configuration
 * values in it; `V8Delete` frees it.  Both belong to the object's
 * `V8Interface.c`, which is why they are all that is left here once the
 * handshake machine moved to `V8.c`.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/v8.h"
#include "dsplib/sysdep.h"

/*
 * Build a handshake.
 *
 * Note what is NOT here: the object is allocated and never zeroed.  Only the
 * six configuration words below and whatever `v8handshakinit` writes are
 * defined when this returns, and the rest is whatever the allocator had.  A
 * caller that reads anything else is reading rubbish -- which is worth
 * knowing, because on a fresh page that rubbish is usually zero and so looks
 * deliberate.
 *
 * THE GUARD IS SINGLE-EXIT AND THAT IS READ OFF THE OBJECT, not a style
 * choice.  The blob's `je` lands on `add $0x34,%esp` with the `mov %esi,%eax`
 * BELOW it, so the failure path falls through the same return the success
 * path uses.  Written as an early `return`, GCC 3.4.2 has a second value to
 * materialise, spends `xor %eax,%eax` on it before the first `cfg` copy --
 * which costs `%eax` as a store base for two of them -- and has to jump PAST
 * the shared `mov`.  All three early-return spellings (`return 0`, `return v`,
 * `if (!v) return v`) compile to ONE emission, because the compiler knows `v`
 * is null on that arm and the returned expression is free; both single-exit
 * spellings compile to another.  A two-element domain, exhausted, and only
 * one element produces the object's control flow: 167 differing bytes to 25.
 *
 * The rate copy uses byte access to retain the dependency between its store
 * and the following CM pointer load.  A scalar int assignment lets GCC
 * 3.4.2 hoist that pointer load, changing 25 bytes; memcpy reproduces all
 * 1124 bytes.  The six field values and their source order are unchanged.
 * See F10218 for the finite domain and the compiler's alias-set evidence.
 */
struct v8 *
V8Create(const struct v8_cfg *cfg)
{
	struct v8 *v = sysdep_malloc(sizeof(struct v8));

	if (v != 0) {
		v->side = cfg->side;
		v->op_mode = cfg->op_mode;
		v->timeout_a = cfg->timeout_a;
		v->timeout_b = cfg->timeout_b;
		memcpy(&v->rate, &cfg->rate, sizeof(v->rate));
		v->cm = cfg->cm;

		/*
		 * The configuration trace: seventeen messages, each behind its own
		 * gate.  This is where the author dates the module (23/09/03) and
		 * names what the fields mean -- `side` and `op_mode` were called
		 * operation mode, `offered` the ansPcmLevel, `menu` the ucodeForQts,
		 * and the two CM extension fields are raw call-function and protocol
		 * octets.  See finding F164.
		 *
		 * Every message ends \r\n -- all of V8's diagnostics do.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: Create called, V8 version 23/09/03 .\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("#################################"
					     "###########################\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8: local configuration : \r\n");
		/*
		 * `== 0` with "Caller" first, and both halves of that are the
		 * object's.  The blob branches `je` where we branched `jne` --
		 * one byte, 0x74 against 0x75, same displacement, same target
		 * -- and its `.rodata.str1.1` holds "Caller" before "Answer"
		 * where ours held them the other way round.  GCC interns
		 * literals in source-text order, so the pool order is a second
		 * observable that agrees without reference to any instruction
		 * (lever 2).  Four spellings compiled; the two that put
		 * "Caller" first both reach it, so what is decoded is the arm
		 * ORDER and not `== 0` against `!v->side`.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tSide = %s\r\n",
					     v->side == 0 ? "Caller" : "Answer");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tOperation Mode = %d\r\n", v->op_mode);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "\tModulations - V90=%d, V34=%d, V34HD=%d, V32=%d, "
			    "V22=%d, V17=%d, V29=%d, V27=%d, V23=%d, V21=%d\r\n",
			    (v->cm->b0 >> 3) & 1, (v->cm->b0 >> 5) & 1,
			    (v->cm->b0 >> 6) & 1, v->cm->b0 >> 7,
			    v->cm->b1 & 1, (v->cm->b1 >> 1) & 1,
			    (v->cm->b1 >> 2) & 1, (v->cm->b1 >> 3) & 1,
			    (v->cm->b1 >> 4) & 1, (v->cm->b1 >> 5) & 1);

		/* The presence bits are tested outside the gates, not inside. */
		if (v->cm->b2 & V8_CM_EXT1_PRESENT) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "\tCall Functions - raw CF specified: cf[0]=%d , "
				    "cf[1]=%d , cf[2]=%d , cf[3]=%d\r\n",
				    v->cm->ext1[0], v->cm->ext1[1],
				    v->cm->ext1[2], v->cm->ext1[3]);
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
			    "\tCall Functions - Data=%d, CallRxFax=%d, CallTxFax=%d, "
			    "V.80=%d\r\n",
			    (v->cm->b1 >> 6) & 1, v->cm->b1 >> 7,
			    v->cm->b2 & 1, (v->cm->b2 >> 1) & 1);
		}

		if (v->cm->b2 & V8_CM_EXT2_PRESENT) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "\tProtocol - raw Protocol specified: prot[0]=%d "
				    ", prot[1]=%d , prot[2]=%d , prot[3]=%d\r\n",
				    v->cm->ext2[0], v->cm->ext2[1],
				    v->cm->ext2[2], v->cm->ext2[3]);
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf("\tProtocol - LAPM V.42\r\n");
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tv8bisIndication - %d\r\n",
					     (v->cm->b0 >> 1) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\ttimeouts - signal detect %d sec, "
					     "message detect %d sec\r\n",
					     v->timeout_a, v->timeout_b);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tquickConnectEnabled - %d\r\n",
					     (v->cm->b2 >> 4) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tlapmIndication - %d\r\n",
					     (v->cm->b2 >> 6) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tucodeForQts - %d\r\n", v->cm->menu);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tansPcmLevel - %d\r\n", v->cm->offered);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("#################################"
					     "###########################\r\n");

		v->tx_gain = 0x4000;
		v8handshakinit(v);

		v->pole_state = 0;
		v->prev_status = 0;
	}
	return v;
}

void
V8Delete(struct v8 *v)
{
	if (v != 0)
		sysdep_free(v);
}
