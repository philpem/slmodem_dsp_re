/*
 * class1.h -- Class 1 fax: the session object, as far as this batch reads it.
 *
 * THE FAX PHASE IS DELIBERATELY LAST (see CLAUDE.md); what is here now is
 * the handful of `class1.c`-span symbols that nothing in the object reaches
 * -- exported API with no internal referrer, finding F8320's bucket --
 * written on their own merit.  So this header models ONLY what those leaves
 * touch, and the pad regions are expected to become fields when the fax
 * phase reads `fax_class1_create` (0x092e20) properly.
 *
 * What is already established about the object from this batch's reads:
 *
 *   +0x1204 / +0x1208   two handles handed to FAXVMI_status by
 *                       fax_class1_status -- receive-side for states 4..6,
 *                       transmit-side for 12..13 (which side is which is
 *                       NOT settled; the state numbers are)
 *   +0x121c             the session state machine's state number.  0x0f and
 *                       0x10 are the send/receive silence states (their
 *                       init functions store exactly those), 4..6 and
 *                       12..13 are the ranges fax_class1_status reports on
 *   +0x1228             a countdown the state inits arm: silence samples
 *                       over 2 for the silence states, 0 for tx-nulls and
 *                       the HDLC between-buffer state
 *   +0x1250             armed to 1 by _handle_hdlc_input_open, and read
 *                       back minus one into +0x000 by .._close
 *
 * `fax_class1_create` writes at least to +0x12dc and takes `&ctx->0x12e0`
 * as a sub-block, so the object extends past this struct; nothing here
 * allocates one, and CLASS1_MODELLED_BYTES is a modelling bound, not a size
 * claim.
 */

#ifndef DSPLIB_CLASS1_H
#define DSPLIB_CLASS1_H

struct fax_class1 {
	short f000;			/* +0x0000 <- f1250 - 1, by
					 * _handle_hdlc_input_close         */
	unsigned char pad_002[2];	/* +0x0002                          */
	unsigned char flags004;		/* +0x0004 bit 4 gates f1224        */
	unsigned char pad_005[0x11ff];	/* +0x0005                          */
	void *vmi_a;			/* +0x1204 FAXVMI handle, states
					 * 4..6 of fax_class1_status        */
	void *vmi_b;			/* +0x1208 FAXVMI handle, states
					 * 12..13                           */
	unsigned char pad_120c[0x10];	/* +0x120c                          */
	int state;			/* +0x121c the session state        */
	unsigned char pad_1220[4];	/* +0x1220                          */
	int f1224;			/* +0x1224 set 1 on hdlc close when
					 * flags004 bit 4                   */
	int countdown;			/* +0x1228 armed by the state inits */
	unsigned char pad_122c[0x24];	/* +0x122c                          */
	int f1250;			/* +0x1250 1 while hdlc input open  */
	unsigned char pad_1254[0x34];	/* +0x1254                          */
	void *f1288;			/* +0x1288 fax_class1_info(1) reads
					 * an unsigned short at +0xc of it  */
	unsigned char pad_128c[0x2c];	/* +0x128c                          */
	int f12b8;			/* +0x12b8 receive-silence extra    */
	short f12bc;			/* +0x12bc receive-silence extra    */
	unsigned char pad_12be[0x1a];	/* +0x12be                          */
	int f12d8;			/* +0x12d8 fax_class1_info(0);
					 * create clears it                 */
	unsigned char pad_12dc[4];	/* +0x12dc create: a .rodata ptr    */
};

#define CLASS1_MODELLED_BYTES	0x12e0

/* The two states the silence inits install. */
#define CLASS1_STATE_SEND_SILENCE	0x0f
#define CLASS1_STATE_RECV_SILENCE	0x10

/*
 * The state-init family: (object, an argument) -> int.  The silence pair's
 * argument is a sample count, halved into `countdown` with a floor of 1;
 * both leave that halved value in eax, which is declared here as the return
 * because the family returns int (_idle_state_init returns 0) -- no caller
 * is known to read it.
 */
int _send_silence_state_init(struct fax_class1 *ctx, int samples);
int _recieve_silence_state_init(struct fax_class1 *ctx, int samples);

/*
 * T.30 rate code -> (modulation, bit rate).  The four two-code ranges are
 * V.17 (with/without short training); the singles below them select V.29
 * (mod 1) and V.27ter (mod 0).  A code outside the table writes NOTHING --
 * both outputs keep their callers' values -- and the ranges are separate
 * IFs in the object, so 0x91/0x92 would fall through the later equality
 * tests unharmed either way.
 */
void _set_modem_rate(int code, int *mod, int *rate);

/* Bits per symbol for a bit rate; 0 for one not in the table. */
int _sym_size(int rate);

/*
 * info(ctx, 0, out): *out = f12d8.  info(ctx, 1, out): *out = the unsigned
 * short at f1288 + 0xc, or 0 with f1288 null.  Any other selector writes
 * nothing.  Returns 0, always.
 */
int fax_class1_info(struct fax_class1 *ctx, int sel, int *out);

/*
 * Returns 0, reads nothing.  ARITY NOT SETTLED by the object (`xor eax;
 * ret`); one pointer is declared as the least claim compatible with the
 * name.
 */
int fax_class1_GetConstalation(void *ctx);

#endif /* DSPLIB_CLASS1_H */
