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

/*
 * The ORDINAL of each live allocation -- 1 for the first sysdep_malloc of the
 * run, 2 for the second, and so on.
 *
 * WHY IT EXISTS.  A test that wants "fir1 holds the FIRST of the two
 * allocations and fir2 the second" was asking it as `fir1 < fir2`, and an
 * address comparison does not answer that question.  Both this tree's
 * allocators recycle LIFO: free(a) then free(b), and the next two mallocs
 * hand back b's chunk and then a's, so the comparison INVERTS on alternate
 * trials.  Measured at 20 non-monotonic pairs in 40 on 2005 static glibc and
 * 20 in 40 on a modern one -- identical behaviour, which is why the check
 * that depended on it passed on one and failed on the other for reasons that
 * had nothing to do with either compiler.
 *
 * The ordinal is what the question was always about, and the allocator is the
 * only thing that knows it.
 */
static unsigned long alloc_ord[HARNESS_ALLOC_SLOTS];
static unsigned long alloc_next_ord;

/*
 * The size each live allocation was ASKED for.
 *
 * `malloc_usable_size` is not that number and cannot be substituted for it.
 * It reports the chunk the allocator happened to serve the request from, so
 * two identical requests differ whenever one was split from the top and the
 * other recycled a larger freed chunk -- glibc hands over a remainder it
 * cannot split rather than wasting it.  A test comparing our block's size
 * against the blob's was reading that, and got 132 against 140 for two
 * allocations that had asked for the same thing.  Finding F1353.
 */
static unsigned alloc_size[HARNESS_ALLOC_SLOTS];
static unsigned alloc_insert_size;

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
			alloc_ord[k] = ++alloc_next_ord;
			alloc_size[k] = alloc_insert_size;
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
			alloc_ord[k] = 0;
			alloc_size[k] = 0;
			return 1;
		}
	}
	return 0;
}

/*
 * Which sysdep_malloc handed this pointer out: 1 for the run's first, 2 for
 * its second, and 0 if the pointer is not live.  See `alloc_ord` above for
 * what this is instead of, and why an address comparison is not it.
 */
unsigned long
harness_alloc_ordinal(const void *p)
{
	unsigned i = alloc_hash((void *)p);
	unsigned n;

	for (n = 0; n < HARNESS_ALLOC_SLOTS; n++) {
		unsigned k = (i + n) % HARNESS_ALLOC_SLOTS;

		if (alloc_slots[k] == p)
			return alloc_ord[k];
	}
	return 0;
}

/*
 * The number of bytes this pointer's sysdep_malloc was ASKED for, or 0 if it
 * is not live.  See `alloc_size` above for why malloc_usable_size is not a
 * substitute.
 */
unsigned
harness_alloc_reqsize(const void *p)
{
	unsigned i = alloc_hash((void *)p);
	unsigned n;

	for (n = 0; n < HARNESS_ALLOC_SLOTS; n++) {
		unsigned k = (i + n) % HARNESS_ALLOC_SLOTS;

		if (alloc_slots[k] == p)
			return alloc_size[k];
	}
	return 0;
}

/*
 * The live set as a list.  See harness.h: a test snapshotting a constructed
 * object's whole heap graph needs the pointers, and this is where they are.
 */
int
harness_alloc_live_set(void **out, int max)
{
	unsigned k;
	int n = 0;

	for (k = 0; k < HARNESS_ALLOC_SLOTS; k++)
		if (alloc_slots[k] != 0) {
			if (n < max)
				out[n] = alloc_slots[k];
			n++;
		}
	return n;
}

void
harness_alloc_reset(void)
{
	memset(alloc_slots, 0, sizeof(alloc_slots));
	memset(alloc_ord, 0, sizeof(alloc_ord));
	memset(alloc_size, 0, sizeof(alloc_size));
	alloc_next_ord = 0;
	memset(&harness_alloc, 0, sizeof(harness_alloc));
}

void *
sysdep_malloc(unsigned int size)
{
	void *p = malloc(size);

	if (p != 0) {
		/*
		 * A fixed non-zero pattern, so that a field a constructor
		 * leaves alone is the same on both sides and is obviously
		 * wrong when something reads it.  Fresh pages are zero, which
		 * is the one value that makes an uninitialised field look
		 * deliberate.
		 */
		memset(p, HARNESS_MALLOC_FILL, size);
		harness_alloc.allocs++;
		harness_alloc.live++;
		harness_alloc.bytes += size;
		alloc_insert_size = size;
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

/*
 * Blob-resolved callees.
 *
 * Reconstructed code may call a blob function that is not reconstructed
 * yet.  dsplibs_ref.o renames every symbol the blob DEFINES to ref_*, so
 * such a call would go undefined at link time; these forwarders hand the
 * reconstruction the blob's own implementation -- which is exactly what the
 * eventual reconstruction must be differentially identical to, so nothing
 * is presumed that the tests do not enforce later.
 *
 * The sharing argument is the SHARED/STATEFUL split at the top of this
 * file: both of these write only through their argument (read from the
 * disassembly -- neither touches a global), so one copy serving both sides
 * carries no state across the comparison.  A stateful callee must NOT be
 * added here; it belongs to the renamed side with a driven ref_ shim.
 *
 * When one of these is reconstructed in src/, this forwarder collides with
 * the new definition at link time and is deleted -- a loud removal.
 */
void ref_MTK_phasor(void *p);
void
MTK_phasor(void *p)
{
	ref_MTK_phasor(p);
}

void ref_FDSP_Kernel_InitObj(void *k);
void
FDSP_Kernel_InitObj(void *k)
{
	ref_FDSP_Kernel_InitObj(k);
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

/*
 * The diagnostic hooks, our side.  slmodemd defines these; the harness has to
 * because the reconstruction imports them exactly as the object does.
 *
 * Zero, like slmodemd's own default, so every gated call site takes the
 * not-logging branch -- which is the configuration a working modem runs in
 * and therefore the one the differential tests should be comparing.  A test
 * that wants to drive a logging path can raise it, but must raise
 * ref_dsplibs_debug_level to match or the two sides will diverge in their
 * control flow for reasons that have nothing to do with the modem.
 */
unsigned int dsplibs_debug_level = 0;


/*
 * ---------------------------------------------------------------------------
 * Debug capture.
 *
 * The diagnostic paths were, until this existed, the one part of the object
 * no test could reach: `dsplibs_debug_level` ships at zero, so every gated
 * call site is dead in both the blob and the reconstruction, and a wrong
 * format string or a wrong argument list there survives indefinitely.  That
 * is not hypothetical -- `updateAlpha` had all three wrong (finding F126).
 *
 * Both sides already have their own printf (symmap prefixes it, because it
 * is a stateful callback), so each can be captured separately and the two
 * transcripts compared like any other output.
 */
#define DBGCAP_SIZE 16384

int dsplib_debug_capture_on;
static char dbgcap[2][DBGCAP_SIZE];
static unsigned dbgcap_len[2];

/*
 * Counted separately from the text, and ONLY by the two printf entry points:
 * the callback markers below also write into the buffer, so "the buffer is
 * non-empty" stopped meaning "something printed" the moment they existed.
 * Every anti-vacuity check in the transcript tests wants the second claim.
 */
static unsigned dbgcap_lines[2];

void
dsplib_debug_capture_reset(void)
{
	dbgcap_len[0] = dbgcap_len[1] = 0;
	dbgcap_lines[0] = dbgcap_lines[1] = 0;
	dbgcap[0][0] = dbgcap[1][0] = '\0';
}

unsigned
dsplib_debug_capture_lines(int side)
{
	return dbgcap_lines[side & 1];
}

const char *
dsplib_debug_capture_text(int side)
{
	return dbgcap[side & 1];
}

static void
dbgcap_add(int side, const char *fmt, va_list ap)
{
	int n;

	if (!dsplib_debug_capture_on)
		return;
	if (dbgcap_len[side] + 1 >= DBGCAP_SIZE)
		return;
	n = vsnprintf(dbgcap[side] + dbgcap_len[side],
		      DBGCAP_SIZE - dbgcap_len[side], fmt, ap);
	if (n > 0) {
		dbgcap_len[side] += (unsigned)n;
		if (dbgcap_len[side] >= DBGCAP_SIZE)
			dbgcap_len[side] = DBGCAP_SIZE - 1;
	}
}

/*
 * Callback markers.
 *
 * Comparing the two transcripts catches a wrong format string and a wrong
 * argument, but NOT where a call site sits relative to anything that does not
 * print.  That gap is not theoretical: `IsPulseDialerReady` prints "hook on"
 * BEFORE its modem_set_param and "hook off" AFTER, and moving either one past
 * the call left every test passing (finding F147).  The order is not decoration
 * -- it is what a host tracing the line sees.
 *
 * So each side's modem_* callback drops a marker into its own transcript.  The
 * text is deliberately unlike a format string, and nothing emits it unless a
 * test turns capture on, so the only cost is to tests that opted in.
 */
static void
dbgcap_note(int side, const char *fmt, ...)
{
	va_list ap;

	if (!dsplib_debug_capture_on)
		return;
	va_start(ap, fmt);
	dbgcap_add(side, fmt, ap);
	va_end(ap);
}

int
dsplibs_debug_printf(const char *fmt, ...)
{
	va_list ap;

	dbgcap_lines[0]++;
	va_start(ap, fmt);
	dbgcap_add(0, fmt, ap);
	va_end(ap);
	return 0;			/* logging only, as on the ref side */
}

int
modem_debug_log_data(void *m, unsigned id, const void *buf, int len)
{
	(void)m; (void)buf;
	dbgcap_note(0, "<< log_data %u, %d bytes >>\n", id, len);
	return 0;
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
	va_list ap;

	dbgcap_lines[1]++;
	va_start(ap, fmt);
	dbgcap_add(1, fmt, ap);
	va_end(ap);
	return 0;			/* harmless: logging only */
}

int
ref_modem_debug_log_data(void *m, unsigned id, const void *buf, int len)
{
	(void)m; (void)buf;
	dbgcap_note(1, "<< log_data %u, %d bytes >>\n", id, len);
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
	dbgcap_note(0, "<< get_param %u >>\n", param);
	return param_get(&harness_param_ours, m, param);
}

long
ref_modem_get_param_impl(void *m, unsigned param)
{
	dbgcap_note(1, "<< get_param %u >>\n", param);
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

/*
 * The routing table.  Empty until a test fills it, and every unrouted handle
 * lands on the one pair of shims that has always been there.
 */
struct modem_shim harness_modem_route_ours[HARNESS_SHIM_ROUTES];
struct modem_shim harness_modem_route_ref[HARNESS_SHIM_ROUTES];
static void *shim_route_m[HARNESS_SHIM_ROUTES];
static int shim_nroutes;

void
harness_modem_route_reset(void)
{
	shim_nroutes = 0;
	memset(shim_route_m, 0, sizeof(shim_route_m));
	memset(harness_modem_route_ours, 0, sizeof(harness_modem_route_ours));
	memset(harness_modem_route_ref, 0, sizeof(harness_modem_route_ref));
}

int
harness_modem_route_add(void *m, const unsigned char *pattern, int len)
{
	int i;

	for (i = 0; i < shim_nroutes; i++)
		if (shim_route_m[i] == m)
			return -1;
	if (shim_nroutes >= HARNESS_SHIM_ROUTES)
		return -1;
	i = shim_nroutes++;
	shim_route_m[i] = m;
	harness_modem_route_ours[i].pattern = pattern;
	harness_modem_route_ours[i].pattern_len = len;
	harness_modem_route_ref[i].pattern = pattern;
	harness_modem_route_ref[i].pattern_len = len;
	return i;
}

static struct modem_shim *
shim_for(int side, void *m)
{
	int i;

	for (i = 0; i < shim_nroutes; i++)
		if (shim_route_m[i] == m)
			return side ? &harness_modem_route_ref[i]
				    : &harness_modem_route_ours[i];
	return side ? &harness_modem_ref : &harness_modem_ours;
}

void
harness_modem_reset(const unsigned char *pattern, int len)
{
	shim_pattern = pattern;
	shim_pattern_len = len;
	memset(&harness_modem_ours, 0, sizeof(harness_modem_ours));
	memset(&harness_modem_ref, 0, sizeof(harness_modem_ref));
	harness_modem_route_reset();
}

static int
shim_get_bits(struct modem_shim *s, unsigned char *buf, int n)
{
	const unsigned char *pat = s->pattern != 0 ? s->pattern : shim_pattern;
	int len = s->pattern != 0 ? s->pattern_len : shim_pattern_len;
	int i;

	if (pat == 0 || len == 0)
		return 0;
	for (i = 0; i < n; i++) {
		buf[i] = pat[s->tx_pos % len];
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
{
	(void)nbits;
	dbgcap_note(0, "<< get_bits %d >>\n", n);
	return shim_get_bits(shim_for(0, m), buf, n);
}

int modem_put_bits(void *m, int nbits, const unsigned char *buf, int n)
{
	(void)nbits;
	dbgcap_note(0, "<< put_bits %d >>\n", n);
	return shim_put_bits(shim_for(0, m), buf, n);
}

long modem_set_param(void *m, unsigned name, int val)
{
	dbgcap_note(0, "<< set_param %u = %d >>\n", name, val);
	return (int)shim_set_param(shim_for(0, m), name, val);
}

int ref_modem_get_bits(void *m, int nbits, unsigned char *buf, int n)
{
	(void)nbits;
	dbgcap_note(1, "<< get_bits %d >>\n", n);
	return shim_get_bits(shim_for(1, m), buf, n);
}

int ref_modem_put_bits(void *m, int nbits, unsigned char *buf, int n)
{
	(void)nbits;
	dbgcap_note(1, "<< put_bits %d >>\n", n);
	return shim_put_bits(shim_for(1, m), buf, n);
}

long ref_modem_get_param(void *m, unsigned param)
{ return ref_modem_get_param_impl(m, param); }

long ref_modem_set_param(void *m, unsigned name, int val)
{
	dbgcap_note(1, "<< set_param %u = %d >>\n", name, val);
	return shim_set_param(shim_for(1, m), name, val);
}

/*
 * The S-registers.  One store for both sides: unlike the parameters, nothing
 * writes them, so there is nothing to keep apart and no log worth having.
 */
static long harness_sreg[HARNESS_SREGS];

void
harness_sreg_reset(void)
{
	unsigned i;

	for (i = 0; i < HARNESS_SREGS; i++)
		harness_sreg[i] = 0;
}

void
harness_sreg_set(unsigned n, long v)
{
	if (n < HARNESS_SREGS)
		harness_sreg[n] = v;
}

long
modem_get_sreg(void *m, unsigned sreg)
{
	(void)m;
	return sreg < HARNESS_SREGS ? harness_sreg[sreg] : 0;
}

long ref_modem_get_sreg(void *m, unsigned sreg)
{ return modem_get_sreg(m, sreg); }

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

static void
reg_remove(struct reg_log *log, int id, void *ops)
{
	if (log->deregistered < HARNESS_MAX_REG) {
		log->dereg_id[log->deregistered] = id;
		log->dereg_ops[log->deregistered] = ops;
	}
	log->deregistered++;
}

int
modem_dp_register(int id, void *op)
{
	return reg_add(&harness_reg_ours, id, op);
}

void
modem_dp_deregister(int id, void *op)
{
	reg_remove(&harness_reg_ours, id, op);
}

int ref_modem_dp_register(int id, void *op)
{ return reg_add(&harness_reg_ref, id, op); }

void ref_modem_dp_deregister(int id, void *op)
{ reg_remove(&harness_reg_ref, id, op); }
