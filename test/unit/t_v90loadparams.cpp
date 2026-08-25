/*
 * t_v90loadparams.cpp -- the call SEQUENCE of both `loadParams` members,
 * ours against the blob's, in order.
 *
 * WHY A SEQUENCE AND NOT A STATE COMPARISON.  `V90Parameters::loadParams` is
 * 7,894 bytes containing 295 calls to `Vparser_read_int`/`Vparser_read_float`
 * and nothing else -- no branch, no store, no local -- and both callees are
 * `xor %eax,%eax; ret` in the shipped object.  So a test that ran both
 * members over a poisoned object and compared the bytes would pass on an
 * EMPTY function, on a function with the 295 calls in the wrong order, and on
 * one that named every field wrongly.  That is the "result indistinguishable
 * from success" this tree keeps finding in its own checks, and it is why the
 * member was left unwritten until now (finding F879).
 *
 * HOW BOTH SIDES ARE OBSERVED, AND WHY THE TWO MECHANISMS DIFFER.  The two
 * halves of this are not symmetric and the asymmetry is a measurement:
 *
 *   - OUR `loadParams` calls `Vparser_read_int` ACROSS a translation unit, so
 *     the reference is undefined at link time and `ld --wrap` catches it.
 *     The flags are in `test/unit/t_v90loadparams.ldflags`, which the
 *     Makefile and `tools/toolchain/period_inner.sh` both read.
 *   - THE BLOB'S `ref__ZN13V90Parameters10loadParamsEPc` calls
 *     `ref_Vparser_read_int` from INSIDE THE SAME OBJECT FILE, so the
 *     reference is not undefined and `--wrap` does not fire.  ld's manual
 *     says so and it was measured before this test was written: a
 *     two-function object linked `--wrap=foo` calls the real `foo`, and the
 *     same object with `foo` weakened takes a strong definition from
 *     elsewhere.  The `$(REF)` recipe therefore weakens both `ref_Vparser_*`
 *     stubs and the strong definitions below win.  Finding F6400, which is
 *     also where 879's recommendation of `--wrap` for BOTH sides is retired.
 *
 * WHY THIS IS NOT `vparse.py` COMPARED WITH A COPY OF ITSELF.  Finding F879
 * declined this oracle on that ground and it was too strong a reading.  Our
 * third argument is `&this->FIELD` -- a member name that the COMPILER turns
 * into a displacement through `include/dsplib/V90Parameters.h` -- so the
 * offset reaches this log by a path the static walk is not on, and a header
 * whose layout disagreed with the object would show up here as a wrong
 * offset.  The ORDER of the 295 calls, the PAIRING of each name with each
 * offset, and the CHOICE of reader are checked against the blob's own
 * execution and against nothing else in this tree.
 *
 * WHAT IS CHECKED AGAINST A LITERAL RATHER THAN AGAINST THE BLOB.  The two
 * call counts, 295 and 54.  A detector must report its denominator: if the
 * weakening or the `--wrap` silently stopped working, BOTH logs would be
 * empty and every ordered comparison over them would pass.  gates.md rule 1.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V92Parameters.h"

/* The object's own counts.  Finding F861, and `tools/vparse.py` re-derives
 * them from the blob at every `make params`. */
#define V90_CALLS	295
#define V92_CALLS	54

#define LOG_MAX		512

enum { READ_INT = 1, READ_FLOAT = 2 };

struct entry {
	int		reader;
	const char	*name;
	long		offset;		/* from the object's base */
};

static struct entry log_[LOG_MAX];
static int log_n;
static int log_overflow;
static const unsigned char *log_base;

static void
log_reset(const void *base)
{
	log_n = 0;
	log_overflow = 0;
	log_base = (const unsigned char *)base;
}

static void
log_add(int reader, const char *name, const void *target)
{
	if (log_n >= LOG_MAX) {
		log_overflow = 1;
		return;
	}
	log_[log_n].reader = reader;
	log_[log_n].name = name;
	log_[log_n].offset = (long)((const unsigned char *)target - log_base);
	log_n++;
}

/* A second buffer, so the blob's log survives while ours is being made. */
static struct entry saved[LOG_MAX];
static int saved_n;

static void
log_save(void)
{
	memcpy(saved, log_, sizeof saved);
	saved_n = log_n;
}

extern "C" {

/*
 * OUR side, through `--wrap`.  `__real_*` is the reconstruction's own stub in
 * src/core/Vparser.c, so it still executes and its return value is still the
 * one this member sees -- the wrapper observes, it does not replace.
 */
int __real_Vparser_read_int(char *paramFile, const char *name, int *value);
int __real_Vparser_read_float(char *paramFile, const char *name, float *value);

int
__wrap_Vparser_read_int(char *paramFile, const char *name, int *value)
{
	log_add(READ_INT, name, value);
	return __real_Vparser_read_int(paramFile, name, value);
}

int
__wrap_Vparser_read_float(char *paramFile, const char *name, float *value)
{
	log_add(READ_FLOAT, name, value);
	return __real_Vparser_read_float(paramFile, name, value);
}

/*
 * THE BLOB's side, by strong definition over the weakened `ref_` stubs.
 * There is no `__real_` here and there cannot be: the blob's own three-byte
 * copies are what these displace.  Returning 0 is what they returned --
 * `t_vparser` is the binary that proves that, by linking them untouched.
 */
int
ref_Vparser_read_int(char *paramFile, const char *name, int *value)
{
	(void)paramFile;
	log_add(READ_INT, name, value);
	return 0;
}

int
ref_Vparser_read_float(char *paramFile, const char *name, float *value)
{
	(void)paramFile;
	log_add(READ_FLOAT, name, value);
	return 0;
}

void ref_v90_loadParams(void *self, char *f)
	asm("ref__ZN13V90Parameters10loadParamsEPc");
void ref_v92_loadParams(void *self, char *f)
	asm("ref__ZN13V92Parameters10loadParamsEPc");

/*
 * `init()` TOO, and not for completeness.  The object's `init()` guards the
 * call on `modemParams->paramFile`, and because the callee writes nothing an
 * `init()` that skipped the call entirely would leave the object identical --
 * so no state comparison anywhere in this tree can see the CALL SITE.  Driving
 * `init()` through the same log turns "it makes the call" into a measurement:
 * the log holds 295 entries if the branch was taken and none if it was not.
 * `setToDefault` and `loadModemParamsData` reach no `Vparser` function, so
 * they contribute nothing to the log and this stays a test about the call.
 */
void ref_v90_init(void *self) asm("ref__ZN13V90Parameters4initEv");
void ref_v92_init(void *self) asm("ref__ZN13V92Parameters4initEv");

extern unsigned int ref_dsplibs_debug_level;

}

/* Room past each object, to catch a store that overruns it. */
#define GUARD	64

static unsigned char ours_raw[0x558 + GUARD] __attribute__((aligned(16)));
static unsigned char theirs_raw[0x558 + GUARD] __attribute__((aligned(16)));

static char param_file[] = "v90.par";

static void
poison(unsigned bytes)
{
	unsigned i;

	memset(ours_raw, 0, sizeof ours_raw);
	memset(theirs_raw, 0, sizeof theirs_raw);
	for (i = 0; i + 4 <= bytes; i += 4) {
		unsigned w = 0xa5000000u | i;

		memcpy(ours_raw + i, &w, 4);
		memcpy(theirs_raw + i, &w, 4);
	}
	for (i = bytes; i < bytes + GUARD; i++)
		ours_raw[i] = theirs_raw[i] = (unsigned char)(0x5a + (i & 7));
}

static int
unchanged(const unsigned char *p, unsigned bytes)
{
	unsigned i;

	for (i = 0; i + 4 <= bytes; i += 4) {
		unsigned w, want = 0xa5000000u | i;

		memcpy(&w, p + i, 4);
		if (w != want)
			return 0;
	}
	for (i = bytes; i < bytes + GUARD; i++)
		if (p[i] != (unsigned char)(0x5a + (i & 7)))
			return 0;
	return 1;
}

/*
 * Compare the two logs entry by entry, in order.  A SET comparison would pass
 * a transposition, which is the failure a 295-line generated body actually
 * has; this compares position for position and stops reporting after the
 * harness's line cap, because everything after a divergence is consequence.
 */
static void
compare(const char *what, int calls)
{
	int i;
	long base = (long)(what[1] == '9' ? 90 : 92);

	diff_eq_int("%s: the blob made this many calls", saved_n, calls, base);
	diff_eq_int("%s: we made this many calls", log_n, calls, base);
	diff_eq_int("%s: neither log overflowed", log_overflow, 0, base);

	if (saved_n != log_n)
		return;

	for (i = 0; i < log_n; i++) {
		diff_eq_int("call %ld: reader", log_[i].reader,
			    saved[i].reader, (long)i);
		diff_eq_int("call %ld: offset from this",
			    (int)log_[i].offset, (int)saved[i].offset,
			    (long)i);
		diff_eq_int("call %ld: parameter name",
			    strcmp(log_[i].name, saved[i].name), 0, (long)i);
	}
}

/*
 * ONE shared modem block, for the `init()` arms.  Both sides only read it.
 */
static struct _tagModemParameters mp;

/*
 * Drive `init()` on both sides and compare the logs.  `expect` is how many
 * calls the guard should let through -- every call, or none.
 */
static void
init_arm(long which, int expect)
{
	unsigned bytes = (which == 90) ? 0x558u : 0xdcu;

	poison(bytes);

	log_reset(theirs_raw);
	if (which == 90) {
		((V90Parameters *)(void *)theirs_raw)->modemParams = &mp;
		ref_v90_init(theirs_raw);
	} else {
		((V92Parameters *)(void *)theirs_raw)->modemParams = &mp;
		ref_v92_init(theirs_raw);
	}
	log_save();

	log_reset(ours_raw);
	if (which == 90) {
		((V90Parameters *)(void *)ours_raw)->modemParams = &mp;
		((V90Parameters *)(void *)ours_raw)->init();
	} else {
		((V92Parameters *)(void *)ours_raw)->modemParams = &mp;
		((V92Parameters *)(void *)ours_raw)->init();
	}

	diff_eq_int("init: the blob's guard let this many calls through (%ld)",
		    saved_n, expect, which);
	diff_eq_int("init: ours let this many calls through (%ld)",
		    log_n, expect, which);
	if (saved_n == log_n) {
		int i;

		for (i = 0; i < log_n; i++) {
			diff_eq_int("init call %ld: offset",
				    (int)log_[i].offset,
				    (int)saved[i].offset, (long)i);
			diff_eq_int("init call %ld: name",
				    strcmp(log_[i].name, saved[i].name), 0,
				    (long)i);
		}
	}
}

int
main(void)
{
	diff_begin("v90loadparams");

	/* ---------------------------------------------------------- V.90 */
	poison(0x558);

	log_reset(theirs_raw);
	ref_v90_loadParams(theirs_raw, param_file);
	log_save();

	log_reset(ours_raw);
	((V90Parameters *)(void *)ours_raw)->loadParams(param_file);

	compare("V90", V90_CALLS);

	diff_eq_int("V90: the blob wrote nothing",
		    unchanged(theirs_raw, 0x558), 1, 90);
	diff_eq_int("V90: we wrote nothing",
		    unchanged(ours_raw, 0x558), 1, 90);

	/*
	 * THE FILE POINTER IS PASSED ON UNCHANGED, and both sides are asked
	 * with a null one too -- the object's `init()` guards the call on it
	 * being non-null, so a `loadParams` that dereferenced it would be
	 * reachable only from a caller that never does.  Nothing here reads
	 * it, so a null is as good an input as any and is the one that would
	 * fault if either side did.
	 */
	poison(0x558);
	log_reset(theirs_raw);
	ref_v90_loadParams(theirs_raw, (char *)0);
	log_save();
	log_reset(ours_raw);
	((V90Parameters *)(void *)ours_raw)->loadParams((char *)0);
	compare("V90", V90_CALLS);

	/* ---------------------------------------------------------- V.92 */
	poison(0xdc);

	log_reset(theirs_raw);
	ref_v92_loadParams(theirs_raw, param_file);
	log_save();

	log_reset(ours_raw);
	((V92Parameters *)(void *)ours_raw)->loadParams(param_file);

	compare("V92", V92_CALLS);

	diff_eq_int("V92: the blob wrote nothing",
		    unchanged(theirs_raw, 0xdc), 1, 92);
	diff_eq_int("V92: we wrote nothing",
		    unchanged(ours_raw, 0xdc), 1, 92);

	poison(0xdc);
	log_reset(theirs_raw);
	ref_v92_loadParams(theirs_raw, (char *)0);
	log_save();
	log_reset(ours_raw);
	((V92Parameters *)(void *)ours_raw)->loadParams((char *)0);
	compare("V92", V92_CALLS);

	/* ------------------------------------------- the call site in init() */
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	mp.minRate = 4800;
	mp.maxRate = 33600;
	mp.sessionFlags = 0;
	mp.modeFlags = 0;
	mp.powerReductionTenths = 0;
	mp.connectionType = 0;

	/*
	 * BOTH ARMS.  With `paramFile` set the log must hold every call; with
	 * it null the log must be EMPTY, which is the half that catches an
	 * `init()` that dropped the guard and called unconditionally.
	 */
	mp.paramFile = param_file;
	init_arm(90, V90_CALLS);
	init_arm(92, V92_CALLS);
	mp.paramFile = (char *)0;
	init_arm(90, 0);
	init_arm(92, 0);

	return diff_end();
}
