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
	int deregistered;
};

extern struct reg_log harness_reg_ours;
extern struct reg_log harness_reg_ref;
void harness_reg_reset(void);

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

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_TEST_HARNESS_H */
