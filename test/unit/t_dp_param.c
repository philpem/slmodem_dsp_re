/*
 * t_dp_param.c -- differential test of the datapump parameter accessor.
 *
 * Trivial in itself, but it checks the thing that actually matters: that both
 * sides request the *same* parameter index.  A reconstruction that fetched
 * MDMPRM_DSPINFO instead of MDMPRM_DPRUNTIME would return a plausible pointer
 * and fail much later, somewhere unrelated.
 */

#include "harness.h"
#include "dsplib/dp_param.h"

extern void *ref_dp_param_get(void *modem);
extern void *ref_dp_runtime_create(void *modem);
extern void ref_dp_runtime_delete(void *runtime);

/*
 * The four `struct dsp_info` values the runtime block is built from, and the
 * codec type, swept together.  `qc_index` 0 is in the list on purpose: it is
 * the one input with a branch on it, and the arm writes the literal 9.
 */
static const struct {
	unsigned int	connection_type;
	int		clock_deviation;
	unsigned int	qc_lapm;
	unsigned int	qc_index;
	int		codec;
} info_cases[] = {
	{ 0,          0,           0, 0,          0 },
	{ 1,          1,           1, 1,          4 },
	{ 0xffffffff, -1,          1, 9,          3 },
	{ 2,          123456,      0, 0x7fffffff, 1 },
	{ 7,          -123456,     1, 2,          2 },
	{ 0x80000000, -2147483647, 0, 5,          0 },
	{ 3,          4,           2, 0,          6 },   /* qc_lapm bit 1 only */
	{ 3,          4,           3, 8,          6 },   /* both low bits */
};

/*
 * A block built from all-zero inputs, so every field the reconstruction gets
 * from an INPUT reads zero and only the constants remain.  Two blocks that
 * are equal because both are blank would pass the comparison and prove
 * nothing (`docs/method/gates.md`), so the sweep is required to move.
 */
static int
block_differs(const void *a, const void *b, unsigned n)
{
	const unsigned char *p = (const unsigned char *)a;
	const unsigned char *q = (const unsigned char *)b;
	unsigned i;

	for (i = 0; i < n; i++)
		if (p[i] != q[i])
			return 1;
	return 0;
}

static void
runtime_block(void)
{
	char modem;
	struct dsp_info info;
	unsigned char first[sizeof(struct _tagModemParameters)];
	int i, moved = 0, nonzero = 0;

	for (i = 0; i < (int)(sizeof(info_cases) / sizeof(info_cases[0])); i++) {
		void *ours, *ref;
		unsigned j;

		harness_param_reset();
		harness_alloc_reset();

		info.connection_type = info_cases[i].connection_type;
		info.clock_deviation = info_cases[i].clock_deviation;
		info.qc_lapm = info_cases[i].qc_lapm;
		info.qc_index = info_cases[i].qc_index;

		harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&info);
		harness_param_set(MDMPRM_CODECTYPE, info_cases[i].codec);

		ours = dp_runtime_create(&modem);
		ref = ref_dp_runtime_create(&modem);

		diff_eq_int("case %ld: ours allocated", ours != 0, 1, i);
		diff_eq_int("case %ld: ref allocated", ref != 0, 1, i);
		if (!ours || !ref)
			continue;

		diff_eq_obj("dp_runtime_create", struct _tagModemParameters,
			    ours, ref, i);

		/*
		 * Both sides ask the host for the same two indices, in the
		 * same order and the same number of times.  A reconstruction
		 * that read MDMPRM_DSPINFO twice, or asked for DPRUNTIME
		 * instead of CODECTYPE, would build an identical block from
		 * this harness and diverge against a real modem.
		 */
		diff_eq_int("case %ld: get_param calls",
			    harness_param_ours.calls,
			    harness_param_ref.calls, i);
		diff_eq_int("case %ld: last index", harness_param_ours.last_param,
			    harness_param_ref.last_param, i);
		diff_eq_int("case %ld: last index is CODECTYPE",
			    harness_param_ours.last_param, MDMPRM_CODECTYPE, i);
		diff_eq_int("case %ld: two get_param calls",
			    harness_param_ours.calls, 2, i);

		/* Anti-vacuity: the block is not blank, and the sweep moves it. */
		for (j = 0; j < sizeof(first); j++)
			nonzero |= ((const unsigned char *)ours)[j] != 0;
		if (i == 0)
			memcpy(first, ours, sizeof(first));
		else
			moved |= block_differs(first, ours, sizeof(first));

		dp_runtime_delete(ours);
		ref_dp_runtime_delete(ref);

		diff_eq_int("case %ld: both blocks freed",
			    harness_alloc.frees, harness_alloc.allocs, i);
		diff_eq_int("case %ld: no bad free", harness_alloc.bad_free, 0, i);
	}

	diff_eq_int("the block is not blank", nonzero, 1, 0);
	diff_eq_int("the dsp_info sweep moves the block", moved, 1, 0);
}

/*
 * The constants the block carries whatever the host says, read off the
 * REFERENCE rather than off ours -- so this is a claim about the blob and it
 * fails if the blob ever stops saying it, not a restatement of our own source.
 */
static void
runtime_constants(void)
{
	char modem;
	struct dsp_info info;
	struct _tagModemParameters *r;

	harness_param_reset();
	harness_alloc_reset();
	memset(&info, 0, sizeof(info));
	info.qc_lapm = 1;
	harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&info);
	harness_param_set(MDMPRM_CODECTYPE, 4);

	r = (struct _tagModemParameters *)ref_dp_runtime_create(&modem);
	diff_eq_int("reference block allocated", r != 0, 1, 0);
	if (!r)
		return;

	/*
	 * THE SIZE, asked of the blob rather than of our own header.  Nothing
	 * else has allocated since the reset, so this is the one
	 * `movl $0x88,(%esp)` at 0x58ea, and it is the entire evidence that
	 * `struct _tagModemParameters` is 136 bytes.  Comparing our sizeof
	 * against a literal 0x88 would only restate the header.
	 */
	diff_eq_int("the blob asks sysdep_malloc for 0x88 (%ld)",
		    (long)harness_alloc.bytes, 0x88, 0);
	diff_eq_int("one allocation (%ld)", harness_alloc.allocs, 1, 0);
	diff_eq_int("our header agrees (%ld)",
		    (long)sizeof(struct _tagModemParameters),
		    (long)harness_alloc.bytes, 0);

	diff_eq_int("+0x004 is 60 (%ld)", r->unnamed_0004, 60, 0);
	diff_eq_int("+0x008 is 40 (%ld)", r->unnamed_0008, 40, 0);
	diff_eq_int("+0x014 is 700 (%ld)", r->unnamed_0014, 700, 0);
	diff_eq_int("+0x044 is 6 (%ld)", r->unnamed_0044, 6, 0);
	diff_eq_int("+0x050 modeFlags is 1 (%ld)", r->modeFlags, 1, 0);
	diff_eq_int("+0x054 codecType is the host's (%ld)", r->codecType, 4, 0);
	/* qc_index 0 takes the arm at 0x5a00, which writes 9. */
	diff_eq_int("+0x010 qcIndex defaults to 9 (%ld)", r->qcIndex, 9, 0);
	/* bit 4 set by dp_runtime_create, bit 6 from qc_lapm, 5 and 7 clear. */
	diff_eq_int("+0x002 qcFlags is 0x50 (%ld)", r->qcFlags, 0x50, 0);
	/* The two the datapump is expected to fill in, still zero here. */
	diff_eq_int("+0x064 hwDelay starts 0 (%ld)", r->hwDelay, 0, 0);
	diff_eq_int("+0x038 minRate starts 0 (%ld)", (long)r->minRate, 0, 0);
	diff_eq_int("+0x078 paramFile starts NULL (%ld)",
		    r->paramFile != 0, 0, 0);

	ref_dp_runtime_delete(r);
	diff_eq_int("reference block freed (%ld)", harness_alloc.frees,
		    harness_alloc.allocs, 0);
}

int
main(void)
{
	/* Distinct dummy modem pointers; never dereferenced. */
	char modem_a, modem_b;
	void *ours, *ref;

	diff_begin("dp_param_get");

	harness_param_reset();
	ours = dp_param_get(&modem_a);
	ref = ref_dp_param_get(&modem_a);

	diff_eq_int("returned value (%ld)", (long)ours, (long)ref, 0);
	diff_eq_int("parameter index (%ld)", harness_param_ours.last_param,
		    harness_param_ref.last_param, 0);
	diff_eq_int("is MDMPRM_DPRUNTIME (%ld)", harness_param_ours.last_param,
		    MDMPRM_DPRUNTIME, 0);
	diff_eq_int("call count (%ld)", harness_param_ours.calls,
		    harness_param_ref.calls, 0);
	diff_eq_int("modem passed through (%ld)",
		    harness_param_ours.last_modem == &modem_a,
		    harness_param_ref.last_modem == &modem_a, 0);

	/* A different modem pointer must reach the callback unchanged. */
	harness_param_reset();
	ours = dp_param_get(&modem_b);
	ref = ref_dp_param_get(&modem_b);
	diff_eq_int("second modem value (%ld)", (long)ours, (long)ref, 0);
	diff_eq_int("second modem pointer (%ld)",
		    harness_param_ours.last_modem == &modem_b,
		    harness_param_ref.last_modem == &modem_b, 0);

	runtime_block();
	runtime_constants();

	return diff_end();
}
