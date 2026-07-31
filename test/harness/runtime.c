/*
 * runtime.c -- the environment dsplibs.o expects, for test binaries.
 *
 * The original object imports 24 symbols.  tools/symmap.py splits them in two
 * and the split is load-bearing:
 *
 *   SHARED    - stateless runtime (sysdep_*, memcpy, __divdi3 ...).  Both the
 *               reconstruction and the reference call the same ones.  Defined
 *               here, unprefixed.
 *
 *   STATEFUL  - callbacks that hand out or consume data (modem_get_bits,
 *               modem_put_bits, the parameter store, the datapump registry).
 *               These are renamed to ref_* so the reference gets its own copy
 *               and cannot race the reconstruction for the same bits.  If the
 *               two shared a bit source, each would eat the bits the other
 *               should have seen and every downstream comparison would be
 *               quietly wrong while still looking like plausible audio.
 *
 * The ref_* shims below abort rather than return a plausible-looking default.
 * A pure-function test should never reach them, so reaching one means the test
 * is exercising more of the blob than intended and its result cannot be
 * trusted.  Tests that legitimately need these (phase 2 onward) will replace
 * them with a driven implementation.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/sysdep.h"

/* ---------------------------------------------------------------- shared */

/*
 * Allocation tracking.
 *
 * A leak or a double free is invisible to a differential test that compares
 * field values -- both sides can agree perfectly on every byte and still be
 * wrong about who owns what.  dsplibs uses a "pass NULL to allocate" idiom at
 * three nesting levels with an ownership flag at each, so this is exactly the
 * kind of code where that goes wrong, and the only place it can be caught.
 *
 * The live set is a small open-addressed table.  It only has to hold one
 * datapump's worth of allocations, and it reports rather than aborts so a
 * test can assert on the numbers.
 */
#define HARNESS_ALLOC_SLOTS 4096

static void *alloc_slots[HARNESS_ALLOC_SLOTS];

struct alloc_log harness_alloc;

static unsigned
alloc_hash(const void *p)
{
	return (unsigned)((unsigned long)p >> 4) % HARNESS_ALLOC_SLOTS;
}

static void
alloc_insert(void *p)
{
	unsigned i = alloc_hash(p);
	unsigned n;

	for (n = 0; n < HARNESS_ALLOC_SLOTS; n++) {
		unsigned k = (i + n) % HARNESS_ALLOC_SLOTS;

		if (alloc_slots[k] == 0) {
			alloc_slots[k] = p;
			return;
		}
	}
	harness_alloc.overflow++;
}

/* Returns non-zero if `p` was in the live set (and removes it). */
static int
alloc_remove(void *p)
{
	unsigned i = alloc_hash(p);
	unsigned n;

	for (n = 0; n < HARNESS_ALLOC_SLOTS; n++) {
		unsigned k = (i + n) % HARNESS_ALLOC_SLOTS;

		if (alloc_slots[k] == p) {
			alloc_slots[k] = 0;
			return 1;
		}
	}
	return 0;
}

void
harness_alloc_reset(void)
{
	memset(alloc_slots, 0, sizeof(alloc_slots));
	memset(&harness_alloc, 0, sizeof(harness_alloc));
}

void *
sysdep_malloc(unsigned int size)
{
	void *p = malloc(size);

	if (p != 0) {
		harness_alloc.allocs++;
		harness_alloc.live++;
		harness_alloc.bytes += size;
		alloc_insert(p);
	}
	return p;
}

void
sysdep_free(void *mem)
{
	void *ptr = mem;

	if (ptr == 0) {
		harness_alloc.free_null++;
		return;
	}
	if (!alloc_remove(ptr)) {
		/*
		 * Freeing something we never handed out: a double free, or a
		 * pointer that was never allocated.  Count it and DO NOT pass
		 * it on -- letting it reach the real free() would abort the
		 * run before the test could report the number.
		 */
		harness_alloc.bad_free++;
		return;
	}
	harness_alloc.frees++;
	harness_alloc.live--;
	free(ptr);
}

void *
sysdep_memcpy(void *dst, const void *src, size_t n)
{
	return memcpy(dst, src, n);
}

void *
sysdep_memset(void *dst, int c, size_t n)
{
	return memset(dst, c, n);
}

char *
sysdep_strcpy(char *dst, const char *src)
{
	return strcpy(dst, src);
}

char *
sysdep_strcat(char *dst, const char *src)
{
	return strcat(dst, src);
}

unsigned
sysdep_strlen(const char *s)
{
	return (unsigned)strlen(s);
}

int
sysdep_sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsprintf(buf, fmt, ap);
	va_end(ap);
	return ret;
}

int
sysdep_vsnprintf(char *buf, unsigned size, const char *fmt, va_list ap)
{
	return vsnprintf(buf, size, fmt, ap);
}

/* -------------------------------------------------------- reference side */

unsigned int ref_dsplibs_debug_level = 0;

static void
unexpected(const char *who)
{
	fprintf(stderr,
		"harness: reference called %s(), which this test does not "
		"drive.\nThe test is reaching further into dsplibs.o than "
		"intended; its result is not trustworthy.\n", who);
	abort();
}

int
ref_dsplibs_debug_printf(const char *fmt, ...)
{
	(void)fmt;
	return 0;			/* harmless: logging only */
}

int
ref_modem_debug_log_data(void *m, unsigned id, const void *buf, int len)
{
	(void)m; (void)id; (void)buf; (void)len;
	return 0;			/* harmless: logging only */
}

/*
 * Parameter store.
 *
 * Each side gets its own recorder so a test can check not just that the two
 * agreed on the returned value, but that they asked for the same parameter --
 * a module that reads the wrong MDMPRM_* would otherwise pass whenever the
 * store happened to hold matching values.
 *
 * The returned value is derived from the request rather than fixed, so a
 * module that silently ignores its arguments cannot pass by accident.
 */
struct param_log harness_param_ours;
struct param_log harness_param_ref;

/*
 * A test that needs a particular value -- a tolerance, a cycle count -- can
 * override one parameter without disturbing the rest.  Both sides read the
 * same table, so an override cannot make the two disagree; it only moves
 * where in the input space the comparison happens.
 */
#define HARNESS_PARAM_OVERRIDES 32

static struct {
	unsigned	param;
	long		value;
	int		set;
} harness_param_override[HARNESS_PARAM_OVERRIDES];

void
harness_param_set(unsigned param, long value)
{
	int i;

	for (i = 0; i < HARNESS_PARAM_OVERRIDES; i++)
		if (harness_param_override[i].set
		    && harness_param_override[i].param == param) {
			harness_param_override[i].value = value;
			return;
		}
	for (i = 0; i < HARNESS_PARAM_OVERRIDES; i++)
		if (!harness_param_override[i].set) {
			harness_param_override[i].param = param;
			harness_param_override[i].value = value;
			harness_param_override[i].set = 1;
			return;
		}
}

static long
param_value(unsigned param)
{
	int i;

	for (i = 0; i < HARNESS_PARAM_OVERRIDES; i++)
		if (harness_param_override[i].set
		    && harness_param_override[i].param == param)
			return harness_param_override[i].value;

	return 0x5A000000L + (long)param * 7L;
}

static long
param_get(struct param_log *log, void *m, unsigned param)
{
	log->calls++;
	log->last_modem = m;
	log->last_param = param;
	return param_value(param);
}

void
harness_param_reset(void)
{
	memset(&harness_param_ours, 0, sizeof(harness_param_ours));
	memset(&harness_param_ref, 0, sizeof(harness_param_ref));
	memset(harness_param_override, 0, sizeof(harness_param_override));
}

/* Our side calls the unprefixed names; the reference calls ref_*. */
long
modem_get_param(void *m, unsigned param)
{
	return param_get(&harness_param_ours, m, param);
}

long
ref_modem_get_param_impl(void *m, unsigned param)
{
	return param_get(&harness_param_ref, m, param);
}

/*
 * The bit pipe.
 *
 * modem_get_bits hands out bits from one stream and modem_put_bits consumes
 * them, so sharing an implementation between the two sides would have each
 * consuming the other's data and the comparison would be meaningless.  Each
 * side therefore gets its OWN cursor over the SAME scripted pattern, and its
 * own sink -- identical input, independently observed output.
 *
 * modem_set_param is logged rather than acted on: what a test wants to know
 * is that the datapump reported 300 bit/s each way at the right moment.
 */
struct modem_shim harness_modem_ours;
struct modem_shim harness_modem_ref;

static const unsigned char *shim_pattern;
static int shim_pattern_len;

void
harness_modem_reset(const unsigned char *pattern, int len)
{
	shim_pattern = pattern;
	shim_pattern_len = len;
	memset(&harness_modem_ours, 0, sizeof(harness_modem_ours));
	memset(&harness_modem_ref, 0, sizeof(harness_modem_ref));
}

static int
shim_get_bits(struct modem_shim *s, unsigned char *buf, int n)
{
	int i;

	if (shim_pattern == 0 || shim_pattern_len == 0)
		return 0;
	for (i = 0; i < n; i++) {
		buf[i] = shim_pattern[s->tx_pos % shim_pattern_len];
		s->tx_pos++;
	}
	s->gets++;
	return n;
}

static int
shim_put_bits(struct modem_shim *s, const unsigned char *buf, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (s->rx_len < HARNESS_SHIM_BITS)
			s->rx[s->rx_len++] = buf[i];
		else
			s->rx_overflow++;
	}
	s->puts++;
	return n;
}

static long
shim_set_param(struct modem_shim *s, unsigned name, int val)
{
	if (s->nparams < HARNESS_SHIM_PARAMS) {
		s->param_name[s->nparams] = name;
		s->param_value[s->nparams] = val;
		s->nparams++;
	}
	return 0;
}

int modem_get_bits(void *m, int nbits, unsigned char *buf, int n)
{ (void)m; (void)nbits; return shim_get_bits(&harness_modem_ours, buf, n); }

int modem_put_bits(void *m, int nbits, const unsigned char *buf, int n)
{ (void)m; (void)nbits; return shim_put_bits(&harness_modem_ours, buf, n); }

long modem_set_param(void *m, unsigned name, int val)
{ (void)m; return (int)shim_set_param(&harness_modem_ours, name, val); }

int ref_modem_get_bits(void *m, int nbits, unsigned char *buf, int n)
{ (void)m; (void)nbits; return shim_get_bits(&harness_modem_ref, buf, n); }

int ref_modem_put_bits(void *m, int nbits, unsigned char *buf, int n)
{ (void)m; (void)nbits; return shim_put_bits(&harness_modem_ref, buf, n); }

long ref_modem_get_param(void *m, unsigned param)
{ return ref_modem_get_param_impl(m, param); }

long ref_modem_set_param(void *m, unsigned name, int val)
{ (void)m; return shim_set_param(&harness_modem_ref, name, val); }

long ref_modem_get_sreg(void *m, unsigned sreg)
{ (void)m; (void)sreg; unexpected("modem_get_sreg"); return 0; }

int ref_modem_send_to_tty(void *m, const void *buf, int n)
{ (void)m; (void)buf; (void)n; unexpected("modem_send_to_tty"); return 0; }

int ref_modem_recv_from_tty(void *m, void *buf, int n)
{ (void)m; (void)buf; (void)n; unexpected("modem_recv_from_tty"); return 0; }

/*
 * Datapump registry.  Each side records into its own log; see harness.h.
 */
struct reg_log harness_reg_ours;
struct reg_log harness_reg_ref;

void
harness_reg_reset(void)
{
	memset(&harness_reg_ours, 0, sizeof(harness_reg_ours));
	memset(&harness_reg_ref, 0, sizeof(harness_reg_ref));
}

static int
reg_add(struct reg_log *log, int id, void *ops)
{
	if (log->count < HARNESS_MAX_REG) {
		log->id[log->count] = id;
		log->ops[log->count] = ops;
	}
	log->count++;
	return 0;
}

int
modem_dp_register(int id, void *op)
{
	return reg_add(&harness_reg_ours, id, op);
}

void
modem_dp_deregister(int id, void *op)
{
	(void)id; (void)op;
	harness_reg_ours.deregistered++;
}

int ref_modem_dp_register(int id, void *op)
{ return reg_add(&harness_reg_ref, id, op); }

void ref_modem_dp_deregister(int id, void *op)
{ (void)id; (void)op; harness_reg_ref.deregistered++; }
