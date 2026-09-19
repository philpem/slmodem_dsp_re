/*
 * t_v32hdx.c -- differential test of V.32's two half-duplex drivers.
 *
 * NEITHER DRIVER NEEDS A REAL STATE TO BE TESTED, and that is the whole point
 * of this file: both are pure dispatchers, so a SCRIPTED state installed into
 * the blob's context and into ours exercises everything they do while being
 * a fixture rather than a reconstruction.  The twenty real `TxHdx*` /
 * `RxHdx*` states do not exist yet and are not needed.
 *
 * The script is consumed one step per call, so the two sides see the same
 * sequence: side 0 (ours) runs first and its trace is recorded, the fixture
 * is rebuilt, side 1 (the blob) runs, and the traces, the two contexts, the
 * two output buffers and the reported sample count are compared.
 *
 * WHAT THE SCRIPTED STATE IS FOR.  Every check is built around a NAMED WRONG
 * READING, and the fixture exists to make each one reach an output:
 *
 *   - `hdx` cached outside the loop.  The object reloads obj + 0x64 at the
 *     loop's back-edge target (7fd06), so a step can swap the WHOLE context
 *     and the next iteration must dispatch from the new one.  A cached
 *     driver calls the old context's state pointer.
 *   - the state pointer cached.  A step can install a different function at
 *     hdx + 0x6c and the next iteration must run it.  This is how a real
 *     transition mid-block works.
 *   - `out` not advanced, or advanced by `n` bytes instead of `n` shorts.
 *     Each call writes a marker run at its own `out`, so the buffer's
 *     contents place every call.
 *   - the total accumulated in an `int`.  The object truncates to a `short`
 *     every iteration (`lea` then `movswl %dx`), so a run whose partial sums
 *     pass 0x7fff separates the two.
 *   - the loop seeded from hdx + 0xa0 instead of hdx + 0x9e.  The two are
 *     V32_SAMPLE_LEN and V32_SYMBOL_LEN and are never set equal here.
 *   - the loop written `while` instead of `do`/`while`.  Seeding +0x9e with
 *     zero must still call the state once.
 *   - the state's return zero-extended instead of sign-extended (`cwtl`).
 *     One trial returns a negative count, which walks `out` backwards over
 *     the guard region below the buffer.
 *   - the receive driver dispatching from hdx + 0x6c rather than + 0x70.
 *     The two hold different functions throughout.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32hdx.h"

extern void ref_V32TxHdxModem(void *modem, short *data, short *out,
			      short *nsamples);
extern void ref_V32RxHdxModem(void *modem, short *in, unsigned short *out,
			      unsigned short *count);

#define OBJ_SIZE	0x80
#define HDX_SIZE	0x100

/*
 * Headroom below `out` so a negative advance stays inside the array, and
 * enough above it for the 33,600 shorts the accumulator-wrap trial advances
 * through.  Both are pointer arithmetic the object really performs.
 */
#define GUARD		4096
#define NOUT		(GUARD + 40960)

#define MAXCALL		64

/* --------------------------------------------------------------------- */

struct txstep {
	short		ret;		/* samples the state claims to write  */
	unsigned short	take;		/* subtracted from *left, floored at 0*/
	int		swap_ctx;	/* install the alternate context      */
	int		install;	/* -1 none, else the state to install */
	int		write;		/* fill `ret` markers at `out`        */
};

struct txcall {
	long	id;		/* which state function ran               */
	long	out_off;	/* out - (obuf + GUARD)                   */
	long	data_off;	/* data - dbuf                            */
	long	left_in;	/* *left on entry                         */
	long	ctx_alt;	/* obj + 0x64 pointed at the alternate    */
	long	obj_ok;		/* modem was this side's instance         */
};

struct rxcall {
	long	id;
	long	in_off;
	long	out_off;
	long	count_in;
	long	obj_ok;
};

/*
 * Named images, because `diff_eq_obj` stringifies its type for
 * `tools/whichfield.py` and an anonymous struct resolves to nothing.
 */
struct hdx_image { unsigned char b[HDX_SIZE]; };
struct out_image { short s[NOUT]; };
struct uout_image { unsigned short u[NOUT]; };

struct fix {
	unsigned char	obj[OBJ_SIZE];
	unsigned char	hdx[HDX_SIZE];
	unsigned char	alt[HDX_SIZE];	/* the swap target                    */
	short		dbuf[256];
	short		obuf[NOUT];
	unsigned short	ubuf[NOUT];
	double		align;
};

static struct fix fa, fb;

static struct v32_modem *
modem_of(struct fix *f)
{
	return (struct v32_modem *)(void *)f->obj;
}

static const struct txstep *script;
static int script_len;
static int script_pos;

static int cur_side;			/* 0 = ours, 1 = the blob             */
static struct fix *cur_fix;

static struct txcall txlog[2][MAXCALL];
static int txn[2];
static struct rxcall rxlog[2][MAXCALL];
static int rxn[2];

/*
 * `diff_begin` ZEROES the failure count, so a section whose `diff_end` is
 * discarded reports FAIL and exits 0.  Every section accumulates here.
 */
static int rc_total;

/* Non-vacuity counters: every one is asserted non-zero in main(). */
static long sep_multi_iter;
static long sep_ctx_swap;
static long sep_state_swap;
static long sep_short_wrap;
static long sep_negative_ret;
static long sep_zero_seed;
static long sep_len_differ;
static long sep_out_advanced;
static long sep_total_zero;
static long rx_called;
static long rx_count_changed;

/* --------------------------------------------------------------------- */

static void
put_ptr(unsigned char *p, int off, void *v)
{
	*(void **)(void *)(p + off) = v;
}

static void
put_short(unsigned char *p, int off, short v)
{
	*(short *)(void *)(p + off) = v;
}

static void *
get_ptr(const unsigned char *p, int off)
{
	return *(void *const *)(const void *)(p + off);
}

/* --------------------------------------------------------------------- */

static short tx_state_0(struct v32_modem *modem, short *data, short *out,
			unsigned short *left);
static short tx_state_1(struct v32_modem *modem, short *data, short *out,
			unsigned short *left);

static const v32_txhdx_fn tx_states[2] = { tx_state_0, tx_state_1 };

static short
tx_body(int id, struct v32_modem *modem, short *data, short *out,
	unsigned short *left)
{
	const struct txstep *st;
	struct txcall *c;
	int i;

	if (txn[cur_side] < MAXCALL) {
		c = &txlog[cur_side][txn[cur_side]];
		c->id = id;
		c->out_off = (long)(out - (cur_fix->obuf + GUARD));
		c->data_off = (long)(data - cur_fix->dbuf);
		c->left_in = (long)*left;
		c->ctx_alt = get_ptr(cur_fix->obj, V32_OBJ_HDX)
				== (void *)cur_fix->alt;
		c->obj_ok = (modem == (void *)cur_fix->obj);
	}
	txn[cur_side]++;

	/*
	 * Past the script the state stops the loop.  A driver that runs away
	 * would otherwise hang the suite rather than fail it.
	 */
	if (script_pos >= script_len) {
		*left = 0;
		return 0;
	}
	st = &script[script_pos++];

	if (st->write && st->ret > 0) {
		for (i = 0; i < st->ret; i++)
			out[i] = (short)(0x100 * (script_pos & 0x7f)
					 + (i & 0xff));
	}

	if (st->swap_ctx) {
		void *hdx = get_ptr(cur_fix->obj, V32_OBJ_HDX);
		void *other = hdx == (void *)cur_fix->hdx
				? (void *)cur_fix->alt : (void *)cur_fix->hdx;

		put_ptr(cur_fix->obj, V32_OBJ_HDX, other);
	}
	if (st->install >= 0) {
		unsigned char *hdx = get_ptr(cur_fix->obj, V32_OBJ_HDX);

		put_ptr(hdx, V32HDX_TXSTATE, (void *)tx_states[st->install]);
	}

	*left = (unsigned short)(*left > st->take ? *left - st->take : 0);
	return st->ret;
}

static short
tx_state_0(struct v32_modem *modem, short *data, short *out,
	   unsigned short *left)
{
	return tx_body(0, modem, data, out, left);
}

static short
tx_state_1(struct v32_modem *modem, short *data, short *out,
	   unsigned short *left)
{
	return tx_body(1, modem, data, out, left);
}

/* --------------------------------------------------------------------- */

static unsigned short rx_new_count;
static int rx_write_n;

static void
rx_body(int id, void *modem, short *in, unsigned short *out,
	unsigned short *count)
{
	struct rxcall *c;
	int i;

	if (rxn[cur_side] < MAXCALL) {
		c = &rxlog[cur_side][rxn[cur_side]];
		c->id = id;
		c->in_off = (long)(in - cur_fix->dbuf);
		c->out_off = (long)(out - cur_fix->ubuf);
		c->count_in = (long)*count;
		c->obj_ok = (modem == (void *)cur_fix->obj);
	}
	rxn[cur_side]++;

	for (i = 0; i < rx_write_n; i++)
		out[i] = (unsigned short)(0x2000 + i + id);
	*count = rx_new_count;
}

static void
rx_state_0(void *modem, short *in, unsigned short *out, unsigned short *count)
{
	rx_body(0, modem, in, out, count);
}

static void
rx_state_1(void *modem, short *in, unsigned short *out, unsigned short *count)
{
	rx_body(1, modem, in, out, count);
}

/* --------------------------------------------------------------------- */

/*
 * `alt_state` is what the ALTERNATE context dispatches, so a driver that
 * swaps contexts and one that does not call different functions.
 */
static void
fixture(struct fix *f, unsigned seed, short symlen, short samplen,
	int primary, int alt_state, int rx_primary)
{
	unsigned s = seed ? seed : 1u;
	int i;

	memset(f, 0, sizeof(*f));

	for (i = 0; i < (int)(sizeof(f->dbuf) / sizeof(f->dbuf[0])); i++) {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		f->dbuf[i] = (short)(s & 0x3fff);
	}
	for (i = 0; i < NOUT; i++) {
		f->obuf[i] = (short)0xa5a5;
		f->ubuf[i] = 0x5a5a;
	}

	put_ptr(f->obj, V32_OBJ_HDX, f->hdx);

	put_ptr(f->hdx, V32HDX_TXSTATE, (void *)tx_states[primary]);
	put_ptr(f->hdx, V32HDX_RXSTATE,
		(void *)(rx_primary ? rx_state_1 : rx_state_0));
	put_short(f->hdx, V32HDX_SYMBOL_LEN, symlen);
	put_short(f->hdx, V32HDX_SAMPLE_LEN, samplen);

	/*
	 * The alternate context: a DIFFERENT transmit state and a different
	 * receive state, so dispatching from the wrong context is visible in
	 * the trace rather than only in a pointer.
	 */
	put_ptr(f->alt, V32HDX_TXSTATE, (void *)tx_states[alt_state]);
	put_ptr(f->alt, V32HDX_RXSTATE, (void *)rx_state_1);
	put_short(f->alt, V32HDX_SYMBOL_LEN, (short)(symlen ^ 0x55));
	put_short(f->alt, V32HDX_SAMPLE_LEN, (short)(samplen ^ 0x33));
}

/* --------------------------------------------------------------------- */

/*
 * The instance byte for byte EXCEPT its context pointer, which holds a
 * different address on each side and always will -- CLAUDE.md's exception for
 * a region that must be skipped.  Which of the two contexts it names is
 * compared as a flag in the trace instead.
 */
static void
compare_obj(void)
{
	int i;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (i >= V32_OBJ_HDX && i < V32_OBJ_HDX + (int)sizeof(void *))
			continue;
		diff_eq_int("instance byte %ld", fa.obj[i], fb.obj[i], i);
	}
	diff_eq_int("the context pointer names the same context (%ld)",
		    get_ptr(fa.obj, V32_OBJ_HDX) == (void *)fa.alt,
		    get_ptr(fb.obj, V32_OBJ_HDX) == (void *)fb.alt, 0);
}

static void
run_tx_one(const char *what, unsigned seed, short symlen, short samplen,
	   int primary, int alt_state, const struct txstep *steps, int nsteps)
{
	short na = (short)0x7777, nb = (short)0x7777;
	long total_abs = 0;
	int i;

	script = steps;
	script_len = nsteps;

	txn[0] = txn[1] = 0;
	memset(txlog, 0, sizeof(txlog));

	fixture(&fa, seed, symlen, samplen, primary, alt_state, 0);
	script_pos = 0;
	cur_side = 0;
	cur_fix = &fa;
	V32TxHdxModem(modem_of(&fa), fa.dbuf, fa.obuf + GUARD, &na);

	fixture(&fb, seed, symlen, samplen, primary, alt_state, 0);
	script_pos = 0;
	cur_side = 1;
	cur_fix = &fb;
	ref_V32TxHdxModem(fb.obj, fb.dbuf, fb.obuf + GUARD, &nb);

	diff_begin(what);

	diff_eq_int("call count (%ld)", txn[0], txn[1], txn[0]);
	for (i = 0; i < txn[0] && i < MAXCALL; i++) {
		diff_eq_int("call %ld: which state", txlog[0][i].id,
			    txlog[1][i].id, i);
		diff_eq_int("call %ld: out offset", txlog[0][i].out_off,
			    txlog[1][i].out_off, i);
		diff_eq_int("call %ld: data pointer", txlog[0][i].data_off,
			    txlog[1][i].data_off, i);
		diff_eq_int("call %ld: *left on entry", txlog[0][i].left_in,
			    txlog[1][i].left_in, i);
		diff_eq_int("call %ld: context in use", txlog[0][i].ctx_alt,
			    txlog[1][i].ctx_alt, i);
		diff_eq_int("call %ld: instance pointer", txlog[0][i].obj_ok,
			    1, i);
		diff_eq_int("call %ld: instance pointer (blob)",
			    txlog[1][i].obj_ok, 1, i);
	}

	diff_eq_int("reported sample count (%ld)", na, nb, na);

	diff_eq_obj("the output buffer", struct out_image, fa.obuf, fb.obuf,
		    0);
	diff_eq_obj("the context", struct hdx_image, fa.hdx, fb.hdx, 0);
	diff_eq_obj("the alternate context", struct hdx_image, fa.alt, fb.alt,
		    0);
	compare_obj();

	/* Non-vacuity: what did this trial actually exercise? */
	if (txn[0] > 1)
		sep_multi_iter++;
	if (symlen != samplen)
		sep_len_differ++;
	if (symlen == 0)
		sep_zero_seed++;
	for (i = 0; i < txn[0] && i < MAXCALL; i++) {
		if (txlog[0][i].ctx_alt)
			sep_ctx_swap++;
		if (txlog[0][i].id != txlog[0][0].id)
			sep_state_swap++;
		if (txlog[0][i].out_off != 0)
			sep_out_advanced++;
		if (txlog[0][i].out_off < 0)
			sep_negative_ret++;
	}
	{
		long run = 0;
		int k;

		for (k = 0; k < nsteps; k++) {
			run += steps[k].ret;
			total_abs += steps[k].ret;
			if (run == 0 && k + 1 < nsteps)
				sep_total_zero++;
		}
	}
	if (total_abs > 0x7fff)
		sep_short_wrap++;

	rc_total |= diff_end();
}

/* --------------------------------------------------------------------- */

static void
run_rx_one(const char *what, unsigned seed, int rx_primary,
	   unsigned short count_in, unsigned short new_count, int writes)
{
	unsigned short ca = count_in, cb = count_in;
	int i;

	rxn[0] = rxn[1] = 0;
	memset(rxlog, 0, sizeof(rxlog));
	rx_new_count = new_count;
	rx_write_n = writes;

	fixture(&fa, seed, 7, 11, 0, 1, rx_primary);
	cur_side = 0;
	cur_fix = &fa;
	V32RxHdxModem(modem_of(&fa), fa.dbuf, fa.ubuf, &ca);

	fixture(&fb, seed, 7, 11, 0, 1, rx_primary);
	cur_side = 1;
	cur_fix = &fb;
	ref_V32RxHdxModem(fb.obj, fb.dbuf, fb.ubuf, &cb);

	diff_begin(what);

	diff_eq_int("call count (%ld)", rxn[0], rxn[1], rxn[0]);
	for (i = 0; i < rxn[0] && i < MAXCALL; i++) {
		diff_eq_int("call %ld: which state", rxlog[0][i].id,
			    rxlog[1][i].id, i);
		diff_eq_int("call %ld: in pointer", rxlog[0][i].in_off,
			    rxlog[1][i].in_off, i);
		diff_eq_int("call %ld: out pointer", rxlog[0][i].out_off,
			    rxlog[1][i].out_off, i);
		diff_eq_int("call %ld: *count on entry", rxlog[0][i].count_in,
			    rxlog[1][i].count_in, i);
		diff_eq_int("call %ld: instance pointer", rxlog[0][i].obj_ok,
			    1, i);
		diff_eq_int("call %ld: instance pointer (blob)",
			    rxlog[1][i].obj_ok, 1, i);
	}
	diff_eq_int("*count after (%ld)", ca, cb, ca);
	diff_eq_obj("the output buffer", struct uout_image, fa.ubuf, fb.ubuf,
		    0);
	diff_eq_obj("the context", struct hdx_image, fa.hdx, fb.hdx, 0);
	compare_obj();

	if (rxn[0] > 0)
		rx_called++;
	if (ca != count_in)
		rx_count_changed++;

	rc_total |= diff_end();
}

/* --------------------------------------------------------------------- */

/* One call, then stop.  The plainest shape there is. */
static const struct txstep s_single[] = {
	{ 40, 12, 0, -1, 1 }
};

/* Four calls, each writing and each taking a slice of the budget. */
static const struct txstep s_four[] = {
	{ 40, 3, 0, -1, 1 },
	{ 24, 3, 0, -1, 1 },
	{ 16, 3, 0, -1, 1 },
	{  8, 3, 0, -1, 1 }
};

/*
 * A context swap on the first call.  The alternate context dispatches the
 * OTHER state, so a driver that cached obj + 0x64 calls the wrong function.
 */
static const struct txstep s_swapctx[] = {
	{ 12, 2, 1, -1, 1 },
	{ 12, 2, 0, -1, 1 },
	{ 12, 9, 1, -1, 1 },
	{ 12, 9, 0, -1, 1 }
};

/* A state that installs its successor at hdx + 0x6c, twice. */
static const struct txstep s_install[] = {
	{ 10, 1, 0,  1, 1 },
	{ 10, 1, 0,  0, 1 },
	{ 10, 1, 0,  1, 1 },
	{ 10, 40, 0, -1, 1 }
};

/*
 * Partial sums past 0x7fff.  700 * 48 = 33,600, so an `int` accumulator and
 * a `short` one report different totals.  No writes: 33,600 shorts would run
 * off the buffer, and it is the ACCUMULATOR being tested, not the output.
 */
static const struct txstep s_wrap[] = {
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 },
	{ 700, 0, 0, -1, 1 }, { 700, 0, 0, -1, 1 }, { 700, 60, 0, -1, 1 }
};

/*
 * A negative return.  `cwtl` sign-extends, so `out` walks BACKWARDS by 2*n
 * bytes and the running total goes down; zero-extension would add 65,236 to
 * the pointer instead and leave the buffer.  The guard region below `out`
 * absorbs it, and no marker is written on that step.
 */
static const struct txstep s_negative[] = {
	{  30, 2, 0, -1, 1 },
	{ -20, 2, 0, -1, 0 },
	{  10, 2, 0, -1, 1 },
	{ -50, 2, 0, -1, 0 },
	{  16, 9, 0, -1, 1 }
};

/*
 * The running total returns to exactly zero on the second call while the
 * budget is still unspent, which is the only shape that separates the loop's
 * real condition from one that also watches the total.
 */
static const struct txstep s_zerototal[] = {
	{  10, 1, 0, -1, 1 },
	{ -10, 1, 0, -1, 0 },
	{  14, 1, 0, -1, 1 },
	{   6, 40, 0, -1, 1 }
};

/* The budget already exhausted: a `while` loop would not call at all. */
static const struct txstep s_zero[] = {
	{ 20, 5, 0, -1, 1 }
};

/*
 * A state that never touches `*left`.  The driver must keep calling; the
 * script's own backstop ends it, and the call count is the evidence.
 */
static const struct txstep s_notake[] = {
	{ 4, 0, 0, -1, 1 }, { 4, 0, 0, -1, 1 }, { 4, 0, 0, -1, 1 },
	{ 4, 0, 0, -1, 1 }, { 4, 0, 0, -1, 1 }, { 4, 0, 0, -1, 1 }
};

/* --------------------------------------------------------------------- */

#define NSTEP(a)	((int)(sizeof(a) / sizeof((a)[0])))

static void
run_tx(void)
{
	run_tx_one("V32TxHdxModem: one call", 0x11111111u, 12, 40, 0, 1,
		   s_single, NSTEP(s_single));
	run_tx_one("V32TxHdxModem: four calls", 0x22222222u, 12, 40, 0, 1,
		   s_four, NSTEP(s_four));
	run_tx_one("V32TxHdxModem: four calls, other primary", 0x23232323u,
		   48, 160, 1, 0, s_four, NSTEP(s_four));
	run_tx_one("V32TxHdxModem: the context is swapped mid-block",
		   0x33333333u, 12, 40, 0, 1, s_swapctx, NSTEP(s_swapctx));
	run_tx_one("V32TxHdxModem: a state installs its successor",
		   0x44444444u, 48, 160, 0, 1, s_install, NSTEP(s_install));
	run_tx_one("V32TxHdxModem: the total passes 0x7fff", 0x55555555u,
		   48, 160, 0, 1, s_wrap, NSTEP(s_wrap));
	run_tx_one("V32TxHdxModem: a state returns a negative count",
		   0x66666666u, 12, 40, 0, 1, s_negative, NSTEP(s_negative));
	run_tx_one("V32TxHdxModem: the running total passes through zero",
		   0x7a7a7a7au, 12, 40, 0, 1, s_zerototal,
		   NSTEP(s_zerototal));
	run_tx_one("V32TxHdxModem: the budget is already zero", 0x77777777u,
		   0, 40, 0, 1, s_zero, NSTEP(s_zero));
	run_tx_one("V32TxHdxModem: the state never spends the budget",
		   0x88888888u, 12, 40, 0, 1, s_notake, NSTEP(s_notake));
	/* +0x9e and +0xa0 transposed would seed 12 instead of 48 here. */
	run_tx_one("V32TxHdxModem: symbol and sample lengths far apart",
		   0x99999999u, 48, 12, 0, 1, s_four, NSTEP(s_four));
}

static void
run_rx(void)
{
	run_rx_one("V32RxHdxModem: the primary state", 0xabcdef01u, 0, 40,
		   17, 24);
	run_rx_one("V32RxHdxModem: the other state", 0xabcdef02u, 1, 40, 3,
		   8);
	run_rx_one("V32RxHdxModem: a zero count in", 0xabcdef03u, 0, 0, 9, 4);
	run_rx_one("V32RxHdxModem: the count is left alone", 0xabcdef04u, 1,
		   55, 55, 0);
	run_rx_one("V32RxHdxModem: a large count", 0xabcdef05u, 0, 0xfffe,
		   0x8001, 12);
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	run_tx();
	run_rx();

	diff_begin("v32hdx separating trials");
	diff_eq_int("the loop ran more than once (%ld)", sep_multi_iter > 0,
		    1, sep_multi_iter);
	diff_eq_int("a mid-block context swap was dispatched (%ld)",
		    sep_ctx_swap > 0, 1, sep_ctx_swap);
	diff_eq_int("a state installed its successor (%ld)",
		    sep_state_swap > 0, 1, sep_state_swap);
	diff_eq_int("the running total passed 0x7fff (%ld)",
		    sep_short_wrap > 0, 1, sep_short_wrap);
	diff_eq_int("a negative return walked `out` backwards (%ld)",
		    sep_negative_ret > 0, 1, sep_negative_ret);
	diff_eq_int("the budget was seeded at zero (%ld)", sep_zero_seed > 0,
		    1, sep_zero_seed);
	diff_eq_int("symbol and sample lengths differed (%ld)",
		    sep_len_differ > 0, 1, sep_len_differ);
	diff_eq_int("`out` advanced between calls (%ld)", sep_out_advanced > 0,
		    1, sep_out_advanced);
	diff_eq_int("the running total passed through zero mid-block (%ld)",
		    sep_total_zero > 0, 1, sep_total_zero);
	diff_eq_int("the receive state ran (%ld)", rx_called > 0, 1,
		    rx_called);
	diff_eq_int("the receive state rewrote *count (%ld)",
		    rx_count_changed > 0, 1, rx_count_changed);
	rc |= diff_end();

	return rc | rc_total;
}
