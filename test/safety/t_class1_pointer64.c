/* Component-boundary probe: real fax_class1_progress with an instrumented
 * state callback. The callback is synthetic; this is not a complete fax call. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dsplib/class1.h"
#include "dsplib/fpm_iir.h"

unsigned int dsplibs_debug_level;
int dsplibs_debug_printf(const char *fmt, ...) { (void)fmt; return 0; }
void FPM_iir_filt_II(short *s, const short *c, short *h, short n, short count)
{
	(void)s; (void)c; (void)h; (void)n; (void)count;
	abort(); /* The fixture explicitly disables the filter. */
}
static unsigned char *want_out;
static const unsigned char *want_in;
static int *want_count;
static int checks, failures, calls, negative;
static void check(const char *name, int ok)
{
	++checks;
	if (!ok) {
		++failures;
		printf("%s: %s\n", negative ? "expected rejection" : "FAIL", name);
	}
}
static int handler(struct fax_class1 *ctx, const short *rx, short *tx,
	unsigned char *out, const unsigned char *in, int *rx_count, int *tx_count,
	int *out_count, int *in_count)
{
	int valid = out == want_out && in == want_in && out_count == want_count;
	(void)ctx; (void)rx; (void)tx; (void)in_count;
	++calls;
	check("callback output buffer", out == want_out);
	check("callback input buffer", in == want_in);
	check("callback output count", out_count == want_count);
	/* Reject truncation before touching any potentially invalid pointer. */
	if (!valid) return -1;
	check("valid component sample counts", *rx_count == 160 && *tx_count == 160);
	check("progress cleared output count", *out_count == 0);
	out[0] = in[0];
	*out_count = 1;
	return 0;
}
int main(void)
{
	struct fax_class1 ctx;
	short rx[160] = { 0 }, tx[160] = { 0 };
	unsigned char out[8] = { 0 }, in[8] = { 0xa7 };
	int nr = 160, nt = 160, nout = 99, nin = 1, before, i;
	memset(&ctx, 0, sizeof ctx);
	ctx.state = ctx.prev_state = CLASS1_IDLE_STATE;
	want_out = out; want_in = in; want_count = &nout;
	check("LP64 required", sizeof(void *) == 8);
	check("output address above uint32", (uintptr_t)out > UINT32_MAX);
	check("input address above uint32", (uintptr_t)in > UINT32_MAX);
	check("count address above uint32", (uintptr_t)&nout > UINT32_MAX);
	class1_state_functions[ctx.state] = handler;
	check("progress status", fax_class1_progress(&ctx, rx, tx, out, in,
		&nr, &nt, &nout, &nin) == FAX_CLASS1_NO_MESSAGE);
	check("one callback dispatch", calls == 1);
	check("callback copied input byte", out[0] == 0xa7);
	check("callback wrote native output count", nout == 1);
	negative = 1;
	for (i = 0; i < 3; ++i) {
		before = failures;
		handler(&ctx, rx, tx,
			i == 0 ? (void *)(uintptr_t)(uint32_t)(uintptr_t)out : out,
			i == 1 ? (void *)(uintptr_t)(uint32_t)(uintptr_t)in : in,
			&nr, &nt,
			i == 2 ? (void *)(uintptr_t)(uint32_t)(uintptr_t)&nout : &nout,
			&nin);
		if (failures == before + 1) failures = before;
		else ++failures;
		++checks;
	}
	printf("class1 pointer64 safety: %d checks, %d failures; 3 truncation negative controls\n",
		checks, failures);
	return failures != 0;
}
