/*
 * v22ctl.h -- V.22 / V.22bis: the datapump object's four exported accessors.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V22FP_GetDiagnostics  .text 0x088480   21 bytes
 *   V22FP_control         .text 0x08c3b0  145 bytes
 *   ScramblerOn           .text 0x08e670   11 bytes
 *   DescramblerOn         .text 0x08e680   11 bytes
 *
 * NONE OF THE FOUR HAS A CALLER IN THE OBJECT.  `objdump -dr` over the whole
 * 1.2 MB finds no relocation against any of them, so they are exported
 * surface: the host, or a part of slmodemd this library does not contain,
 * reaches them.  That has two consequences worth stating rather than
 * discovering later.  First, nothing corroborates the argument types from the
 * outside, so `V22FP_control`'s second parameter is typed from its own two
 * loads and no more.  Second, the two one-line readers below are the only
 * evidence in the whole object for what two of the datapump's flag words MEAN
 * -- see the note on that.
 *
 * ---------------------------------------------------------------------------
 * WHAT `ScramblerOn` AND `DescramblerOn` SETTLE, AND WHAT THIS FILE DOES NOT
 * DO ABOUT IT
 *
 * `ScramblerOn` returns `dsp->scrambler_on` and `DescramblerOn` returns `dsp->descrambler_on`,
 * and `V22FP_control` sets exactly those two from bits 0 and 1 of its control
 * byte.  Two accessors whose names say what they answer, reading two fields
 * that a third function sets from two adjacent bits, is the "a caller or
 * callee that types it" rule at its strongest: `struct v22fp_dsp`'s `r18` is
 * the transmit scrambler's enable and `r1c` is the receive descrambler's.
 *
 * THEY ARE NOT RENAMED HERE, and that is deliberate rather than an oversight:
 * two other reconstructions are in flight against this same base commit and
 * both name those fields, so the rename is a merge conflict waiting to
 * happen.  The evidence is recorded; the edit belongs to whoever lands last.
 *
 * `V22FP_create` derives both from `params.flags` bits 0 and 1 (v22fp.h), so
 * the control block's bits 0 and 1 are the same two switches the constructor
 * takes -- which is what says this function is a RECONFIGURE and not a
 * separate mechanism.
 */

#ifndef DSPLIB_V22CTL_H
#define DSPLIB_V22CTL_H

struct v22fp;

/*
 * The control block `V22FP_control` is handed.  TWELVE BYTES OF IT ARE
 * UNMODELLED: the function reads +0x0c and +0x0d and nothing else, so the
 * size, the shape and even whether the caller's object ends at +0x0e are all
 * unknown.  What is here is what the two loads force.
 */
struct v22fp_ctl {
	unsigned char unmapped_0000[0x0c];
	unsigned char flags_0c;		/* +0x0c, read with movzbl */
	unsigned char flags_0d;		/* +0x0d, read with movzbl */
};

/*
 * `flags_0c`.  Four single bits, each landing on a field the constructor also
 * writes, plus one that goes back into the parameter block.
 */
#define V22_CTL_SCRAMBLER	(1 << 0)	/* -> dsp->scrambler_on, ScramblerOn   */
#define V22_CTL_DESCRAMBLER	(1 << 1)	/* -> dsp->descrambler_on, DescramblerOn */
#define V22_CTL_EQ_THIRD	(1 << 2)	/* -> dsp->r20                */
/*
 * INVERTED: `dsp->agc.f18` is set to 1 when this bit is CLEAR.  `fpm_agc.h`
 * has f18 as the flag `FPM_AGC_init` sets and `FPM_AGC_agc` never touches,
 * and `DemodDataV22` ANDs it into both the clock loop's and the equaliser's
 * adaptation enables -- so clearing this bit is what lets the receiver adapt.
 * The name states the bit's SENSE, which is the part the object forces; what
 * the adaptation is for is the callees' business.
 */
#define V22_CTL_FREEZE_ADAPT	(1 << 3)	/* -> dsp->agc.f18 = !bit     */
/*
 * Copied straight into `params.flags` bit 9, which `V22FP_create` patches
 * from `struct v22fp_cfg::f18` and which nothing reconstructed reads.  Named
 * for where it goes, because that is all there is.
 */
#define V22_CTL_PARAM_BIT9	(1 << 7)
#define V22_PARAMS_FLAG_BIT9	(1u << 9)

/*
 * `flags_0d` carries one flag and one two-bit field, and they are checked in
 * that order.  Both write the same PAIR of half-duplex words -- `hdx->protocol`
 * and `hdx->connect_substate` -- so the second overwrites the first when both apply.
 *
 * The two-bit field is bits 7:6 and only the value 2 does anything; the
 * object shifts the whole byte right by six and compares, so bit 7 set with
 * bit 6 clear is the case that fires.  A width and a shift rather than a flag
 * name, because it is not a single bit.
 *
 * WHAT THE TWO VALUES SELECT IS SETTLED, and not by this function.  `hdx->protocol`
 * is the index `V22FP_modem` uses into `V22_PROTOCOL`, a seven-entry table of
 * the seven V.22 protocol handlers; the relocations name them, so 6 is
 * `v22_retrain` and 4 is `v22_org_rmloop2` (finding F8529).  So this byte's
 * bit 2 requests a RETRAIN and its two-bit field selects the ORIGINATE
 * remote-loopback-2 state.
 */
#define V22_CTL_RETRAIN		(1 << 2)	/* r0e = 6, r0c = 1 */
#define V22_CTL_HDX_SHIFT	6
#define V22_CTL_HDX_ORG_RMLOOP2	2		/* r0e = 4, r0c = 0 */

/*
 * The values that pair of writes installs: two of `V22_PROTOCOL`'s seven
 * indices.  See v22status.h for the whole table.
 */
#define V22_PROTOCOL_RETRAIN		6
#define V22_PROTOCOL_ORG_RMLOOP2	4

/*
 * The equaliser's diagnostic word.
 *
 * The object TAIL-JUMPS to `V22_FSE_getdiag` after rewriting only the first
 * argument slot, so anything else its caller passed is still on the stack and
 * this function cannot say how many arguments it was given.  One is declared,
 * for the same reason v22_fse.h gives for declaring one on the callee: under
 * cdecl a caller passing more is harmless.
 */
int V22FP_GetDiagnostics(struct v22fp *fp);

/*
 * Reconfigure a live datapump from a control block.  Always returns 1 -- the
 * value is a literal on every path and is not a status.
 */
int V22FP_control(struct v22fp *fp, const struct v22fp_ctl *ctl);

/* Is the transmit scrambler enabled?  Is the receive descrambler? */
int ScramblerOn(struct v22fp *fp);
int DescramblerOn(struct v22fp *fp);

#endif /* DSPLIB_V22CTL_H */
