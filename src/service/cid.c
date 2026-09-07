/*
 * cid.c -- the Caller ID service's face to slmodemd, reconstructed from the
 * blob's second span labelled `dp_init.c`.
 *
 *   CID_create    .text 0x000340   195 bytes
 *   CID_delete    .text 0x000410    84 bytes
 *   CID_process   .text 0x000470   386 bytes
 *
 * slmodemd declares all three itself (`ref/slmodemd/modem.c:87-89`):
 *
 *     void *CID_create(struct modem *m, unsigned rate, unsigned cid_val);
 *     void  CID_delete(void *cid);
 *     int   CID_process(void *cid, void *in, int count);
 *
 * so the signatures here are quoted, not inferred.  The object underneath is
 * eight bytes: the modem handle, for `modem_send_to_tty`, and the real CID
 * receiver `cid_create` hands back.  Everything that demodulates lives below
 * this file, in `src/service/rxcid.c`, `cid_fsd.c`, `cid_mtd.c` and
 * `dtmf_rx.c`, and all of it is written -- an unwritten callee would not
 * "resolve to the blob's copy at link time" but fail to link at all (F8492).
 *
 * THE SPAN NAME IS NOT THE MODULE NAME (the CLAUDE.md rule): the blob's
 * layout labels 0x340..0x5f5 `dp_init.c`, the same label as the two
 * `prop_dp_*` functions at 0x0 -- but `dcr.c`'s four functions sit between
 * (0x60..0x33f), so those two stretches cannot be one translation unit and
 * this file is separate from `src/core/dp_init.c` on that evidence.
 *
 * THE OTHER HALF OF THIS FILE is the service object's own small setters
 * and TLV walkers, written in the same wave from the V32mod.c span:
 * Reconstructed from dsplibs.o cid.c (finding F1410 for the TU map):
 *
 *   cid_reset             .text 0x08fce0   118
 *   cid_freq_sampl        .text 0x08fd60   107
 *   cid_threshold         .text 0x08fdd0    40
 *   cid_value             .text 0x08fe00    15
 *   cid_create            .text 0x08fe10   204
 *   cid_delete            .text 0x08fee0    86
 *   cid_progress          .text 0x08ff40   992
 *   cid_get_strings       .text 0x090320   145
 *   _look_for             .text 0x0903c0    72
 *   _look_for_other_than  .text 0x090410    88
 *
 * The four leaves were exported API with no internal referrer (finding
 * F8320's bucket), written on their own merit; `cid_freq_sampl` came with the
 * CID service pass and calls nothing.  The rest came with the receiver
 * underneath: `cid_reset`, `cid_create` and `cid_delete` needed `reset_cid`
 * and `create_cid` (Rxcid.c) written first, which is why F8492 records the
 * link constraint -- not difficulty -- as what ordered this file.
 *
 * `cid_progress` was the last of them and is written now, so this file is
 * complete and no CID symbol is bridged in `test/harness/unwritten.c` any
 * more.
 *
 * See include/dsplib/cid_modem.h for the object and the mode encoding.
 */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/cid_modem.h"
#include "dsplib/cid.h"
#include "dsplib/dtmf_rx.h"

/* Host callback, undefined in the object, supplied by slmodemd's modem.c. */
extern int modem_send_to_tty(void *m, const void *buf, int n);

/*
 * THE WEAK DECLARATION OF `cid_progress` THAT USED TO BE HERE IS GONE.
 * It was the `DSPLIB_*_UNWRITTEN` idiom, bridged to the blob's `ref_` alias
 * by `test/harness/unwritten.c` while the receiver underneath was unwritten;
 * this file defines the function now, so the bridge is deleted and the
 * declaration comes from `cid_modem.h` like every other one here.  That is the
 * "WHEN A BRIDGED SYMBOL IS RECONSTRUCTED" paragraph in `unwritten.c`, taken
 * for the fifth and last CID symbol.
 */

/* The line rates `CID_create` accepts; cid.h names the same two values. */
#define CID_LINE_8000	8000
#define CID_LINE_9600	9600

/*
 * `cid_progress` is fed at most this many samples per call; `CID_process`
 * walks a longer buffer in chunks of it.
 */
#define CID_CHUNK	192

/*
 * What `cid_freq_sampl` puts back into the FSK receiver's
 * `mark_conf_step`, and what `create_cid` seeds it with.  Usage inference
 * only -- the object has no string and no other caller that types it, and
 * the value is the same 9 down all three of cid_freq_sampl's arms.
 */
#define CID_MARK_CONF_STEP	9

/*
 * The one value of `struct cid_modem::cid_val` (was `f264`, renamed this
 * wave; see the struct's own field comment for why) -- the `cid_val`
 * slmodemd hands `CID_create`, which `cid_create` parks at +0x264 and
 * `cid_value` overwrites -- that the object tests for: it sends
 * `cid_get_strings` to the raw hex dump instead of the labelled rendering.
 * Usage inference from that single comparison; no format string names it
 * and no other function reads the field.
 */
#define CID_VALUE_RAW	2

/*
 * The five values of `mode` -- including CID_MODE_AUTOMATIC, whose name is the
 * object's own "\n AUTOMATIC MODULATION MODE \n" -- now live in cid_modem.h
 * beside the object they belong to, because `cid_progress` reads and writes
 * three more of them than this file used to.
 */

struct CID {
	void *modem;		/* +0x0 slmodemd's struct modem       */
	void *cid;		/* +0x4 what `cid_create` handed back */
};

void *
CID_create(void *m, unsigned rate, unsigned cid_val)
{
	struct CID *cid;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("cid: create...\n");

	if (rate != CID_LINE_8000 && rate != CID_LINE_9600)
		return 0;

	cid = sysdep_malloc(sizeof(struct CID));
	if (!cid)
		return 0;
	sysdep_memset(cid, 0, sizeof(struct CID));

	cid->modem = m;
	cid->cid = cid_create(0, cid_val, 0);
	if (!cid->cid) {
		sysdep_free(cid);
		return 0;
	}

	if (rate == CID_LINE_9600)
		cid_freq_sampl(cid->cid, CID_LINE_9600);

	return cid;
}

void
CID_delete(void *cidp)
{
	struct CID *cid = cidp;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("cid: delete...\n");

	cid_delete(cid->cid);
	sysdep_free(cid);
}

/*
 * Run `count` line samples through the receiver.  0 while the message is
 * still arriving, 1 once a whole one has been delivered to the TTY, -1 when
 * the receiver gives up -- and the first chunk that settles it ends the
 * call; the rest of the buffer is not offered.
 *
 * On success the receiver hands back its strings as up to four consecutive
 * NUL-terminated fields in one buffer, ended early by an empty one.  Each is
 * sent to the TTY with its own CRLF.
 */
int
CID_process(void *cidp, void *in, int count)
{
	struct CID *cid = cidp;
	short *buf = in;
	int ret = 0;

	while (count > 0) {
		int chunk = count;
		short n;
		short res;

		if (chunk > CID_CHUNK)
			chunk = CID_CHUNK;
		n = chunk;
		ret = 0;
		res = cid_progress(cid->cid, buf, 0, &n);
		if (res == 1) {
			char *p;
			int i;

			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("cid: CID Success\n");
			ret = 1;
			p = cid_get_strings(cid->cid);
			for (i = 0; i <= 3; i++) {
				if (!p || !*p)
					break;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "cid: STR: '%s'\n", p);
				modem_send_to_tty(cid->modem, p,
						  sysdep_strlen(p));
				modem_send_to_tty(cid->modem, "\r\n", 2);
				p += sysdep_strlen(p) + 1;
			}
		} else if (res != 0) {
			ret = -1;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("cid: CID Failure\n");
		}
		if (ret)
			break;
		count -= chunk;
		buf += chunk;
	}

	return ret;
}


/*
 * Reset both receivers in place, without freeing or reallocating anything.
 *
 * The mode clamp is `cid_create`'s, repeated: anything above 1 becomes 5, so
 * a caller can raise the mode between calls and the receivers it needs are
 * the ones that get reset.  What it does NOT do is BUILD the second receiver,
 * so raising the mode here leaves a null pointer that `reset_dtmf` or
 * `reset_cid` then walks -- the object has no guard and neither has this.
 *
 * `samples_fill` is `cid_progress`'s sample-buffer fill level, cleared
 * last, and the mode-5 arm puts `mark_conf_step` back exactly as
 * `cid_freq_sampl` and `cid_create` do.
 */
void
cid_reset(struct cid_modem *ctx)
{
	if (ctx->mode > 1)
		ctx->mode = CID_MODE_AUTOMATIC;
	if (ctx->mode != 0)
		reset_dtmf(ctx->dtmf);
	if (ctx->mode != 1)
		reset_cid(ctx->fsk);
	ctx->samples_fill = 0;
	if (ctx->mode == CID_MODE_AUTOMATIC)
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;
}

/*
 * Retune both receivers.  The mode gating is cid_threshold's -- `!= 0` reaches
 * the DTMF receiver, `!= 1` the FSK one -- and `rate` is stored as a short on
 * both sides, which is the object's own truncation of the int argument.
 *
 * THE THREE STORES OF 9 ARE THE OBJECT'S.  It writes `movw $0x9,0x2c(...)`
 * three times over, into the FSK receiver's mark_conf_step, from three
 * separate tests: mode 0 with rate 9600, mode 0 with rate 8000, and
 * mode > 1 for any rate.  The first two are if-converted in the object --
 * one `sete` for `mode == 0`, reused, ANDed against a second `sete` per
 * rate -- so they are two statements sharing a condition and not one test
 * of a rate pair.  The value is 9 in all three, which is also what
 * create_cid seeds mark_conf_step with, so the net effect over the modes
 * and rates this service uses is to put it back.  Kept as three because
 * that is what the object encodes; nothing here reads a rate-dependent
 * value into it.
 *
 * Mode 1 touches the FSK receiver not at all, which is why the pointer
 * may be null there: every mark_conf_step store is under `mode == 0` or
 * `mode > 1`.
 */
void
cid_freq_sampl(struct cid_modem *ctx, int rate)
{
	if (ctx->mode != 0)
		ctx->dtmf->rate = (short)rate;
	if (ctx->mode != 1)
		ctx->fsk->rate = (short)rate;

	if (ctx->mode == 0 && rate == CID_RATE_9600)
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;
	if (ctx->mode == 0 && rate == CID_RATE_8000)
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;
	if (ctx->mode > 1)
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;
}

/*
 * Both writes are gated on the mode, with the same two tests cid_create and
 * cid_reset use: `!= 0` reaches the DTMF side, `!= 1` the FSK side, so mode
 * 5 sets both.  The object stores the int argument as a short on both sides.
 */
void
cid_threshold(struct cid_modem *ctx, int thr)
{
	if (ctx->mode != 0)
		ctx->dtmf->sens = (short)thr;
	if (ctx->mode != 1)
		ctx->fsk->threshold = (short)thr;
}

void
cid_value(struct cid_modem *ctx, int v)
{
	ctx->cid_val = v;
}

/*
 * Build the service object, or rebuild the receivers inside one the caller
 * already has: a null `ctx` allocates 0x3fc bytes and takes `mode` from the
 * argument, and a non-null one keeps the mode it already has and only builds.
 * Both paths end at the same three statements, which is why the object reads
 * `ctx->mode` back from memory after each call rather than keeping it in a
 * register -- `create_cid_dtmf` and `create_cid` could have changed it.
 *
 * THE CLAMP IS THE AUTHOR'S OWN WORDS: any mode above 1 becomes 5 and, at
 * debug level 2, says so as " AUTOMATIC MODULATION MODE ".  That is the
 * strongest evidence available for what mode 5 means (CLAUDE.md's evidence
 * order, rule 1) and it is why the constant is named rather than left as 5.
 *
 * Each receiver constructor is handed the EXISTING pointer, so a second call
 * on the same object reuses the allocation rather than leaking it.
 */
void *
cid_create(struct cid_modem *ctx, int cid_val, int mode)
{
	if (!ctx) {
		ctx = sysdep_malloc(CID_MODEM_BYTES);
		if (!ctx)
			return 0;

		ctx->mode = mode;
		ctx->fsk = 0;
		ctx->dtmf = 0;
		ctx->samples_fill = 0;

		if (mode > 1) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "\n AUTOMATIC MODULATION MODE \n");
			ctx->mode = CID_MODE_AUTOMATIC;
		}
	}

	if (ctx->mode != 0)
		ctx->dtmf = create_cid_dtmf(ctx->dtmf);
	if (ctx->mode != 1)
		ctx->fsk = create_cid(ctx->fsk);
	if (ctx->mode == CID_MODE_AUTOMATIC)
		ctx->fsk->mark_conf_step = CID_MARK_CONF_STEP;

	ctx->cid_val = cid_val;
	return ctx;
}

/*
 * Tear the object down.  The DTMF receiver is released with a plain
 * `sysdep_free` -- it owns nothing -- and the FSK one needs its resampler
 * history released first.
 *
 * `sysdep_free(ctx->fsk)` is UNCONDITIONAL, including on the DTMF-only mode
 * where nothing ever built one: the pointer is null there and free(NULL) is
 * the object's own reading of it.  The mode is re-read from memory between
 * the two tests, which is why this is two `if`s and not an if/else chain.
 *
 * The object places a literal 1 in the second argument slot before
 * `FPM_MRF_free`, which takes ONE argument and never loads it (0xa8df0 reads
 * `0x4(%esp)` and tail-calls `sysdep_free`).  Not reproduced, on exactly the
 * ground `B103FP_delete` states for the same dead slot: an argument the callee
 * never loads has no observable effect.
 */
void
cid_delete(struct cid_modem *ctx)
{
	if (ctx->mode != 0)
		sysdep_free(ctx->dtmf);
	if (ctx->mode != 1)
		FPM_MRF_free(&ctx->fsk->mrf);
	sysdep_free(ctx->fsk);
	sysdep_free(ctx);
}

/*
 * The service's state machine: buffer the caller's samples a block at a time
 * and run whichever receivers the mode selects over each full block.
 *
 * THE BLOCK IS 20 ms.  `len` is 160 when the receiver's `rate` is 8000 and 192
 * otherwise, taken from the DTMF receiver when the mode has one and then
 * OVERWRITTEN from the FSK receiver when the mode has one of those -- two
 * separate `if`s in the object, not an if/else, so mode 5 ends up using the
 * FSK receiver's rate for both.  Neither is initialised in the object; the two
 * tests are `mode != 0` and `mode != 1` and cannot both be false, so `len` is
 * always assigned before it is read.
 *
 * `*count` IS AN INPUT ONLY.  The object reads it once as a short and stores
 * zero over it immediately, and never looks at it again -- so a caller cannot
 * learn how many samples were consumed, and `CID_process` does not try.
 *
 * THE THIRD ARGUMENT IS NEVER READ: `0x48(%esp)` appears nowhere in the
 * function's 992 bytes, and `CID_process` passes a literal zero into it.
 *
 * WHAT THE RETURN VALUES MEAN comes from the four format strings this
 * function carries, which are the author's own words:
 *
 *   1  a whole message arrived           (a receiver answered 3)
 *   2  a receiver gave up                ("... demodulator Failed ...")
 *   3  the DTMF receiver is mid-string in mode 1, or the mode is one of the
 *      values the dispatch has no arm for
 *   0  nothing has happened yet
 *
 * `CID_process` reads 1 as success and everything non-zero as failure, so 2
 * and 3 both end the call there.
 *
 * THE THREE RESULT VARIABLES ARE FUNCTION-SCOPE AND ALL START AT 1, and the
 * join below tests all three whatever arm ran -- so a block processed in mode
 * 5 is judged partly on `res`, which only the single-receiver arms ever
 * write, and a block processed in mode 3 is judged partly on the automatic
 * arm's two.  That is the object's shape: three separate stack slots, each
 * initialised to 1 in the prologue and each surviving from one turn of the
 * outer loop to the next.
 *
 * `ret = 3` on the mode > 1, mode != 5 path is set BEFORE the `mode == 3`
 * test and so covers mode 3 as well as the modes with no arm at all -- which
 * means a mode-3 block where the DTMF receiver is still collecting returns 3
 * and `CID_process` calls that a failure.  Kept as encoded; slmodemd only ever
 * reaches mode 0, because `CID_create` passes `cid_create` a mode of zero.
 */
short
cid_progress(struct cid_modem *ctx, short *in, int what, short *count)
{
	int len = 0;
	short n;
	short i;
	int ret = 0;
	short auto_dtmf = 1;
	short auto_fsk = 1;
	short res = 1;

	if (ctx->mode != CID_MODE_FSK)
		len = ctx->dtmf->rate != DTMF_RX_RATE_8000
		      ? CID_BLOCK_9600 : CID_BLOCK_8000;
	if (ctx->mode != CID_MODE_DTMF)
		len = ctx->fsk->rate != CID_RATE_8000
		      ? CID_BLOCK_9600 : CID_BLOCK_8000;

	n = *count;
	*count = 0;
	i = 0;

	for (;;) {
		/*
		 * Top up the block from the caller's buffer.  `i` and `n` are
		 * shorts and the comparison is sixteen bits wide, which is the
		 * object's; `samples_fill` is the fill level and survives the call, so
		 * a caller feeding fewer samples than a block just returns 0
		 * and comes back.
		 */
		while (i < n && ctx->samples_fill < len)
			ctx->samples[ctx->samples_fill++] = in[i++];

		if (ctx->samples_fill != len)
			return (short)ret;

		if (ctx->mode <= CID_MODE_DTMF) {
			if (ctx->mode == CID_MODE_DTMF)
				res = (short)dtmf_modem(ctx->samples, len,
						       ctx->dtmf);
			else
				res = (short)cid_modem(ctx->samples, len,
						      ctx->fsk);
			/*
			 * The mode is re-read from memory here, and after
			 * every debug call below, because the object does:
			 * `cmpl $0x1,0x260(%ebx)` rather than a cached copy.
			 */
			if (res == 2 && ctx->mode == CID_MODE_DTMF)
				ret = 3;
		} else {
			if (ctx->mode == CID_MODE_AUTOMATIC) {
				auto_dtmf = (short)dtmf_modem(ctx->samples, len,
							     ctx->dtmf);
				auto_fsk = (short)cid_modem(ctx->samples, len,
							    ctx->fsk);
			} else {
				ret = 3;
				if (ctx->mode == CID_MODE_CID_MESSAGE)
					res = (short)dtmf_modem(ctx->samples,
							       len, ctx->dtmf);
			}

			/* Digits are arriving: commit to the DTMF side. */
			if (auto_dtmf == 2)
				ctx->mode = CID_MODE_CID_MESSAGE;
			/*
			 * A DTMF string that completed or gave up inside one
			 * block of the automatic mode is a failure, and the
			 * author says why: there were not enough digits.
			 */
			if (auto_dtmf == 3 || auto_dtmf == -1) {
				ret = 2;
				ctx->mode = CID_MODE_CID_MESSAGE;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "\n DTMF Failed , not enough numbers detected \n");
			}
			/* A whole FSK message: commit to the FSK side and win. */
			if (auto_fsk == 3) {
				ctx->mode = CID_MODE_FSK_DONE;
				ret = 1;
			}
			/*
			 * The FSK receiver gave up.  It is reset and the run
			 * continues -- this arm does NOT set `ret`, so the
			 * automatic mode keeps hunting on the DTMF side.
			 */
			if (auto_fsk == -1) {
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "\n FSK demodulator Failed \n");
				reset_cid(ctx->fsk);
			}
		}

		if (res == 3)
			ret = 1;
		if (res == -1) {
			ret = 2;
			/*
			 * Three separate tests of the mode, each re-reading
			 * the field, and each with its own message: which
			 * receiver failed, and -- for mode 3 -- that it failed
			 * after the automatic mode had already committed to it.
			 * That last string is where CID_MODE_CID_MESSAGE's
			 * name comes from.
			 */
			if (ctx->mode == CID_MODE_FSK
			    && dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "\n FSK demodulator Failed \n");
			if (ctx->mode == CID_MODE_DTMF
			    && dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "\n DTMF demodulator Failed  \n");
			if (ctx->mode == CID_MODE_CID_MESSAGE
			    && dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "\n DTMF demodulator Failed after CID_MESSAGE state \n");
		}

		ctx->samples_fill = 0;
	}
}

/*
 * Clear the whole string buffer, render into it, and hand it back.  The
 * memset is unconditional and covers all 0x258 bytes, so a caller walking the
 * result stops on a NUL whatever the renderers did.
 *
 * The receiver is chosen by `mode != 0 && mode != 2`, if-converted in the
 * object into two `setne`s and a `test`.  **Mode 2 is on the FSK side**,
 * which no other test in this file does -- everything else reads `> 1` or
 * `== 5` -- and `cid_create` clamps anything above 1 to 5, so the value is
 * reachable only from a caller that writes `mode` itself.  Kept as written.
 *
 * The DTMF answer is sixteen bytes of `digits[20]` copied byte by byte, with
 * no terminator added: the object's loop runs 0..15 inclusive and stops.  It
 * relies on the memset above for the terminator, which is why that covers the
 * whole buffer rather than the sixteen bytes it is about to fill.
 */
char *
cid_get_strings(struct cid_modem *ctx)
{
	char *out = ctx->strings;
	int i;

	sysdep_memset(out, 0, sizeof(ctx->strings));

	if (ctx->mode != CID_MODE_FSK && ctx->mode != CID_MODE_FSK_DONE) {
		for (i = 0; i <= 15; i++)
			out[i] = ctx->dtmf->digits[i];
		return out;
	}

	if (ctx->cid_val == CID_VALUE_RAW)
		data_unformatted_output(ctx->fsk, out);
	else
		data_formatted_output(ctx->fsk, out);

	return out;
}

/*
 * Walk the TLV entries: buf[2] is the first tag, buf[pos + 1] each entry's
 * length, and the next entry starts at pos + length + 2.  The scan bound is
 * buf[1], the message length, read once as a SIGNED char; positions live in
 * shorts.  Both are the object's widths -- a length byte >= 0x80 steps the
 * position BACKWARDS here, exactly as it does there.
 */
int
_look_for(const char *buf, char tag)
{
	short len = buf[1];
	short pos = 2;

	while (pos < len) {
		if (buf[pos] == tag)
			return pos;
		pos = (short)(pos + buf[pos + 1] + 2);
	}
	return -1;
}

/*
 * The same walk from `start`, returning the first entry whose tag is none
 * of 1, 2 or 7 and whose position is not `except`.  The object tests 1 and
 * 7 together (two setne into one test) and 2 apart; behaviourally one
 * exclusion set.
 */
int
_look_for_other_than(const char *buf, short start, int except)
{
	/* This order is the original object's preimage under GCC 3.4.2. */
	short pos = start;
	short len = buf[1];

	while (pos < len) {
		char tag = buf[pos];

		if (tag != 1 && tag != 7 && tag != 2 && pos != except)
			return pos;
		pos = (short)(pos + buf[pos + 1] + 2);
	}
	return -1;
}
