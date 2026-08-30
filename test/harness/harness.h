/*
 * harness.h -- Tier-1 differential test scaffolding.
 *
 * A differential test drives the reconstruction and the original object from
 * the same input and compares the results.  The original is linked in as
 * dsplibs_ref.o, produced by:
 *
 *     python3 tools/symmap.py ref/slmodemd/dsplibs.o -o build/symmap.txt
 *     objcopy --redefine-syms=build/symmap.txt \
 *             ref/slmodemd/dsplibs.o build/dsplibs_ref.o
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
#include <string.h>

/*
 * MANGLED `ref_` NAMES MUST BE DECLARED extern "C" FROM C++.
 *
 * Nothing here demangles.  symmap.py prepends the prefix to the raw symbol
 * string, so the blob's `_ZN12VPcmFloModem11enterPhase3Ev` becomes
 * `ref__ZN12VPcmFloModem11enterPhase3Ev` -- a valid C identifier that happens
 * to have a mangled name inside it, and 886 of the aliases are this shape.
 *
 * From C it just works.  From C++ a plain declaration is mangled AGAIN, as an
 * ordinary function whose name is that string:
 *
 *   extern int ref__ZN12VPcmFloModem11enterPhase3Ev(void *);
 *       -> U _Z36ref__ZN12VPcmFloModem11enterPhase3EvPv     never resolves
 *
 *   extern "C" int ref__ZN12VPcmFloModem11enterPhase3Ev(void *);
 *       -> U ref__ZN12VPcmFloModem11enterPhase3Ev           resolves
 *
 * The failure is a link error whose undefined symbol CONTAINS the name you
 * wanted, so it reads as "the blob does not export this" rather than "my
 * declaration was mangled".  Put every mangled `ref_` declaration inside the
 * extern "C" block below, or write the test in C.
 */

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
 * turned on (findings F69), and one in the original: D17, the half-duplex
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
 * The TTY, one transcript per side, so what `modem_send_to_tty` was handed
 * can be compared like any other output.  Bytes past the cap are counted in
 * `len` and dropped, never truncating a comparison silently: a test compares
 * `len` first, and equal lengths above the cap already differ from what fits.
 * `calls` is the anti-vacuity number -- CID sends each string and its CRLF
 * as separate calls, and a wrapper that coalesced them would hand equal
 * bytes over a different call pattern.
 */
#define HARNESS_TTY_MAX	4096
struct tty_log {
	int calls;
	int len;			/* total OFFERED, even past the cap */
	unsigned char data[HARNESS_TTY_MAX];
};
extern struct tty_log harness_tty_ours;
extern struct tty_log harness_tty_ref;
void harness_tty_reset(void);

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
	/*
	 * This shim's own stream.  NULL means the shared scripted one, which
	 * is what every caller predating the routing below gets.
	 */
	const unsigned char *pattern;
	int pattern_len;
};

extern struct modem_shim harness_modem_ours;
extern struct modem_shim harness_modem_ref;
void harness_modem_reset(const unsigned char *pattern, int len);

/*
 * ROUTING THE BIT PIPE ON THE MODEM HANDLE.
 *
 * One shim per SIDE is enough while a test drives one datapump.  Two
 * endpoints of one CALL are a different shape: both are the blob's, so both
 * land in `harness_modem_ref`, drawing from one `tx_pos` and writing into one
 * `rx` sink.  A bit-error rate measured off that reads zero when the two
 * endpoints are quietly sharing a stream -- gates.md's "result
 * indistinguishable from success", and it cannot be argued away because the
 * right answer and the vacuous answer are the same number.
 *
 * So a test may register a handle and get its own pair of shims and its own
 * pattern.  Nothing is routed until something registers, so every existing
 * test keeps the single pair it had.
 */
#define HARNESS_SHIM_ROUTES 4

extern struct modem_shim harness_modem_route_ours[HARNESS_SHIM_ROUTES];
extern struct modem_shim harness_modem_route_ref[HARNESS_SHIM_ROUTES];

/* Forget every route.  `harness_modem_reset` does this too. */
void harness_modem_route_reset(void);

/*
 * Give `m` its own shims and its own stream.  Returns the route's index, or
 * -1 if the table is full or the handle is already registered -- a silently
 * ignored duplicate would put two endpoints back on one shim, which is the
 * whole thing this exists to prevent.
 */
int harness_modem_route_add(void *m, const unsigned char *pattern, int len);

extern struct alloc_log harness_alloc;
void harness_alloc_reset(void);

/*
 * WHICH sysdep_malloc HANDED A POINTER OUT -- 1 for the run's first, 2 for
 * its second, 0 if it is not live.
 *
 * Use this, not an address comparison, to ask "did this field get the FIRST
 * of the two allocations".  Both allocators recycle LIFO, so after
 * free(a); free(b) the next two mallocs return b's chunk and then a's, and
 * `a < b` inverts on alternate trials -- 20 non-monotonic pairs in 40 on
 * 2005 static glibc and 20 in 40 on a modern one.  A check written that way
 * is reading the allocator, not the code under test.  See finding F1353.
 */
unsigned long harness_alloc_ordinal(const void *p);

/*
 * How many bytes this pointer's sysdep_malloc was ASKED for, 0 if not live.
 *
 * Use this, not `malloc_usable_size`, to compare our block's size against the
 * reference's.  Usable size reports the CHUNK the allocator served the
 * request from, and glibc hands over a remainder too small to split rather
 * than wasting it -- so two identical requests differ whenever one was carved
 * from the top and the other recycled something larger.  Finding F1353.
 */
unsigned harness_alloc_reqsize(const void *p);

/*
 * THE LIVE SET ITSELF, not just its size.
 *
 * A test that wants to snapshot and restore a whole allocated GRAPH -- 125
 * regions for a V.34 construction (finding F802) -- needs the pointers, and
 * the allocator already holds them.  Exposing them is what lets four
 * sequential runs use literally the same memory at the same addresses, which
 * is the congruence findings F780 and F783 get from a static arena and a
 * constructed object cannot get any other way.
 *
 * Writes at most `max` pointers and returns HOW MANY THERE ARE, so a caller
 * whose buffer is too small finds out rather than silently seeing a prefix.
 * Sizes are `malloc_usable_size`'s; the allocator does not record them.
 *
 * The order is the hash table's, which is a function of the addresses alone
 * -- so two calls with the same live set return the same order, and that is
 * all a snapshot/restore pair needs.
 */
int harness_alloc_live_set(void **out, int max);

extern int diff_checks;
extern int diff_failures;
extern int diff_max_report;

void diff_begin(const char *name);
int diff_end(void);

/*
 * NaN, DETECTED FROM THE BITS.  `x != x` is folded to zero under the object's
 * own -mno-ieee-fp, which `make period` builds this apparatus with, so the
 * idiom detects nothing there and does so silently.  Use these instead of a
 * self-comparison anywhere a test has to know whether a value is unordered.
 * Finding F2303.
 */
int diff_isnan_f(float x);
int diff_isnan_ld(long double x);

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
 * Compare two floats AS FLOATS, reporting the values and their distance in
 * ULP rather than the integers their bits happen to spell.
 *
 * `diff_eq_float` is EXACT -- bit for bit, the same test a punned
 * `diff_eq_int` performs, and the one to reach for by default.  It exists
 * because the punned form reports a last-place difference as two nine-digit
 * integers with no indication that either is a float.
 *
 * `diff_eq_float_ulp` allows a budget, and is correct ONLY where exactness is
 * unachievable rather than merely unmet: the modern build keeps x87
 * intermediates at 80 bits where the blob's compiler spilled them to 32, so it
 * declines to discard precision the object discarded and no source change
 * makes the two agree.  A budget is a claim about the TIER, not a tolerance
 * for being wrong -- state the reason at every call site, and keep it as tight
 * as the measurement allows so that a real regression still fails.
 *
 * Two NaNs compare equal: which NaN the coprocessor produced is not a property
 * of the reconstruction.
 *
 * `diff_eq_float_abs` bounds the ABSOLUTE difference instead, and is the right
 * form wherever values pass through zero.  ULP is a relative measure, so a bin
 * whose terms cancel can be a hundred thousand ULP from the reference while
 * being seven millionths away from it -- measured, not supposed: `four1`'s
 * worst ULP distance is 121,933 and its worst absolute error is 6.1e-05, and
 * they are not the same comparison.
 *
 * THE TEST FOR ANY BUDGET IS WHETHER IT COULD CHANGE A DECISION DOWNSTREAM.
 * An error beneath the resolution of everything that consumes the value is
 * noise; say at the call site what consumes it, and keep the budget tight
 * enough that a real regression still fails.
 */
void diff_eq_float_(const char *file, int line, const char *fmt, float got,
		    float want, unsigned long ulp_budget, double abs_eps,
		    long input);

#define diff_eq_float(fmt, got, want, input) \
	diff_eq_float_(__FILE__, __LINE__, (fmt), (float)(got), \
		       (float)(want), 0UL, 0.0, (long)(input))

#define diff_eq_float_ulp(fmt, got, want, budget, input) \
	diff_eq_float_(__FILE__, __LINE__, (fmt), (float)(got), \
		       (float)(want), (unsigned long)(budget), 0.0, \
		       (long)(input))

#define diff_eq_float_abs(fmt, got, want, eps, input) \
	diff_eq_float_(__FILE__, __LINE__, (fmt), (float)(got), \
		       (float)(want), 0UL, (double)(eps), (long)(input))

/*
 * Compare two whole objects, reporting the first differing FIELD.
 *
 *   diff_eq_obj("after process", struct v34_decision, &da, &db, i);
 *
 * `type` is written unquoted and is stringified: it is the C type name that
 * tools/whichfield.py looks up in our DWARF to turn an offset into a field
 * path.  See harness.c for why this exists rather than a memcmp.
 */
void diff_eq_obj_(const char *file, int line, const char *what,
		  const char *type, const void *got, const void *want,
		  size_t n, long input);

#define diff_eq_obj(what, type, got, want, input) \
	diff_eq_obj_(__FILE__, __LINE__, (what), #type, (got), (want), \
		     sizeof(type), (long)(input))

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
 * call site firing (finding F149).
 */
unsigned dsplib_debug_capture_lines(int side);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_TEST_HARNESS_H */
