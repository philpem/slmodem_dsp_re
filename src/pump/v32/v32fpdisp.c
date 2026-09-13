/*
 * v32fpdisp.c -- ITU-T V.32 / V.32bis: the FP layer's dispatch surface.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32FP_create    .text 0x07f710   169   build the object from a config
 *   V32FP_modem     .text 0x082630   356   one block, through V32_PROTOCOL
 *   v32_data        .text 0x0827a0   859   the data-mode protocol handler
 *   V32FP_control   .text 0x084530   776   apply a control request
 *   V32FP_status    .text 0x084840  1084   report, and post a request back
 *   V32_PROTOCOL    .data  0x007700    36   nine handlers, GLOBAL
 *   PROTOCOL        .data  0x007768    18   nine codes, file-local
 *
 * `v32fpctl.h` is the twenty-one small setters the HANDSHAKE calls; this file
 * is what the datapump GLUE calls, and `v32fpstat.h` declares it.
 *
 * WHY THESE ARE IN ONE FILE AND NOT IN THEIR SPANS.  `make worklist` puts
 * `V32FP_create` in `Dialer.c +18` beside `V32FP_recreate` and the other four
 * in `V32mod.c +39`, so the object has them in two translation units and this
 * file has them in one.  The five are in the object's own ADDRESS ORDER
 * within the file, which is what lever 1 of `docs/method/refinement.md` asks
 * for.  That is a
 * FACTORING difference, of the kind finding F605 warns makes a per-function
 * count measure our layout rather than our completeness; it is taken
 * deliberately because `V32FP_create` is 169 bytes whose only job is to build
 * `V32FP_recreate`'s argument, and splitting it into a file of its own would
 * buy nothing. `v32fpctl.c`'s own banner already records that its span is not
 * a settled translation-unit boundary.
 *
 * ---------------------------------------------------------------------------
 * THE CONTROL REQUEST IS CONSUMED, NOT READ
 *
 * `V32FP_control` CLEARS its own bit in the caller's block on each of the two
 * arms it can take -- `andb $0xfb` at 84669 and `andb $0xf7` at 84700 -- and
 * it SETS one, at 8472e and 84743.  So the second argument is in/out and the
 * caller's copy is where the state lives between calls; `v32fpstat.h` and
 * finding F8642 have the bit table.
 *
 * ---------------------------------------------------------------------------
 * THE `Patch:` LINE IS THE AUTHOR'S OWN WORKAROUND FOR A BUG IN `v32_data`
 *
 * The function opens by testing `V32_OBJ_STATUS` against 9 and, if it matches,
 * printing
 *
 *     Patch: set ctl_ptr->vxx_ctl.options.retrain = TRUE
 *
 * and setting bit 2 of the request's second byte -- the retrain bit, named by
 * that string and by nothing else.  Status 9 is what `v32_data` writes when
 * `RetrainDetectV32` fires, and `v32_data` builds its request in a STACK
 * LOCAL and defers the call to the next block through `Control_Flag`, so the
 * request it built is gone by the time this function sees it.  See
 * `docs/deviations.md` and finding F8645: this is the author patching the one
 * bit that mattered back in from the status code, rather than fixing the
 * lifetime.
 *
 * ---------------------------------------------------------------------------
 * THE RATE-FALLBACK TABLE IS A LADDER STEP, AND IT CORROBORATES v32seq.h
 *
 * The retrain arm, when `fp + 0x50d8` is set, switches on the receive rate
 * INDEX at `V32FP_SHORT_2E` through a six-entry jump table at `.rodata +
 * 0x7f40` and writes a new line rate into the object:
 *
 *     index 5 (14400) -> 12000        index 3 (7200)  -> 4800
 *     index 4 (12000) ->  9600        index 0 (4800)  -> nothing
 *     index 2 ( 9600) ->  7200
 *     index 1 ( 9600) ->  7200
 *
 * -- which is `v32seq.h`'s ladder stepped down exactly one rung at every
 * index, with the bottom rung having nowhere to go.  That is an independent
 * confirmation of the whole index -> rate correspondence from a table that
 * never mentions a rate index, index 0 included.  Finding F8647.
 */

#include "dsplib/debug.h"
#include "dsplib/v32.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32cfg.h"
#include "dsplib/v32fp.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32fpstat.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32hdxst.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"
#include "dsplib/fpm.h"

/*
 * THE TWO RATE-INDEXED TABLES ARE FILE-LOCAL IN THE OBJECT and this file is
 * their only consumer -- `V32FP_create` reads `RATEv32` and `V32FP_status`
 * reads `SnrToRetrainTable` -- so they are `static` here, which is what the
 * reference's LOCAL `d` records say.  They moved out of `v32fptab.c`, which
 * used to hold them with external linkage so a test could name them; the test
 * tier's globalized copies (tools/testvisible.py) are what replaces that now.
 * Both are `.data` and not `const`, even though nothing writes either.
 */
static short SnrToRetrainTable[6] = {
	9,			/* 0   4800                                  */
	13,			/* 1   9600, no trellis                      */
	13,			/* 2   9600, trellis                         */
	11,			/* 3   7200                                  */
	20,			/* 4  12000                                  */
	24			/* 5  14400                                  */
};

static short RATEv32[6] = {
	4800,			/* 0                                         */
	9600,			/* 1   no trellis                            */
	9600,			/* 2   trellis                               */
	7200,			/* 3                                         */
	12000,			/* 4                                         */
	14400			/* 5                                         */
};

/* The instance is not modelled; see v32fpctl.h.  These are its accessors. */
#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_U16(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_U8(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

#define HDX(m)			FIELD_PTR((m), V32_OBJ_HDX)
#define FP(m)			FIELD_PTR((m), V32_OBJ_FP)

/*
 * The parameter block IS the object's first 48 bytes -- v32fp.h derives that
 * from the three `rep movsl` sites -- so this cast is not a reinterpretation.
 */
#define PARAMS(m)		((struct v32fp_params *)(void *)(m))

/*
 * The six `int` switches `struct v32fp_ctl::ctl0` drives, at fp + 0x00 ..
 * fp + 0x24.  v32fpctl.h already names four of the ten; these are the other
 * three this file writes, plus the two `V32FP_status` reports through.  What
 * they SWITCH is not established by anything reconstructed.
 */
#define V32FP_R1C		0x1c
#define V32FP_R20		0x20
#define V32FP_R24		0x24

/*
 * The negotiated rate indices.  v32seq.h's own note pins the pair:
 * "0x7f40b stores to fp + 0x28, 0x7f60f to fp + 0x2a", the two arms of
 * `V32FP_recreate`'s 9600 split.  They are the rate the handshake is moving
 * TO; `V32FP_SHORT_2C` and `V32FP_SHORT_2E` are the rate in force.
 */
#define V32FP_TX_RATE_IDX	0x28
#define V32FP_RX_RATE_IDX	0x2a

/*
 * fp + 0x50d8.  Non-zero selects the rate-fallback ladder on the retrain arm
 * and is cleared by nothing this file writes; `V32FP_status` sets it to 1
 * when eight consecutive SNR drops have been seen, so it reads as "the last
 * retrain was asked for because the line got worse".  Usage inference, and
 * the weakest thing named here.
 */
#define V32FP_RATE_FALLBACK	0x50d8

/*
 * hdx + 0x7c.  Set to 0x960 -- 2400 -- on the originate retrain arm and
 * nowhere else that is written.  Modelled, unnamed.
 */
#define V32HDX_LONG_7C		0x7c
/* hdx + 0x44, zeroed on the ring-response arm.  Modelled, unnamed. */
#define V32HDX_SHORT_44		0x44
/* hdx + 0x84, the transmit block charge; V32TXHDX.c names it. */
#define V32HDX_BLOCK_CHARGE	0x84

/*
 * `V32_OBJ_STATUS` values this file tests or writes.  Only the first is named
 * by the object -- `V32_MSG_NO_CARRIER` is a `.rodata` string `v32_data`
 * prints beside the code 12 -- and the retrain pair below are named for the
 * arm they select, which is usage inference stated as such.
 */
#define V32_MSG_RETRAIN_REQ	0x09	/* v32_data posts it; this file acts   */
#define V32_MSG_RETRAINING	0x0b	/* written on the way into a retrain   */
#define V32_MSG_RESIZED		0x0e	/* the length-change arm               */
#define V32_MSG_RESIZE_DONE	0x0f	/* already reported; do not re-post    */
#define V32_MSG_OK		0x01

/*
 * The datapump-block fields `V32FP_status` reads.  One is named by the author
 * and the rest are not.
 *
 * fp + 0x50d6 is the smoothed DECISION ERROR: the debug line beside it is
 * `Dec error = %d (*64)`, which is a format string and so the strongest
 * evidence class there is.  Everything the function calls an SNR is derived
 * from it by `FPM_div` and `FPM_log10`.
 */
#define V32FP_DEC_ERROR		0x50d6
#define V32FP_SHORT_256		0x256	/* the IIR's input, taken >> 2       */
#define V32FP_SHORT_100		0x100
#define V32FP_SHORT_102		0x102
#define V32FP_SHORT_6C		0x6c
#define V32FP_SHORT_182		0x182
#define V32FP_SHORT_1F8		0x1f8
#define V32FP_INT_188		0x188
#define V32FP_INT_1F4		0x1f4

/*
 * obj + 0x20 counts CONSECUTIVE SNR drops and the ninth starts a retrain --
 * `%d coseq SNR drops detected local retrain is initiated`, printed with the
 * literal 8, is the author's own description of it.
 */
#define V32_OBJ_SNR_DROPS	0x20
/*
 * obj + 0x2c.  While it is at or below 199 the reported SNR is forced to 40
 * and the counter advances, so it is a settling period of two hundred blocks
 * -- one second at `v32.c`'s 5 ms a block.  Modelled, unnamed: nothing says
 * what it settles.
 */
#define V32_OBJ_SHORT_2C	0x2c
#define V32_SNR_SETTLE_BLOCKS	0xc7
#define V32_SNR_SETTLED_VALUE	40
#define V32_SNR_FLOOR		(-60)
#define V32_SNR_DROPS_LIMIT	8

/*
 * obj + 0x11 -- the SECOND BYTE OF `params.options`, and not `V32_OBJ_FLAGS`
 * at 0x31, which is 0x20 above it and is easy to read it as.  Three sites
 * reach it as a byte: `V32FP_create` patches bit 2 (options bit 10) from the
 * caller's configuration, `V32FP_control` writes bit 1 (options bit 9) from
 * `ctl0`'s bit 7, and `V32FP_status` reports both back -- bit 1 as
 * `struct v32_status::flags` bit 7 and bit 2 as `flags1` bit 0.  So the two
 * are a round trip through the control block, which is what says they are the
 * same two bits and not four.
 */
#define V32_OBJ_OPTIONS_HI	0x11
#define V32_OPTIONS_HI_CTL	0x02	/* options bit  9 */
#define V32_OPTIONS_HI_CFG	0x04	/* options bit 10 */

/* obj + 0x31 bit 5, set on the way into a retrain. */
#define V32_FLAG_RETRAIN	0x20
/*
 * obj + 0x31 bit 0.  `V32FP_modem` reads it as "hand the machine to the
 * data-mode handler" -- it parks the state machine at `V32_STATE_DONT_CARE`
 * and points `V32HDX_MODE` at `V32_PROTOCOL`'s data slot -- and `v32_data`
 * clears it when a retrain is wanted.  Usage inference from those two sites.
 */
#define V32_FLAG_DATA		0x01

/* `V32HDX_MODE`'s data-mode slot; V32_MODE_* 0..5 are v32hdxst.h's. */
#define V32_PROTO_DATA		6

/*
 * hdx + 0xae, blocks since the retrain flag was last seen set.  `v32_data`
 * multiplies it by five and the author's own format string calls the product
 * `carrier_loss_time %d ... ms`, so the field is a BLOCK COUNT and five
 * milliseconds is one block.  Finding F8648.
 */
#define V32HDX_LOSS_BLOCKS	0xae
#define V32_BLOCK_MS		5

/*
 * `struct v32_status::flags`, as `v32_data` USES it.  Every one of these four
 * is named for the call it gates and nothing stronger -- the bits themselves
 * come back out of `V32FP_status` from six `int` switches in the datapump
 * block that nothing reconstructed names.  Usage inference, said plainly.
 */
#define V32_STFLAG_SCRAMBLE	0x01	/* ScrambleDataV32 on the tx buffer  */
#define V32_STFLAG_DESCRAMBLE	0x02	/* DescrambleDataV32 on the rx buffer */
#define V32_STFLAG_TXCLOCK	0x04	/* TxClockSyncV32                    */
#define V32_STFLAG_RETRAIN_DET	0x80	/* poll RetrainDetectV32             */

/*
 * Two more `V32_OBJ_STATUS` codes, from `v32_data`.  12 is the author's:
 * the string it prints beside the code is literally `V32_MSG_NO_CARRIER`.
 * 28 is named for the arm that posts it and is inference.
 */
#define V32_MSG_NO_CARRIER	0x0c
#define V32_MSG_RENEG		0x1c

/* ------------------------------------------------------------------------ */

/*
 * V32FP_create -- .text 0x07f710.
 *
 * Copy the template, patch six fields, hand it over.  The object is not
 * allocated here: `V32FP_recreate` is called with a NULL object, which is
 * what makes it allocate one, and its return is this function's.
 *
 * `trellis` is set from a comparison and not copied: `cfg->rate > 0x1c1f` is
 * "faster than 7200", made with `seta`, so it is an UNSIGNED comparison on
 * the 16-bit field and every rate above 7200 selects the trellis variant.
 */
void *
V32FP_create(const struct v32fp_cfg *cfg, void *arg1)
{
	struct v32fp_params params;

	params = V32_CFG;

	params.protocol = (short)(cfg->protocol != 0);
	params.tx_rate = (short)cfg->rate;
	params.rx_rate = (short)cfg->rate;
	params.timeout = cfg->timeout;
	params.options = (params.options & ~0x400u)
		| (unsigned int)((cfg->r10 & 1) << 10);
	params.ec_near_delay = (unsigned short)cfg->phys_delay;
	params.trellis = (cfg->rate > 0x1c1f);
	params.energy_drop_time = (short)cfg->energy_drop_time;

	return V32FP_recreate(0, &params, arg1);
}


/* ------------------------------------------------------------------------ */

/*
 * The two `.bss` scratch buffers `V32FP_modem` copies through --
 * `tx_in_internal` at 0x1c0 and `rx_out_internal` at 0x2a0.  A HUNDRED SHORTS
 * EACH, which is `nm`'s 0xc8 bytes, and they are STATICS AND NOT FIELDS, so
 * two V.32 datapumps in one process would share them.  The names are the
 * object's own; V.22 and V.23 each have a pair of the same two names, which
 * is why `symmap.py` leaves all six local and no `ref_` alias exists for any.
 *
 * They are why `v32.c` clamps its receive count to a hundred: the protocol
 * handler writes into `rx_out_internal` with no bound of its own.
 */
static unsigned short tx_in_internal[V32_BIT_BUFFER];
static unsigned short rx_out_internal[V32_BIT_BUFFER];

/*
 * V32FP_modem -- .text 0x082630.
 *
 * One block.  The two counts are in/out and they CROSS OVER: `nout` arrives as
 * a count of transmit BITS and leaves as a count of output SAMPLES, `nin`
 * arrives as input samples and leaves as receive bits.  Everything around the
 * dispatch is marshalling -- the caller's `int` arrays are narrowed into the
 * two `short` statics and widened back.
 *
 * THE RETURN IS A 32-BIT LOAD OF obj + 0x30 (82779), where every other access
 * to that byte in this tree is `movb` or `movzbl`.  Reproduced as a 32-bit
 * read: the status is the low byte and `v32.c` masks it, so `V32_OBJ_FLAGS`
 * and the two bytes above it reach the caller and are discarded.
 */
int
V32FP_modem(void *modem, const int *txbits, short *out, const short *in,
	    int *rxbits, int *nout, int *nin)
{
	void *fp;
	void *hdx;
	short nsamples;
	unsigned short rxcount;
	int scale;
	int n;
	int i;

	nsamples = (short)*nout;
	rxcount = (unsigned short)*nin;

	fp = FP(modem);
	if (*nin > 0) {
		short *clean = (short *)FIELD_PTR(fp, V32FP_CLEAN_BUF);

		for (i = 0; i < *nin; i++)
			clean[i] = in[i];
	}
	FIELD_U16(fp, V32FP_CLEAN_N) = (unsigned short)*nin;

	for (i = 0; i < *nout; i++)
		tx_in_internal[i] = (unsigned short)txbits[i];

	FIELD_U8(modem, V32_OBJ_FLAGS) &= (unsigned char)~V32_FLAG_FAULT;
	hdx = HDX(modem);
	V32_PROTOCOL[FIELD_S16(hdx, V32HDX_MODE)](modem, tx_in_internal, out,
						  (short *)in, rx_out_internal,
						  &nsamples, &rxcount);

	*nout = (int)(unsigned short)nsamples;
	*nin = (int)rxcount;

	for (i = 0; i < *nin; i++)
		rxbits[i] = (int)rx_out_internal[i];

	if ((FIELD_U8(modem, V32_OBJ_FLAGS) & V32_FLAG_DATA) != 0) {
		hdx = HDX(modem);
		FIELD_U16(hdx, V32HDX_MODE) = V32_PROTO_DATA;
		FIELD_U16(hdx, V32HDX_STATE) = V32_STATE_DONT_CARE;
	}

	n = *nout;
	if (n > 0) {
		scale = PARAMS(modem)->tx_scale;
		for (i = 0; i < n; i++)
			out[i] = (short)((out[i] * scale) >> 15);
	}

	return FIELD_INT(modem, V32_OBJ_STATUS);
}

/* ------------------------------------------------------------------------ */

/*
 * v32_data -- .text 0x0827a0.  `V32_PROTOCOL`'s data-mode handler, and its
 * argument list is `v32_handshake`'s because one table dispatches to both.
 *
 * ---------------------------------------------------------------------------
 * THE DEFERRED CONTROL REQUEST, AND THE BUG IN IT
 *
 * When `RetrainDetectV32` fires, this function builds a `struct v32fp_ctl`
 * from `V32_CTL` IN A STACK LOCAL, sets `Control_Flag`, and returns without
 * issuing it.  The call is made on the NEXT block, from the test at the top of
 * the function -- by which time the local it built is gone and the block
 * handed to `V32FP_control` is whatever is on the stack.  The renegotiation
 * arm two lines below does NOT defer and is correct.
 *
 * `docs/deviations.md` and finding F8645 have it; `V32FP_control`'s `Patch:`
 * debug line is the author's own workaround, putting the one bit that mattered
 * back from the status code.  **No differential test can drive that arm**,
 * because the two sides read two different stack frames.
 *
 * ---------------------------------------------------------------------------
 * THE CARRIER-LOSS TIMER IS FIVE MILLISECONDS A BLOCK
 *
 * hdx + 0xae counts blocks, and the comparison is `count * 5` against
 * `params.energy_drop_time`.  The format string is the author's --
 * `carrier_loss_time %d of %d ms` -- so the unit is MILLISECONDS and five of
 * them is one block, which is exactly `v32.c`'s 40 samples at 8000 Hz.  It is
 * also what makes `energy_drop_time`'s 700 a duration.  Finding F8648.
 *
 * LOCAL in the blob (`t`), so `static` here; `V32_PROTOCOL` holds its
 * address, so the convention stays the ordinary one and the differential
 * test declares it the ordinary way.
 */
static void
v32_data(void *modem, unsigned short *txdata, short *txout, short *rxin,
	 unsigned short *rxout, short *nsamples, unsigned short *rxcount)
{
	struct v32_status st;
	struct v32fp_ctl ctl;
	void *hdx;
	int code;
	int i;

	V32FP_status(modem, &st);
	code = 0;

	if (Control_Flag == 1) {
		/* `ctl` is uninitialised here.  See the note above. */
		V32FP_control(modem, &ctl);
		Control_Flag = 0;
		return;
	}

	hdx = HDX(modem);
	if (FIELD_S16(hdx, V32HDX_SYMBOL_LEN) > 0) {
		short *buf = (short *)FIELD_PTR(hdx, V32_HDX_BUF_A4);

		for (i = 0; i < FIELD_S16(hdx, V32HDX_SYMBOL_LEN); i++)
			buf[i] = (short)txdata[i];
	}

	if ((st.flags & V32_STFLAG_SCRAMBLE) != 0)
		ScrambleDataV32(modem, (short *)FIELD_PTR(hdx, V32_HDX_BUF_A4),
				(unsigned short)*nsamples);

	*rxcount = DemodDataV32(modem, rxin, rxout, *rxcount);
	*nsamples = (short)ModDataV32(modem,
				      (short *)FIELD_PTR(HDX(modem),
							 V32_HDX_BUF_A4),
				      txout, (unsigned short)*nsamples);

	if ((st.flags & V32_STFLAG_DESCRAMBLE) != 0)
		DescrambleDataV32(modem, (short *)rxout, *rxcount);
	if ((st.flags & V32_STFLAG_TXCLOCK) != 0)
		TxClockSyncV32(modem);
	if ((st.flags & V32_STFLAG_RETRAIN_DET) != 0
	    && RetrainDetectV32(modem) != 0) {
		FIELD_U8(modem, V32_OBJ_FLAGS) &= (unsigned char)~V32_FLAG_DATA;
		ctl = V32_CTL;
		ctl.ctl0 = (unsigned char)
			(((((ctl.ctl0 & 0xfc) | (st.flags & 1)
			    | (st.flags & 2)) & 0xfb) | (st.flags & 4)) | 0x80);
		ctl.ctl1 |= V32_CTL1_RETRAIN;
		code = V32_MSG_RETRAIN_REQ;
		Control_Flag = 1;
	}

	if (RenegotiateDetectV32(modem) != 0) {
		ctl = V32_CTL;
		ctl.ctl0 = (unsigned char)
			((((ctl.ctl0 & 0xfc) | (st.flags & 1)
			   | (st.flags & 2)) & 0xfb) | (st.flags & 4));
		ctl.ctl1 |= V32_CTL1_RENEG;
		ctl.r18 = 0;
		ctl.r1c = 0;
		FIELD_U16(HDX(modem), V32HDX_MODE) = V32_MODE_RING_RESP;
		V32FP_control(modem, &ctl);
		code = V32_MSG_RENEG;
	}

	if ((FIELD_U8(modem, V32_OBJ_FLAGS) & V32_FLAG_RETRAIN) != 0) {
		FIELD_U16(HDX(modem), V32HDX_LOSS_BLOCKS) = 0;
	} else {
		short elapsed;

		hdx = HDX(modem);
		FIELD_U16(hdx, V32HDX_LOSS_BLOCKS) = (unsigned short)
			(FIELD_U16(hdx, V32HDX_LOSS_BLOCKS) + 1);
		elapsed = (short)((short)FIELD_U16(hdx, V32HDX_LOSS_BLOCKS)
				  * V32_BLOCK_MS);
		if (PARAMS(modem)->energy_drop_time >= elapsed) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V32_MSG_NO_CARRIER won't be reported "
					"(carrier_loss_time %d of %d ms)\n",
					elapsed,
					PARAMS(modem)->energy_drop_time);
		} else {
			code = V32_MSG_NO_CARRIER;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V32_MSG_NO_CARRIER\n");
		}
	}

	/*
	 * Two bits at once, and NEITHER IS NAMED: the object writes the
	 * immediate 0x0c and nothing reconstructed reads either bit, so a
	 * name here would be an offset in disguise.  Recorded as the object's
	 * own constant.
	 */
	FIELD_U8(modem, V32_OBJ_FLAGS) |= (unsigned char)0x0c;
	FIELD_U8(modem, V32_OBJ_STATUS) = (unsigned char)code;
}

/*
 * `V32_PROTOCOL`, .data 0x007700, GLOBAL.  Nine slots indexed by
 * `V32HDX_MODE`, resolved from the nine `R_386_32 .text` relocations and NOT
 * from the file's bytes, which are zero -- the trap `tools/dis.py` exists for.
 *
 * FIVE slots are the handshake, one is the data-mode handler and THREE are the
 * one-byte `ret`.  Finding F8649 corrects F8594, which read it as six and one
 * and did not count the null ones.
 */
v32_protocol_fn V32_PROTOCOL[9] = {
	v32_handshake,				/* 0  V32_MODE_ORIGINATE    */
	v32_handshake,				/* 1  V32_MODE_ANSWER       */
	v32_handshake,				/* 2  V32_MODE_LOCLOOP_2    */
	(v32_protocol_fn)v32_null_protocol,	/* 3  V32_MODE_LOCLOOP_3    */
	v32_handshake,				/* 4  V32_MODE_RING_INIT    */
	v32_handshake,				/* 5  V32_MODE_RING_RESP    */
	v32_data,				/* 6  V32_PROTO_DATA        */
	(v32_protocol_fn)v32_null_protocol,	/* 7                        */
	(v32_protocol_fn)v32_null_protocol	/* 8                        */
};

/*
 * `PROTOCOL`, .data 0x007768, file-local and `static` here -- v32fpstat.h says
 * why.  Nine codes indexed the same way; `V32FP_status` copies one into
 * `struct v32_status::protocol` and nothing else reads the table.
 */
static short PROTOCOL[9] = {
	0, 1, 2, 9, 6, 6, 3, 7, 8
};

/* ------------------------------------------------------------------------ */

/*
 * V32FP_control -- .text 0x084530.  Always returns 1.
 */
int
V32FP_control(void *modem, struct v32fp_ctl *ctl)
{
	void *fp;
	void *hdx;
	int rate;

	if (FIELD_U8(modem, V32_OBJ_STATUS) == V32_MSG_RETRAIN_REQ) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Patch: set ctl_ptr->vxx_ctl."
					     "options.retrain = TRUE\n");
		ctl->ctl1 |= V32_CTL1_RETRAIN;
	}

	fp = FP(modem);
	FIELD_INT(fp, V32FP_R1C) = ctl->ctl0 & 1;
	FIELD_INT(fp, V32FP_R20) = (ctl->ctl0 >> 1) & 1;
	FIELD_INT(fp, V32FP_R24) = (ctl->ctl0 >> 2) & 1;
	FIELD_INT(fp, V32FP_R00) = ((ctl->ctl0 & 0x08) == 0);
	FIELD_INT(fp, V32FP_R0C) = ((ctl->ctl0 & 0x10) == 0);
	FIELD_INT(fp, V32FP_EQ_ADAPT) = ((ctl->ctl0 & 0x20) == 0);
	FIELD_INT(fp, V32FP_R14) = ctl->r18;
	FIELD_INT(fp, V32FP_R18) = ctl->r1c;
	FIELD_U8(modem, V32_OBJ_OPTIONS_HI) =
		(unsigned char)((FIELD_U8(modem, V32_OBJ_OPTIONS_HI)
				 & (unsigned char)~V32_OPTIONS_HI_CTL)
				| (unsigned char)((ctl->ctl0 >> 7) << 1));

	if (ctl->r14 != 0) {
		int ratio;

		if (FIELD_U8(modem, V32_OBJ_STATUS) != V32_MSG_RESIZE_DONE) {
			FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
			FIELD_U8(modem, V32_OBJ_STATUS) = V32_MSG_RESIZED;
		}
		/*
		 * BOTH TABLE READS ARE `movswl` HERE and `movzwl` twenty
		 * instructions later, on the same table and the same
		 * selector.  The difference is forced: this quotient is used
		 * as a 32-bit multiplier, so the extension survives, while
		 * the three below are stored 16-bit and it does not.  Finding
		 * F614's two columns in one function.
		 */
		ratio = V32_SAMPLE_LEN[PARAMS(modem)->symlen_sel]
			/ V32_SAMPLE_LEN[PARAMS(modem)->r16];
		hdx = HDX(modem);
		FIELD_U16(hdx, V32_HDX_SHORT_9C) =
			(unsigned short)(ratio
					 * FIELD_U16(hdx, V32_HDX_SHORT_9C));
		FIELD_U16(hdx, V32HDX_BLOCK_CHARGE) =
			(unsigned short)V32_SYMBOL_LEN[PARAMS(modem)->symlen_sel];
		FIELD_U16(hdx, V32HDX_SYMBOL_LEN) =
			(unsigned short)V32_SYMBOL_LEN[PARAMS(modem)->symlen_sel];
		FIELD_U16(hdx, V32HDX_SAMPLE_LEN) =
			(unsigned short)V32_SAMPLE_LEN[PARAMS(modem)->symlen_sel];
		FIELD_U8(modem, V32_OBJ_STATUS) = V32_MSG_OK;
	}

	if ((ctl->ctl1 & V32_CTL1_RETRAIN) != 0) {
		if (FIELD_U16(fp, V32FP_RATE_FALLBACK) != 0) {
			switch (FIELD_S16(fp, V32FP_SHORT_2E)) {
			case V32_RATE_14400:
				PARAMS(modem)->rx_rate = 12000;
				PARAMS(modem)->tx_rate = 12000;
				break;
			case V32_RATE_12000:
				PARAMS(modem)->rx_rate = 9600;
				PARAMS(modem)->tx_rate = 9600;
				break;
			case V32_RATE_9600:
			case V32_RATE_9600_NT:
				PARAMS(modem)->rx_rate = 7200;
				PARAMS(modem)->tx_rate = 7200;
				break;
			case V32_RATE_7200:
				PARAMS(modem)->rx_rate = 4800;
				PARAMS(modem)->tx_rate = 4800;
				break;
			default:
				break;
			}
		}
		/*
		 * TWO SEQUENTIAL `if`s AND NOT AN `else if`, and the object
		 * proves it: the protocol is RE-LOADED at 8477e after the
		 * originate arm has run, which an `else` could not need.
		 * `V32FP_recreate` copies its parameter block over the
		 * object's first 48 bytes, and the object here IS that
		 * parameter block, so the compiler cannot know the value
		 * survived the call.
		 */
		if (PARAMS(modem)->protocol == 0) {
			V32FP_recreate(modem, PARAMS(modem), 0);
			hdx = HDX(modem);
			FIELD_U16(hdx, V32HDX_STATE) = 1;
			FIELD_INT(hdx, V32HDX_LONG_7C) = 0x960;
			V32OrgNextState(modem);
		}
		if (PARAMS(modem)->protocol == 1) {
			V32FP_recreate(modem, PARAMS(modem), 0);
			hdx = HDX(modem);
			FIELD_U16(hdx, V32HDX_STATE) = 1;
			V32AnsNextState(modem);
		}
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_RETRAIN;
		FIELD_U8(modem, V32_OBJ_STATUS) = V32_MSG_RETRAINING;
		ctl->ctl1 &= (unsigned char)~V32_CTL1_RETRAIN;
		return 1;
	}

	if ((ctl->ctl1 & V32_CTL1_RENEG) != 0) {
		hdx = HDX(modem);
		FIELD_U16(hdx, V32HDX_STATE) = 0;
		FIELD_U8(modem, V32_OBJ_FLAGS) &= (unsigned char)~V32_FLAG_DATA;
		if (FIELD_S16(hdx, V32HDX_MODE) == V32_MODE_RING_RESP) {
			FIELD_U16(hdx, V32HDX_SHORT_44) = 0;
			V32RngRespNextState(modem);
			ctl->ctl1 &= (unsigned char)~V32_CTL1_RENEG;
			return 1;
		}
		FIELD_U16(hdx, V32HDX_MODE) = V32_MODE_RING_INIT;

		PARAMS(modem)->trellis = ctl->trellis;
		rate = ctl->bps;
		PARAMS(modem)->tx_rate = (short)rate;
		PARAMS(modem)->rx_rate = (short)rate;
		switch (rate) {
		case 14400:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_14400;
			break;
		case 12000:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_12000;
			break;
		case 9600:
			/*
			 * `2 - (trellis == 0)`, which is the same six
			 * instructions `V32FP_recreate` uses at 7f40b and
			 * 7f60f -- v32seq.h records both.
			 */
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = (unsigned short)
				(2 - (PARAMS(modem)->trellis == 0));
			break;
		case 7200:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_7200;
			break;
		default:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_4800;
			break;
		}
		FIELD_U16(fp, V32FP_RX_RATE_IDX) =
			FIELD_U16(fp, V32FP_TX_RATE_IDX);
		V32RngInitNextState(modem);
		ctl->ctl1 &= (unsigned char)~V32_CTL1_RENEG;
		return 1;
	}

	return 1;
}

/* ------------------------------------------------------------------------ */

/*
 * V32FP_status -- .text 0x084840.  Always returns 1.
 *
 * Two jobs in one function: fill the caller's block, and -- if the line has
 * got worse -- build a control request out of `V32_CTL` and issue it on the
 * spot.  It is the only writer of `V32_OBJ_SNR_DROPS`.
 *
 * ---------------------------------------------------------------------------
 * THE SNR IS A LOG OF A SMOOTHED DECISION ERROR, AND EVERY CONSTANT IS Q15
 *
 *     err  = (err * 0x7333 + 0x4000) >> 15
 *          + ((unsigned short)(fp->0x256 >> 2) * 0xccd + 0x4000) >> 15
 *
 * 0x7333 and 0xccd are 0.9 and 0.1 in Q15 and 0x4000 is the round-to-nearest
 * term, so this is a first-order smoother with a time constant of ten blocks.
 * The result is written back to fp + 0x50d6, which the object's own debug line
 * calls `Dec error`.
 *
 * What comes out is `log10` of a reference over that error:
 *
 *     k    = (rate is 12000 or 14400 ? 0x4000 : 0) + 0x1400 - err
 *     m    = (k * FPM_div_reciprocal(err)) >> 16
 *     snr  = ((short)((FPM_log10(m, ~shift) >> 3) * 10) + 256) >> 9
 *
 * -- and the `+ 256 >> 9` is a rounded divide by 512.  The SECOND log, which
 * fills `struct v32_status::r0a`, is the same expression over fp + 0x100 and
 * fp + 0x102 with three differences the object is explicit about: `>> 15`
 * rather than `>> 16`, `-shift` rather than `~shift`, and no `+ 256`.  None of
 * the three is a free choice and all three are reproduced.
 *
 * ---------------------------------------------------------------------------
 * TWO GUARDS THAT ARE NOT THERE
 *
 * `RATEv32` is indexed with fp + 0x2c and fp + 0x2e RAW, with no clamp -- the
 * clamp to 5 above applies only to `SnrToRetrainTable`.  That is D404's shape
 * at two more sites: a six-entry table subscripted from a sixteen-bit field.
 * Recorded rather than fixed.
 */
int
V32FP_status(void *modem, struct v32_status *st)
{
	struct v32fp_ctl ctl;
	void *fp;
	void *hdx;
	unsigned short recip;
	unsigned short shift;
	int rate_idx;
	int snr;
	int r0a;
	int err;
	int k;
	int m;
	int t;

	fp = FP(modem);
	rate_idx = FIELD_S16(fp, V32FP_SHORT_2E);
	if ((unsigned int)rate_idx > 5)
		rate_idx = 5;

	err = ((FIELD_S16(fp, V32FP_DEC_ERROR) * 0x7333 + 0x4000) >> 15)
		+ (((int)(unsigned short)(FIELD_S16(fp, V32FP_SHORT_256) >> 2)
		    * 0xccd + 0x4000) >> 15);
	snr = V32_SNR_SETTLED_VALUE;
	r0a = 0;
	FIELD_S16(fp, V32FP_DEC_ERROR) = (short)err;

	if ((short)err != 0) {
		k = (short)((((unsigned short)
			      (FIELD_U16(fp, V32FP_SHORT_2E) - 4) < 2)
			     ? 0x4000 : 0) + 0x1400 - err);
		FPM_div(FIELD_U16(fp, V32FP_DEC_ERROR), &recip, &shift);
		m = (k * (int)recip) >> 16;
		snr = V32_SNR_FLOOR;
		if ((short)m > 0) {
			t = (int)FPM_log10((unsigned short)m,
					   (short)~(short)shift) >> 3;
			snr = ((int)(short)(t * 10) + 0x100) >> 9;
		}
		fp = FP(modem);
		if (FIELD_S16(fp, V32FP_SHORT_102) != 0
		    && FIELD_S16(fp, V32FP_SHORT_100) != 0) {
			FPM_div(FIELD_U16(fp, V32FP_SHORT_102), &recip, &shift);
			fp = FP(modem);
			m = (FIELD_S16(fp, V32FP_SHORT_100) * (int)recip) >> 15;
			t = (int)FPM_log10((unsigned short)m,
					   (short)-(short)shift) >> 3;
			r0a = (short)(t * 10) >> 9;
			fp = FP(modem);
		}
	}

	if ((short)FIELD_U16(modem, V32_OBJ_SHORT_2C)
	    <= V32_SNR_SETTLE_BLOCKS) {
		FIELD_U16(modem, V32_OBJ_SHORT_2C) = (unsigned short)
			(FIELD_U16(modem, V32_OBJ_SHORT_2C) + 1);
		snr = V32_SNR_SETTLED_VALUE;
	}

	hdx = HDX(modem);
	st->protocol = PROTOCOL[FIELD_S16(hdx, V32HDX_MODE)];
	st->tx_rate = RATEv32[FIELD_S16(fp, V32FP_SHORT_2C)];
	st->rx_rate = RATEv32[FIELD_S16(fp, V32FP_SHORT_2E)];
	st->r0a = (short)r0a;
	st->snr = (short)snr;
	st->r06 = (short)(1 - (FIELD_S16(fp, V32FP_SHORT_256) >> 1));
	st->r0e = 0;
	st->r0c = (short)FIELD_U16(fp, V32FP_SHORT_1F8);
	st->r10 = (short)FIELD_U16(fp, V32FP_SHORT_6C);
	st->r12 = (short)FIELD_U16(fp, V32FP_SHORT_182);

	/*
	 * Eight read-modify-writes of the caller's byte, in the object's own
	 * order.  BIT 6 IS FORCED TO ZERO and bit 7 comes from the object's
	 * fault flag; the other six are the datapump switches, three of them
	 * INVERTED, which is exactly the set `V32FP_control` writes from
	 * `struct v32fp_ctl::ctl0`.
	 */
	st->flags = (unsigned char)((st->flags & 0xfe)
				    | (FIELD_U8(fp, V32FP_R1C) & 1));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xfd)
				    | ((FIELD_U8(fp, V32FP_R20) & 1) << 1));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xfb)
				    | ((FIELD_U8(fp, V32FP_R24) & 1) << 2));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xf7)
				    | ((FIELD_INT(fp, V32FP_R00) == 0) << 3));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xef)
				    | ((FIELD_INT(fp, V32FP_R0C) == 0) << 4));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xdf)
				    | ((FIELD_INT(fp, V32FP_EQ_ADAPT) == 0) << 5));
	st->flags = (unsigned char)(st->flags & 0xbf);
	st->flags = (unsigned char)((st->flags & 0x7f)
				    | (((FIELD_U8(modem, V32_OBJ_OPTIONS_HI)
					 >> 1) & 1) << 7));
	st->r20 = 0;
	st->r24 = 0;
	st->flags1 = (unsigned char)((st->flags1 & 0xfe)
				     | ((FIELD_U8(modem, V32_OBJ_OPTIONS_HI)
					 >> 2) & 1));

	fp = FP(modem);
	st->r18 = FIELD_INT(fp, V32FP_INT_188);
	st->r1c = (FIELD_INT(fp, V32FP_INT_1F4) != 1);

	if (SnrToRetrainTable[rate_idx] <= (short)snr) {
		FIELD_U16(modem, V32_OBJ_SNR_DROPS) = 0;
		return 1;
	}

	ctl = V32_CTL;
	if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V32STC - SNR drop observed, "
				     "SNR = %d < threshold = %d\n",
				     snr, SnrToRetrainTable[rate_idx]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Dec error = %d (*64)\n",
					     FIELD_S16(FP(modem),
						       V32FP_DEC_ERROR));
	}

	if ((short)FIELD_U16(modem, V32_OBJ_SNR_DROPS) > V32_SNR_DROPS_LIMIT) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("%d coseq SNR drops detected "
					     "local retrain is initiated\n",
					     V32_SNR_DROPS_LIMIT);
		ctl.ctl1 |= V32_CTL1_RETRAIN;
		FIELD_U16(modem, V32_OBJ_SNR_DROPS) = 0;
		FIELD_U16(modem, V32_OBJ_SHORT_2C) = 0;
		FIELD_U16(FP(modem), V32FP_RATE_FALLBACK) = 1;
	} else {
		FIELD_U16(modem, V32_OBJ_SNR_DROPS) = (unsigned short)
			(FIELD_U16(modem, V32_OBJ_SNR_DROPS) + 1);
	}

	ctl.ctl0 = (unsigned char)
		(((((ctl.ctl0 & 0xfc) | (st->flags & 1)
		    | (st->flags & 2)) & 0xfb) | (st->flags & 4)) | 0x80);
	V32FP_control(modem, &ctl);
	return 1;
}
