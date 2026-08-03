/*
 * harness.h -- Tier-1 differential test scaffolding.
 *
 * A differential test drives the reconstruction and the original object from
 * the same input and compares the results.  The original is linked in as
 * dsplibs_ref.o, produced by:
 *
 *     python3 tools/symmap.py ../slmodemd/dsplibs.o -o build/symmap.txt
 *     objcopy --redefine-syms=build/symmap.txt \
 *             ../slmodemd/dsplibs.o build/dsplibs_ref.o
 *
 * so every original entry point is reachable as `ref_<name>`.  Declare the
 * ones a test needs with DIFF_REF() and compare against them.
 *
 * Failure output always prints the input that triggered it, because a
 * companding or filter mismatch is meaningless without knowing which sample
 * produced it.
 */

#ifndef DSPLIB_TEST_HARNESS_H
#define DSPLIB_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>

/* Reconstructed C++ modules link against this too. */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Records how a side used the parameter interface, so tests can assert which
 * MDMPRM_* was requested rather than only what came back.
 */
struct param_log {
	int calls;
	void *last_modem;
	unsigned last_param;
};

extern struct param_log harness_param_ours;
extern struct param_log harness_param_ref;
void harness_param_reset(void);

/*
 * Force one parameter to a chosen value for both sides.  Cleared by
 * harness_param_reset.  Without this the store derives its answer from the
 * index, which is fine for proving a module asks for the right parameter but
 * useless when the value itself has to be in a particular range.
 */
void harness_param_set(unsigned param, long value);

/*
 * Datapump registry.
 *
 * Every datapump module registers one or more DP_IDs against a
 * struct dp_operations at init time.  Recording those calls lets a test
 * assert which IDs a module claims and in what order -- the part of a
 * datapump's contract that is pure bookkeeping and easy to get subtly wrong
 * (a transposed ID would only show up as the wrong modulation being selected,
 * far downstream).
 */
#define HARNESS_MAX_REG 32

struct reg_log {
	int count;
	int id[HARNESS_MAX_REG];
	void *ops[HARNESS_MAX_REG];
	/*
	 * And the same for the way out.  Recorded rather than merely counted
	 * because a datapump that deregisters an id or a table other than the
	 * one it registered leaves the core holding a pointer into something
	 * that thinks it has gone, and a bare count cannot see that.
	 */
	int deregistered;
	int dereg_id[HARNESS_MAX_REG];
	void *dereg_ops[HARNESS_MAX_REG];
};

/*
 * What sysdep_malloc fills fresh allocations with.
 *
 * Handing back a fixed non-zero pattern makes a constructor that leaves a
 * field uninitialised comparable -- both sides see the same value -- and
 * makes anything that reads such a field get an obviously wrong number
 * instead of the zero a fresh page happens to give.  Zero is the one value
 * that makes an uninitialised field look deliberate.
 *
 * It found two constructor defects in the reconstruction the moment it was
 * turned on (findings 69), and one in the original: D17, the half-duplex
 * receive handler that loopback never installs.  t_b103hdx uses this
 * constant directly, to tell "nothing has written it" apart from "something
 * wrote a state this test does not know".
 */
#define HARNESS_MALLOC_FILL	0xa5

/* slmodemd has 256 S-registers; the store mirrors that. */
#define HARNESS_SREGS	256
void harness_sreg_reset(void);
void harness_sreg_set(unsigned n, long v);

extern struct reg_log harness_reg_ours;
extern struct reg_log harness_reg_ref;
void harness_reg_reset(void);

/*
 * Allocation bookkeeping, so a test can assert that create/delete balance.
 * `bad_free` counts frees of pointers the allocator never handed out --
 * double frees and wild pointers -- which are swallowed rather than passed to
 * free(), so the run survives to report them.
 */
struct alloc_log {
	int allocs;	/* successful sysdep_malloc calls          */
	int frees;	/* sysdep_free calls that released memory  */
	int live;	/* outstanding allocations                 */
	unsigned bytes;	/* outstanding bytes                       */
	int bad_free;	/* frees of unknown pointers               */
	int free_null;	/* sysdep_free(NULL)                       */
	int overflow;	/* live set full; counts are unreliable    */
};

/*
 * The modem core's bit pipe, one instance per side.
 *
 * `modem_get_bits` hands out data and `modem_put_bits` consumes it, so a
 * shared implementation would have the two sides eating each other's stream.
 * Each gets its own cursor over the same scripted pattern and its own sink.
 */
#define HARNESS_SHIM_BITS   8192
#define HARNESS_SHIM_PARAMS 32

struct modem_shim {
	int tx_pos;		/* cursor into the scripted pattern   */
	int gets;		/* modem_get_bits calls               */
	int puts;		/* modem_put_bits calls               */
	unsigned char rx[HARNESS_SHIM_BITS];
	int rx_len;
	int rx_overflow;
	unsigned param_name[HARNESS_SHIM_PARAMS];
	int param_value[HARNESS_SHIM_PARAMS];
	int nparams;
};

extern struct modem_shim harness_modem_ours;
extern struct modem_shim harness_modem_ref;
void harness_modem_reset(const unsigned char *pattern, int len);

extern struct alloc_log harness_alloc;
void harness_alloc_reset(void);

extern int diff_checks;
extern int diff_failures;
extern int diff_max_report;

void diff_begin(const char *name);
int diff_end(void);

/*
 * Compare one integer result.  `desc` should identify the input, e.g.
 *   diff_eq_int("alaw2linear(0x%02x)", got, want, in);
 */
void diff_eq_int_(const char *file, int line, const char *fmt,
		  long got, long want, long input);

#define diff_eq_int(fmt, got, want, input) \
	diff_eq_int_(__FILE__, __LINE__, (fmt), (long)(got), (long)(want), \
		     (long)(input))

/*
 * Debug capture: with this set, each side's dsplibs_debug_printf appends to
 * its own transcript, so the diagnostic paths can be compared like any other
 * output.  Side 0 is the reconstruction, side 1 the blob.  Remember to raise
 * BOTH dsplibs_debug_level and ref_dsplibs_debug_level, or the two will take
 * different branches for reasons unrelated to the modem.
 */
extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
const char *dsplib_debug_capture_text(int side);
/*
 * How many lines that side PRINTED, not counting the callback markers the
 * harness itself writes.  Anti-vacuity checks want this, not the text: a
 * function that only touches modem_set_param fills the buffer without any
 * call site firing (finding 149).
 */
unsigned dsplib_debug_capture_lines(int side);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_TEST_HARNESS_H */
