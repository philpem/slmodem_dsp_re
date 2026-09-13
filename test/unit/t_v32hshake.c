/*
 * t_v32hshake.c -- differential test of `v32_handshake`, 0x082b00.
 *
 * `v32_handshake` is the two half-duplex drivers' only caller, and like them
 * it needs no real state to be tested: SCRIPTED states installed into both
 * contexts exercise everything it does while being a fixture rather than a
 * reconstruction.  The same argument as `t_v32hdx.c`'s, and the same shape.
 *
 * WHAT EACH TRIAL IS FOR, as a NAMED WRONG READING:
 *
 *   - THE SEVEN ARGUMENTS GROUPED INSTEAD OF INTERLEAVED.  The receive and
 *     transmit arguments alternate: +0x44 and +0x48 are transmit, +0x4c and
 *     +0x50 receive, +0x54 transmit, +0x58 receive.  Every buffer here is a
 *     DISTINCT array and each scripted state records the pointer it was
 *     handed, so any permutation of the seven shows up as a different offset
 *     in the log rather than as a silent pass.
 *   - BIT 0x01 NOT CLEARED, or cleared conditionally.  The object clears it on
 *     every call, whatever the mode.  Trials run every mode 0..6 with all
 *     eight bits of obj + 0x31 set.
 *   - BITS 0x04 AND 0x08 CLEARED UNCONDITIONALLY, or cleared for the wrong
 *     mode.  They go only when `hdx->mode` is 4, and the sweep over all seven
 *     modes is what separates that from "always" and from "never".
 *   - THE MASK APPLIED IN THE WRONG ORDER.  The object's two paths converge on
 *     one store, so mode 4 gets `& 0xf3` and then `& 0xfe`.  Starting from
 *     0xff makes the three surviving readings -- 0xf2, 0xfe and 0xf3 --
 *     distinct values.
 *   - THE COPY BOUND READ AS UNSIGNED.  `cmpw $0x0,0x9e(%ecx); jle` and
 *     `cmp %dx,0x9e(%ecx); jg` are both SIGNED, so a negative `symbol_len`
 *     copies NOTHING.  One trial sets it to -3 and the destination buffer is
 *     compared whole; an unsigned reading would copy 65533 words and run off
 *     the end of the fixture.
 *   - THE COPY LENGTH TAKEN FROM +0xa0 INSTEAD OF +0x9e.  The two are
 *     V32_SAMPLE_LEN and V32_SYMBOL_LEN and are never set equal here.
 *   - THE COPY WRITTEN AS A `while` RATHER THAN THE GUARD-PLUS-`do`.  Covered
 *     by the `symbol_len == 0` and `symbol_len < 0` trials, which must copy
 *     nothing at all.
 *   - `txdata` PASSED STRAIGHT TO THE TRANSMIT DRIVER.  It is copied into
 *     hdx + 0xa4 first, and the driver is handed the BUFFER.  The transmit
 *     script records its `data` pointer, and the two are different arrays.
 *   - THE TRANSMIT BUFFER POINTER TAKEN BEFORE THE RECEIVE CALL.  The object
 *     re-reads obj + 0x64 at 82ba3, AFTER the receive state has run, and only
 *     then loads +0xa4.  So a receive state that swaps the whole context makes
 *     the transmit driver run on the NEW context's buffer -- while the copy
 *     that just happened went into the OLD one.  One trial does exactly that
 *     and checks both buffers.
 *   - THE COPY DONE AFTER THE RECEIVE CALL.  Same trial, other direction: the
 *     old context's buffer must hold the copy and the new one must not.
 *
 * COMPARING SIDES.  The two contexts hold OUR function addresses and the
 * blob's, so the state-pointer words at hdx + 0x6c and + 0x70 differ for ever
 * and cannot be part of an object comparison.  Both sides are given the SAME
 * scripted functions -- these are test functions, not reconstructions, so
 * there is exactly one address for each -- which removes the problem entirely
 * rather than skipping a region.  What is compared is every other byte of the
 * instance, both contexts, and all four buffers.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32hdxst.h"
#include "dsplib/v32fpctl.h"

extern void ref_v32_handshake(void *modem, unsigned short *txdata,
			      short *txout, short *rxin, unsigned short *rxout,
			      short *nsamples, unsigned short *rxcount);

/*
 * Ours: FILE-LOCAL in the object, so v32_handshake is `static` in
 * v32fpdisp.c (which used to be v32hshake.c) and v32hdx.h no longer declares
 * it.  The test tier links a globalized copy (tools/testvisible.py).
 */
extern void v32_handshake(void *modem, unsigned short *txdata,
			  short *txout, short *rxin, unsigned short *rxout,
			  short *nsamples, unsigned short *rxcount);

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100
#define NBUF		64

#define FIELD(o, f)	((unsigned char *)(o) + (f))
#define PUT_PTR(o, f, v) (*(void **)(void *)FIELD((o), (f)) = (void *)(v))
#define PUT_S16(o, f, v) (*(short *)(void *)FIELD((o), (f)) = (short)(v))
#define GET_PTR(o, f)	(*(void **)(void *)FIELD((o), (f)))

/* Named images: diff_eq_obj stringifies its type for tools/whichfield.py. */
struct obj_image { unsigned char b[OBJ_SIZE]; };
struct hdx_image { unsigned char b[HDX_SIZE]; };
struct buf_image { unsigned short u[NBUF]; };
struct sbuf_image { short s[NBUF]; };

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	alt[HDX_SIZE];
	unsigned short	hdxbuf[NBUF];		/* hdx + 0xa4 points here     */
	unsigned short	altbuf[NBUF];		/* alt + 0xa4 points here     */
	unsigned short	txdata[NBUF];
	short		txout[NBUF];
	short		rxin[NBUF];
	unsigned short	rxout[NBUF];
	short		nsamples;
	unsigned short	rxcount;
	double		align;
};

static struct fix fa, fb;

/*
 * What the scripted states saw.  One entry per call; the transmit driver may
 * call its state more than once, so the transmit log is an array.
 */
struct rxlog {
	long	calls;
	long	obj_is_fix;	/* modem was this side's instance             */
	long	in_off;		/* in - fix->rxin                            */
	long	out_off;	/* out - fix->rxout                          */
	long	count_in;
};

struct txlog {
	long	calls;
	long	obj_is_fix;
	long	data_is_hdxbuf;	/* 1 hdxbuf, 2 altbuf, 3 txdata, 0 none       */
	long	out_off;
	long	left_in;
};

static struct rxlog rxl[2];
static struct txlog txl[2];

static int cur_side;
static struct fix *cur_fix;

/* Trial knobs, consumed by the scripted states. */
static int rx_swaps_context;
static unsigned short rx_writes_count;

/* ------------------------------------------------------------------------- */

static short
script_tx(void *modem, short *data, short *out, unsigned short *left)
{
	struct fix *f = cur_fix;
	struct txlog *l = &txl[cur_side];

	l->calls++;
	l->obj_is_fix = (modem == (void *)f->obj);
	if (data == (short *)(void *)f->hdxbuf)
		l->data_is_hdxbuf = 1;
	else if (data == (short *)(void *)f->altbuf)
		l->data_is_hdxbuf = 2;
	else if (data == (short *)(void *)f->txdata)
		l->data_is_hdxbuf = 3;
	else
		l->data_is_hdxbuf = 0;
	l->out_off = (long)(out - f->txout);
	l->left_in = (long)*left;

	*left = 0;		/* one iteration, then the driver stops       */
	return 3;
}

static void
script_rx(void *modem, short *in, unsigned short *out, unsigned short *count)
{
	struct fix *f = cur_fix;
	struct rxlog *l = &rxl[cur_side];

	l->calls++;
	l->obj_is_fix = (modem == (void *)f->obj);
	l->in_off = (long)(in - f->rxin);
	l->out_off = (long)(out - f->rxout);
	l->count_in = (long)*count;

	*count = rx_writes_count;

	if (rx_swaps_context)
		PUT_PTR(f->obj, V32_OBJ_HDX, f->alt);
}

/* ------------------------------------------------------------------------- */

static void
build(struct fix *f, unsigned char flags, int mode, short symbol_len,
      short sample_len)
{
	int i;

	memset(f, 0, sizeof(*f));

	for (i = 0; i < NBUF; i++) {
		f->hdxbuf[i] = (unsigned short)(0xa000 + i);
		f->altbuf[i] = (unsigned short)(0xb000 + i);
		f->txdata[i] = (unsigned short)(0xc000 + i);
		f->txout[i] = (short)(0xd00 + i);
		f->rxin[i] = (short)(0xe00 + i);
		f->rxout[i] = (unsigned short)(0xf000 + i);
	}
	f->nsamples = -1;
	f->rxcount = 17;

	PUT_PTR(f->obj, V32_OBJ_HDX, f->hdx);
	*FIELD(f->obj, V32_OBJ_FLAGS) = flags;

	PUT_PTR(f->hdx, V32HDX_TXSTATE, script_tx);
	PUT_PTR(f->hdx, V32HDX_RXSTATE, script_rx);
	PUT_S16(f->hdx, V32HDX_MODE, mode);
	PUT_S16(f->hdx, V32HDX_SYMBOL_LEN, symbol_len);
	PUT_S16(f->hdx, V32HDX_SAMPLE_LEN, sample_len);
	PUT_PTR(f->hdx, 0xa4, f->hdxbuf);

	/* The alternate context, for the swap trial. */
	PUT_PTR(f->alt, V32HDX_TXSTATE, script_tx);
	PUT_PTR(f->alt, V32HDX_RXSTATE, script_rx);
	PUT_S16(f->alt, V32HDX_MODE, mode);
	PUT_S16(f->alt, V32HDX_SYMBOL_LEN, symbol_len);
	PUT_S16(f->alt, V32HDX_SAMPLE_LEN, sample_len);
	PUT_PTR(f->alt, 0xa4, f->altbuf);
}

/*
 * `V32HDX_SYMBOL_LEN` is the transmit driver's budget as well as the copy's
 * length, and the scripted state zeroes `*left` on its first call, so the
 * driver always makes exactly one call whatever the budget was.
 */
static int
trial(unsigned char flags, int mode, short symbol_len, short sample_len,
      int swap, unsigned short newcount, long input)
{
	memset(rxl, 0, sizeof(rxl));
	memset(txl, 0, sizeof(txl));

	rx_swaps_context = swap;
	rx_writes_count = newcount;

	build(&fa, flags, mode, symbol_len, sample_len);
	cur_side = 0;
	cur_fix = &fa;
	v32_handshake(fa.obj, fa.txdata, fa.txout, fa.rxin, fa.rxout,
		      &fa.nsamples, &fa.rxcount);

	build(&fb, flags, mode, symbol_len, sample_len);
	cur_side = 1;
	cur_fix = &fb;
	ref_v32_handshake(fb.obj, fb.txdata, fb.txout, fb.rxin, fb.rxout,
			  &fb.nsamples, &fb.rxcount);

	/*
	 * obj + 0x64 holds a pointer into each side's own fixture, so it can
	 * never be equal; it is checked by IDENTITY below instead, and zeroed
	 * before the whole-object comparison.
	 */
	diff_eq_int("obj->hdx points at the expected context (%ld)",
		    GET_PTR(fa.obj, V32_OBJ_HDX) ==
		    (void *)(swap ? fa.alt : fa.hdx), 1, input);
	diff_eq_int("ref obj->hdx points at the expected context (%ld)",
		    GET_PTR(fb.obj, V32_OBJ_HDX) ==
		    (void *)(swap ? fb.alt : fb.hdx), 1, input);
	PUT_PTR(fa.obj, V32_OBJ_HDX, 0);
	PUT_PTR(fb.obj, V32_OBJ_HDX, 0);

	/*
	 * hdx + 0xa4 likewise, and + 0x6c / + 0x70 hold the SAME address on
	 * both sides (one scripted function, not two reconstructions), so
	 * those need no special handling.
	 */
	PUT_PTR(fa.hdx, 0xa4, 0);
	PUT_PTR(fb.hdx, 0xa4, 0);
	PUT_PTR(fa.alt, 0xa4, 0);
	PUT_PTR(fb.alt, 0xa4, 0);

	diff_eq_obj("the instance", struct obj_image, fa.obj, fb.obj, input);
	diff_eq_obj("the context", struct hdx_image, fa.hdx, fb.hdx, input);
	diff_eq_obj("the alternate context", struct hdx_image, fa.alt, fb.alt,
		    input);
	diff_eq_obj("the context's transmit buffer", struct buf_image,
		    fa.hdxbuf, fb.hdxbuf, input);
	diff_eq_obj("the alternate's transmit buffer", struct buf_image,
		    fa.altbuf, fb.altbuf, input);
	diff_eq_obj("the caller's transmit words", struct buf_image, fa.txdata,
		    fb.txdata, input);
	diff_eq_obj("the transmit output", struct sbuf_image, fa.txout,
		    fb.txout, input);
	diff_eq_obj("the receive input", struct sbuf_image, fa.rxin, fb.rxin,
		    input);
	diff_eq_obj("the receive output", struct buf_image, fa.rxout, fb.rxout,
		    input);

	diff_eq_int("*nsamples (%ld)", fa.nsamples, fb.nsamples, input);
	diff_eq_int("*rxcount (%ld)", fa.rxcount, fb.rxcount, input);

	diff_eq_int("receive state call count (%ld)", rxl[0].calls,
		    rxl[1].calls, input);
	diff_eq_int("receive state got this instance (%ld)", rxl[0].obj_is_fix,
		    rxl[1].obj_is_fix, input);
	diff_eq_int("receive state's `in` (%ld)", rxl[0].in_off, rxl[1].in_off,
		    input);
	diff_eq_int("receive state's `out` (%ld)", rxl[0].out_off,
		    rxl[1].out_off, input);
	diff_eq_int("receive state's `*count` on entry (%ld)", rxl[0].count_in,
		    rxl[1].count_in, input);

	diff_eq_int("transmit state call count (%ld)", txl[0].calls,
		    txl[1].calls, input);
	diff_eq_int("transmit state got this instance (%ld)", txl[0].obj_is_fix,
		    txl[1].obj_is_fix, input);
	diff_eq_int("which buffer the transmit state was handed (%ld)",
		    txl[0].data_is_hdxbuf, txl[1].data_is_hdxbuf, input);
	diff_eq_int("transmit state's `out` (%ld)", txl[0].out_off,
		    txl[1].out_off, input);
	diff_eq_int("transmit state's `*left` on entry (%ld)", txl[0].left_in,
		    txl[1].left_in, input);

	return 0;
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int mode;
	int rc = 0;

	/*
	 * The mode sweep, with every bit of obj + 0x31 set so all three masks
	 * are observable.  Mode 4 must lose 0x0d; every other mode only 0x01.
	 */
	diff_begin("v32_handshake: the mode sweep and the flag masks");
	for (mode = 0; mode <= 6; mode++)
		trial(0xff, mode, 4, 12, 0, 5, mode);
	/* And from zero, so a mask that SETS a bit shows up as well. */
	for (mode = 0; mode <= 6; mode++)
		trial(0x00, mode, 4, 12, 0, 5, 0x100 + mode);
	rc |= diff_end();

	diff_begin("v32_handshake: the copy's length");
	trial(0xff, 0, 0, 12, 0, 5, 0x200);	/* nothing copied            */
	trial(0xff, 0, 1, 12, 0, 5, 0x201);
	trial(0xff, 0, 8, 12, 0, 5, 0x202);
	trial(0xff, 0, NBUF, 12, 0, 5, 0x203);	/* the whole buffer          */
	trial(0xff, 0, -3, 12, 0, 5, 0x204);	/* SIGNED: copies nothing    */
	trial(0xff, 4, -1, 12, 0, 5, 0x205);
	/*
	 * `sample_len` is varied independently and is never equal to
	 * `symbol_len`, so a copy bound taken from + 0xa0 copies the wrong
	 * number of words.
	 */
	trial(0xff, 0, 2, 40, 0, 5, 0x206);
	trial(0xff, 0, 40, 2, 0, 5, 0x207);
	rc |= diff_end();

	diff_begin("v32_handshake: the context swap inside the receive state");
	trial(0xff, 0, 4, 12, 1, 5, 0x300);
	trial(0xff, 4, 6, 12, 1, 9, 0x301);
	trial(0x00, 2, 0, 12, 1, 0, 0x302);
	rc |= diff_end();

	/*
	 * The separating trials: assert that the fixture actually reached each
	 * behaviour, so a silently inert script cannot report a clean run.
	 * A detector must report its denominator (findings F134, F2400).
	 */
	diff_begin("v32_handshake separating trials");
	rx_swaps_context = 0;
	rx_writes_count = 5;
	build(&fa, 0xff, 4, 4, 12);
	cur_side = 0;
	cur_fix = &fa;
	memset(txl, 0, sizeof(txl));
	memset(rxl, 0, sizeof(rxl));
	v32_handshake(fa.obj, fa.txdata, fa.txout, fa.rxin, fa.rxout,
		      &fa.nsamples, &fa.rxcount);
	diff_eq_int("mode 4 cleared 0x0c as well as 0x01 (got 0x%lx)",
		    *FIELD(fa.obj, V32_OBJ_FLAGS), 0xf2,
		    *FIELD(fa.obj, V32_OBJ_FLAGS));
	diff_eq_int("the copy reached the context's buffer (%ld)",
		    fa.hdxbuf[0], fa.txdata[0], 0);
	diff_eq_int("the copy stopped at symbol_len (%ld)", fa.hdxbuf[4],
		    (unsigned short)(0xa000 + 4), 4);
	diff_eq_int("the transmit state was handed the CONTEXT's buffer (%ld)",
		    txl[0].data_is_hdxbuf, 1, 0);
	diff_eq_int("the receive state ran (%ld)", rxl[0].calls, 1, 0);

	build(&fa, 0xff, 0, 4, 12);
	cur_side = 0;
	cur_fix = &fa;
	memset(txl, 0, sizeof(txl));
	v32_handshake(fa.obj, fa.txdata, fa.txout, fa.rxin, fa.rxout,
		      &fa.nsamples, &fa.rxcount);
	diff_eq_int("a mode other than 4 cleared only 0x01 (got 0x%lx)",
		    *FIELD(fa.obj, V32_OBJ_FLAGS), 0xfe,
		    *FIELD(fa.obj, V32_OBJ_FLAGS));

	rx_swaps_context = 1;
	build(&fa, 0xff, 0, 4, 12);
	cur_side = 0;
	cur_fix = &fa;
	memset(txl, 0, sizeof(txl));
	v32_handshake(fa.obj, fa.txdata, fa.txout, fa.rxin, fa.rxout,
		      &fa.nsamples, &fa.rxcount);
	diff_eq_int("a swapped context sent the transmit driver elsewhere (%ld)",
		    txl[0].data_is_hdxbuf, 2, 0);
	diff_eq_int("and the copy still went to the OLD buffer (%ld)",
		    fa.hdxbuf[0], fa.txdata[0], 0);
	diff_eq_int("and NOT to the new one (%ld)", fa.altbuf[0],
		    (unsigned short)0xb000, 0);
	rx_swaps_context = 0;

	build(&fa, 0xff, 0, -3, 12);
	cur_side = 0;
	cur_fix = &fa;
	v32_handshake(fa.obj, fa.txdata, fa.txout, fa.rxin, fa.rxout,
		      &fa.nsamples, &fa.rxcount);
	diff_eq_int("a negative symbol_len copied nothing (%ld)", fa.hdxbuf[0],
		    (unsigned short)0xa000, 0);
	rc |= diff_end();

	return rc;
}
