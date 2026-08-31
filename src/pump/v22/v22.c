/*
 * v22.c -- V.22 / V.22bis / Bell 212: the datapump's core-facing half.
 *
 * The counterpart of src/pump/b103/b103.c, and it now holds the same five
 * things that file does:
 *
 *   v22_create    .text 0x004fb0  300 bytes, LOCAL
 *   v22_delete    .text 0x0050e0   72 bytes, LOCAL
 *   v22_process   .text 0x005130  557 bytes, LOCAL
 *   dp_v22_init   .text 0x005360   72 bytes, GLOBAL
 *   dp_v22_exit   .text 0x0053b0   70 bytes, GLOBAL
 *   v22_ops       .data  0x000078   24 bytes, LOCAL
 *
 * ---------------------------------------------------------------------------
 * THREE OF THE FIVE ARE FILE-STATIC IN THE OBJECT, AND THAT IS NOT COSMETIC
 *
 * `nm` shows a lower-case `t` for `v22_create`, `v22_delete` and
 * `v22_process` and an upper-case `T` for the registration pair -- exactly
 * b103.c's split, from the same author.  `static` is reproduced because it is
 * CODEGEN-VISIBLE: GCC 3.4 gives a static function `regparm(2)` when it can
 * see every call site, so declaring one of these `extern` moves the calling
 * convention away from the object's (CLAUDE.md, finding F8462).
 *
 * `v22_delete` was committed GLOBAL in an earlier wave, when nothing else in
 * this file existed and a test had to be able to name it; it is static now,
 * and `t_v22dp.c` reaches all three the way the modem core does -- out of the
 * operations table `dp_v22_init` registers.  Finding F8121 is the same move
 * for `b103_ops`.
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
 *
 * ---------------------------------------------------------------------------
 * ONE BUFFER, TWO WIDTHS -- AND THE V.22 VERSION IS NOT B.103'S
 *
 * `modem_get_bits` and `modem_put_bits` deal in one BYTE per data word;
 * `V22FP_modem` deals in one INT.  The same buffer serves both, widened in
 * place and walked BACKWARDS so that writing element i as an int cannot
 * clobber element i+1 as a byte.  Two differences from `b103_process`:
 *
 *   - the widening loop starts at `n - 1`, not at `n`.  The object's is
 *     `mov %eax,%edx; jmp .test; .body: ...; .test: dec %edx; jns .body`,
 *     which runs for n-1 down to 0.  b103's really does start at n; the two
 *     are different loops in the same shape and both are reproduced as
 *     written.
 *   - the second argument to both calls is `bits_per_word`, not a literal
 *     channel number.  slmodemd spells that parameter `nbits`, and V.22
 *     carries two or four bits per symbol where Bell 103 carries one -- so
 *     b103's literal `1` and this field are the same quantity.
 *
 * And `modem_put_bits` is called UNCONDITIONALLY, even with a count of zero:
 * when `tx_bits_wanted` is zero the object sets the receive count to zero and
 * falls into the same call rather than branching around it.
 */

#include "dsplib/v22.h"

#include "dsplib/debug.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22conn.h"
#include "dsplib/v22fp.h"

/* Provided by the modem core (slmodemd/modem.c), as for b103.c. */

extern int modem_dp_register(int id, void *op);
extern void modem_dp_deregister(int id, void *op);

extern int modem_get_bits(void *modem, int nbits, unsigned char *bits,
			  int count);
extern int modem_put_bits(void *modem, int nbits, const unsigned char *bits,
			  int count);

/*
 * The layout, asserted rather than commented.  `v22.h` derives the bit
 * buffers' length from the object's own allocation size, so the assertion
 * below is the derivation itself and not a restatement of it: change either
 * array and the file stops compiling.
 *
 * Compiled only under the 32-bit ABI these offsets describe, and the guard
 * WAS MISSING: the assertions went in ungated and `make check64` has been
 * failing on them ever since, seven errors deep in a log nobody reads the
 * head of.  `-D__SIZEOF_POINTER__=4` is in `period.mk` and `period_inner.sh`
 * precisely so that the guard is live on 3.4.2, which does not define the
 * macro itself -- see docs/method/compilers.md and `tools/assertlive.py`,
 * which is what checks that the guard is not silently `#if 0`.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V22_ASSERT_OFF(tag, type, field, want) \
	typedef char tag[(__builtin_offsetof(type, field) == (want)) ? 1 : -1]

V22_ASSERT_OFF(d_bits, struct v22_dp, bits_per_word, 0x14);
V22_ASSERT_OFF(d_want, struct v22_dp, tx_bits_wanted, 0x18);
V22_ASSERT_OFF(d_fp, struct v22_dp, fp, 0x1c);
V22_ASSERT_OFF(d_wrap, struct v22_dp, wrapper, 0x20);
V22_ASSERT_OFF(d_txb, struct v22_dp, tx_bits, 0x24);
V22_ASSERT_OFF(d_rxb, struct v22_dp, rx_bits, 0x1b4);

typedef char v22_dp_size[(sizeof(struct v22_dp) == 0x344) ? 1 : -1];
#endif /* 32-bit */

static struct dp *v22_create(void *modem, int id, int caller, int srate,
			     int max_frag, struct dp_operations *op);
static int v22_delete(struct dp *dp);
static int v22_process(void *dp_arg, void *in, void *out, int count);

/*
 * FILE-LOCAL, read off the relocation form, exactly as `b103_ops` is:
 * `dp_v22_init` and `dp_v22_exit` reach this table as `.data` plus an inline
 * addend rather than as a named symbol, and `nm` shows a lower-case `d`.
 *
 * `process` is dp_wrapper_run, NOT v22_process.  The core always calls the
 * wrapper; the wrapper calls v22_process at the datapump's own rate and
 * fragment size once it has buffered and rate-converted.  v22_process is
 * handed to `dp_wrapper_create` as a function pointer and is not reachable
 * from this table at all.
 *
 * `use_count` and `hangup` are zero: the object's 24 bytes are a relocation
 * to the name string, a zero word, three relocations, and a zero word.
 */
static struct dp_operations v22_ops = {
	.name = "v22",
	.create = v22_create,
	.destroy = v22_delete,
	.process = (int (*)(struct dp *, void *, void *, int))dp_wrapper_run
};

/*
 * ---------------------------------------------------------------------------
 * v22_create -- .text 0x004fb0, 300 bytes.
 *
 * Allocate the datapump's own state, wire a dp_wrapper in front of
 * v22_process, and build the V22FP configuration.
 *
 * Only two of the seven configuration words depend on the arguments:
 *
 *     mode = (caller == 0)      so mode 1 -- v22_answer, by F8529's table --
 *                               is the NOT-caller, and mode 0 originates.
 *     rate = 2 for 212, 1 for 22, 0 for anything else (122)
 *                               and v22fp.h reads rate 0 as 2400 with 1 and 2
 *                               both 1200, so V.22bis gets 2400 and V.22 and
 *                               Bell 212 get 1200.
 *
 * The other five are constants: 60000, 0, 700, 0, 1.  `f08` becomes the node
 * deadline in milliseconds on ReadGTimer's 20 ms clock -- 3000 blocks -- and
 * `f10` the carrier-loss grace time, 700 ms or 35 blocks (finding F8531).
 *
 * NOTHING IS SET AFTER THE MEMSET except the five words below: unlike
 * `b103_create` there is no `last_status` to seed, and `tx_bits_wanted` is
 * left at zero, which is what makes the first block take the "carry no data"
 * arm of v22_process.
 */
static struct dp *
v22_create(void *modem, int id, int caller, int srate, int max_frag,
	   struct dp_operations *op)
{
	struct v22fp_cfg cfg;
	struct v22_dp *dp;

	(void)max_frag;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v22: create...\n");

	dp = (struct v22_dp *)sysdep_malloc(sizeof(*dp));
	if (dp == NULL)
		return NULL;
	sysdep_memset(dp, 0, sizeof(*dp));

	dp->dp.id = id;
	dp->dp.modem = modem;
	dp->dp.op = op;

	dp->wrapper = dp_wrapper_create(dp, v22_process, V22_DP_FRAG, srate,
					V22_DP_SRATE);
	if (dp->wrapper == NULL) {
		sysdep_free(dp);
		return NULL;
	}
	dp->dp.dp_data = dp->wrapper;
	dp->wrapper->dp = &dp->dp;

	cfg.mode = (caller == 0);
	cfg.rate = (id == V22_DP_ID_BELL212) ? 2 : (id == V22_DP_ID_V22);
	cfg.f08 = 60000;		/* 3000 blocks, the node deadline */
	cfg.f0c = 0;
	cfg.f10 = 700;			/* 35 blocks, the carrier grace   */
	cfg.f14 = 0;
	cfg.f18 = 1;

	dp->fp = V22FP_create(NULL, &cfg);
	if (dp->fp == NULL) {
		dp_wrapper_delete(dp->wrapper);
		sysdep_free(dp);
		return NULL;
	}

	return &dp->dp;
}

/*
 * v22_delete -- .text 0x0050e0, 72 bytes.
 */
static int
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

/*
 * ---------------------------------------------------------------------------
 * v22_process -- .text 0x005130, 557 bytes.
 *
 * One block, called by dp_wrapper at the datapump's own rate.
 *
 * THE STATUS TRANSLATION is a seventeen-entry jump table over the low byte of
 * what `V22FP_modem` returns:
 *
 *     0                -> DPSTAT_OK
 *     1                -> DPSTAT_OK,      and stop asking for data
 *     3                -> DPSTAT_CONNECT, four bits per symbol at 2400
 *     4                -> DPSTAT_CONNECT, two bits per symbol at 1200
 *     everything else  -> DPSTAT_ERROR
 *
 * so 4 is the 1200 connect, which finding F8531 had left unnamed; 3 is
 * already `V22_MSG_CONNECT_2400` from `connect_2400`'s own format strings.
 * Unlike `b103_process` the rates are reported on EVERY connecting block and
 * not only on the transition into one -- there is no edge test here at all.
 *
 * THE TABLE'S LENGTH IS EVIDENCE AND THE SPELLING BELOW IS NOT.  Seventeen
 * entries and a `cmp $0x10; ja` mean the highest case label the author wrote
 * was 16, twelve of the seventeen landing on the same block as `default`.
 * 16 is `V22_MSG_NO_CARRIER` (finding F8534, corroborated from V.32), so it
 * is spelt as an explicit case falling into the default.  Any other
 * assignment of the twelve dead labels is behaviourally identical and this
 * one is not established -- what the object forces is only that a label at 16
 * exists.
 */
static int
v22_process(void *dp_arg, void *in, void *out, int count)
{
	struct dp *dp = (struct dp *)dp_arg;
	struct v22_dp *self = (struct v22_dp *)
		((struct dp_wrapper *)dp->dp_data)->dp;
	int tx_len;
	int rx_len;
	int status;
	int result;
	int i;

	if (self->tx_bits_wanted != 0) {
		/*
		 * Fetch as bytes into the same buffer V22FP_modem will read
		 * as ints, then widen backwards from n-1.
		 */
		tx_len = self->tx_bits_wanted;
		tx_len = modem_get_bits(dp->modem, self->bits_per_word,
					(unsigned char *)self->tx_bits,
					tx_len);
		for (i = tx_len - 1; i >= 0; i--)
			self->tx_bits[i] =
				((unsigned char *)self->tx_bits)[i];
	} else {
		tx_len = V22_WORDS_PER_BLOCK;
	}

	rx_len = count;
	result = V22FP_modem(self->fp, self->tx_bits, (short *)out,
			     (const short *)in, self->rx_bits,
			     &tx_len, &rx_len);

	/*
	 * The clamp, and the author's own word for the count: the format
	 * string is "v22: FATAL: rx_len is huge (%d).\n".  The comparison is
	 * UNSIGNED -- `cmp $0x64; jbe` -- so a negative count would be
	 * clamped too, and the bound is V22_BIT_BUFFER, the length of the
	 * array about to be written.
	 */
	if (self->tx_bits_wanted != 0) {
		if ((unsigned)rx_len > V22_BIT_BUFFER) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "v22: FATAL: rx_len is huge (%d).\n",
				    rx_len);
			rx_len = V22_BIT_BUFFER;
		}
	} else {
		rx_len = 0;
	}

	/* Hand recovered words back, narrowing ints to bytes in place. */
	for (i = 0; i < rx_len; i++)
		((unsigned char *)self->rx_bits)[i] =
			(unsigned char)(self->rx_bits[i]
					& ((1 << self->bits_per_word) - 1));

	modem_put_bits(dp->modem, self->bits_per_word,
		       (unsigned char *)self->rx_bits, rx_len);

	status = result & 0xff;
	switch (status) {
	case 0:
		result = DPSTAT_OK;
		break;
	case 1:
		self->tx_bits_wanted = 0;
		result = DPSTAT_OK;
		break;
	case V22_MSG_CONNECT_2400:
		self->tx_bits_wanted = V22_WORDS_PER_BLOCK;
		self->bits_per_word = V22_BITS_PER_WORD_2400;
		modem_set_param(dp->modem, MDMPRM_TX_RATE, V22_LINE_RATE_2400);
		modem_set_param(dp->modem, MDMPRM_RX_RATE, V22_LINE_RATE_2400);
		result = DPSTAT_CONNECT;
		break;
	case V22_MSG_CONNECT_1200:
		self->tx_bits_wanted = V22_WORDS_PER_BLOCK;
		self->bits_per_word = V22_BITS_PER_WORD_1200;
		modem_set_param(dp->modem, MDMPRM_TX_RATE, V22_LINE_RATE_1200);
		modem_set_param(dp->modem, MDMPRM_RX_RATE, V22_LINE_RATE_1200);
		result = DPSTAT_CONNECT;
		break;
	case V22_MSG_NO_CARRIER:
	default:
		result = DPSTAT_ERROR;
		break;
	}

	/*
	 * The report is on the EDGE, but only the report: `dp->status` is
	 * assigned on every path and the message is printed only when the
	 * value changed.
	 */
	if (result != (int)dp->status && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("v22: V22STAT: --> %d\n", status);

	dp->status = (unsigned)result;
	return result;
}

/*
 * Three ids, one table, in the object's order: 122, 22, 212.  Exit
 * deregisters in the same order rather than unwinding.
 */
int
dp_v22_init(void)
{
	modem_dp_register(V22_DP_ID_V22BIS, &v22_ops);
	modem_dp_register(V22_DP_ID_V22, &v22_ops);
	modem_dp_register(V22_DP_ID_BELL212, &v22_ops);
	return 0;
}

void
dp_v22_exit(void)
{
	modem_dp_deregister(V22_DP_ID_V22BIS, &v22_ops);
	modem_dp_deregister(V22_DP_ID_V22, &v22_ops);
	modem_dp_deregister(V22_DP_ID_BELL212, &v22_ops);
}
